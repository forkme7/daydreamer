
#include "daydreamer.h"
#include <string.h>

extern search_data_t root_data;

selection_phase_t phase_table[6][8] = {
    { PHASE_BEGIN, PHASE_ROOT, PHASE_END },
    { PHASE_BEGIN, PHASE_TRANS, PHASE_GOOD_TACTICS,
        PHASE_KILLERS, PHASE_QUIET, PHASE_BAD_TACTICS, PHASE_END },
    { PHASE_BEGIN, PHASE_TRANS, PHASE_GOOD_TACTICS,
        PHASE_KILLERS, PHASE_QUIET, PHASE_BAD_TACTICS, PHASE_END },
    { PHASE_BEGIN, PHASE_EVASIONS, PHASE_END },
    { PHASE_BEGIN, PHASE_TRANS, PHASE_QSEARCH, PHASE_END },
    { PHASE_BEGIN, PHASE_TRANS, PHASE_QSEARCH_CH, PHASE_END },
};

// How many moves should be selected by scanning through the score list and
// picking the highest available, as opposed to picking them in order? Note
// that root selection is 0 because the moves are already sorted into the
// correct order.
static const int ordered_move_count[6] = { 0, 256, 16, 16, 4, 4 };

static void generate_moves(move_selector_t* sel);
static void score_moves(move_selector_t* sel);
static void sort_root_moves(move_selector_t* sel);
static int score_tactical_move(position_t* pos, move_t move);
static move_t get_best_move(move_selector_t* sel, int* score);
static void score_tactics(move_selector_t* sel);
static void score_quiet(move_selector_t* sel);

/*
 * Initialize the move selector data structure with the information needed to
 * determine what kind of moves to generate and how to order them.
 */
void init_move_selector(move_selector_t* sel,
        position_t* pos,
        generation_t gen_type,
        search_node_t* search_node,
        move_t hash_move,
        int depth,
        int ply)
{
    sel->pos = pos;
    if (is_check(pos) && gen_type != ROOT_GEN) {
        sel->generator = ESCAPE_GEN;
    } else {
        sel->generator = gen_type;
    }
    sel->phase = phase_table[sel->generator];
    sel->hash_move[0] = hash_move;
    sel->hash_move[1] = NO_MOVE;
    sel->depth = depth;
    sel->moves_so_far = 0;
    sel->ordered_moves = ordered_move_count[gen_type];
    sel->num_killers = 0;
    memset(sel->killers, 0, 5*sizeof(move_t));
    if (search_node) {
        sel->mate_killer = search_node->mate_killer;
        sel->killers[0] = search_node->killers[0];
        if (sel->killers[0]) {
            sel->num_killers++;
            sel->killers[1] = search_node->killers[1];
            if (sel->killers[1]) sel->num_killers++;
        }
        if (ply >= 2) {
            move_t* s2k = (search_node-2)->killers;
            if (s2k[0] != sel->killers[0] && s2k[0] != sel->killers[1]) {
                sel->killers[sel->num_killers] = s2k[0];
                if (sel->killers[sel->num_killers]) {
                    sel->num_killers++;
                    if (s2k[1] != sel->killers[0] &&
                            s2k[1] != sel->killers[1]) {
                        sel->killers[sel->num_killers] = s2k[1];
                    }
                    if (sel->killers[sel->num_killers]) sel->num_killers++;
                }
            }
        }
    }
    sel->killers[sel->num_killers] = NO_MOVE;
    generate_moves(sel);
}

/*
 * Is there only one possible move in the current position? This may need to
 * be changed for phased move generation later.
 */
bool has_single_reply(move_selector_t* sel)
{
    return *sel->phase == PHASE_EVASIONS && sel->moves_end == 1;
}

/*
 * Fill the list of candidate moves and score each move for later selection.
 */
static void generate_moves(move_selector_t* sel)
{
    sel->phase++;
    sel->moves_end = 0;
    sel->current_move_index = 0;
    sel->moves = sel->base_moves;
    sel->scores = sel->base_scores;
    assert(*sel->phase == PHASE_EVASIONS ||
            *sel->phase == PHASE_ROOT ||
            *sel->phase == PHASE_END ||
            !is_check(sel->pos));
    switch (*sel->phase) {
        case PHASE_BEGIN:
            assert(false);
        case PHASE_END:
            return;
        case PHASE_TRANS:
            sel->moves = sel->hash_move;
            sel->moves_end = 1;
            break;
        case PHASE_EVASIONS:
            sel->moves_end = generate_evasions(sel->pos, sel->moves);
            score_moves(sel);
            break;
        case PHASE_ROOT:
            sort_root_moves(sel);
            break;
        case PHASE_GOOD_TACTICS:
            sel->moves_end = generate_pseudo_tactical_moves(
                    sel->pos, sel->moves);
            sel->bad_tactics[0] = NO_MOVE;
            sel->num_bad_tactics = 0;
            score_tactics(sel);
            break;
        case PHASE_BAD_TACTICS:
            sel->moves = sel->bad_tactics;
            sel->scores = sel->bad_tactic_scores;
            sel->moves_end = sel->num_bad_tactics;
            break;
        case PHASE_KILLERS:
            sel->moves = sel->killers;
            sel->moves_end = sel->num_killers;
            break;
        case PHASE_QUIET:
            sel->moves_end = generate_pseudo_quiet_moves(sel->pos, sel->moves);
            score_quiet(sel);
            break;
            /**/
        case PHASE_PV:
        case PHASE_NON_PV:
            assert(false);
            sel->moves_end = generate_pseudo_moves(sel->pos, sel->moves);
            score_moves(sel);
            break;
            /**/
        case PHASE_QSEARCH_CH:
            sel->moves_end = generate_quiescence_moves(
                    sel->pos, sel->moves, true);
            score_moves(sel);
            break;
        case PHASE_QSEARCH:
            sel->moves_end = generate_quiescence_moves(
                    sel->pos, sel->moves, false);
            score_moves(sel);
            break;
    }
    sel->single_reply = sel->generator == ESCAPE_GEN && sel->moves_end == 1;
    assert(sel->moves[sel->moves_end] == NO_MOVE);
    assert(sel->current_move_index == 0);
}

/*
 * Return the next move to be searched. The first n moves are returned in order
 * of their score, and the rest in the order they were generated. n depends on
 * the type of node we're at.
 */
move_t select_move(move_selector_t* sel)
{
    if (*sel->phase == PHASE_END) return NO_MOVE;

    move_t move;
    switch (*sel->phase) {
        case PHASE_TRANS:
            move = sel->hash_move[sel->current_move_index++];
            if (!move || !is_plausible_move_legal(sel->pos, move)) break;
            check_pseudo_move_legality(sel->pos, move);
            sel->moves_so_far++;
            return move;
        case PHASE_KILLERS:
            move = sel->killers[sel->current_move_index++];
            if (!move || move == sel->hash_move[0] ||
                    !is_plausible_move_legal(sel->pos, move)) break;
            sel->moves_so_far++;
            check_pseudo_move_legality(sel->pos, move);
            return move;
        case PHASE_ROOT:
            move = sel->moves[sel->current_move_index++];
            if (!move) break;
            check_pseudo_move_legality(sel->pos, move);
            sel->moves_so_far++;
            return move;
        case PHASE_EVASIONS:
            if (sel->current_move_index >= sel->ordered_moves) {
                move = sel->moves[sel->current_move_index++];
                sel->moves_so_far++;
                return move;
            } else {
                assert(sel->current_move_index <= sel->moves_end);
                move = get_best_move(sel, NULL);
                if (!move) break;
                check_pseudo_move_legality(sel->pos, move);
                sel->moves_so_far++;
                return move;
            }
        case PHASE_BAD_TACTICS:
            // TODO: test sorting by bad move score.
            move = sel->moves[sel->current_move_index++];
            if (!move) break;
            assert(!(move == sel->hash_move[0] ||
                        move == sel->killers[0] ||
                        move == sel->killers[1] ||
                        move == sel->killers[2] ||
                        move == sel->killers[3] ||
                        move == sel->killers[4]));
            sel->moves_so_far++;
            return move;
        case PHASE_GOOD_TACTICS:
            while (true) {
                int best_score;
                move = get_best_move(sel, &best_score);
                if (!move) break;
                if (move == sel->hash_move[0] ||
                        !is_pseudo_move_legal(sel->pos, move)) continue;
                int see = static_exchange_eval(sel->pos, move);
                if (see < 0) {
                    sel->bad_tactic_scores[sel->num_bad_tactics] = see;
                    sel->bad_tactics[sel->num_bad_tactics++] = move;
                    sel->bad_tactics[sel->num_bad_tactics] = NO_MOVE;
                    continue;
                }
                check_pseudo_move_legality(sel->pos, move);
                sel->moves_so_far++;
                return move;
            }
            break;
        case PHASE_QUIET:
            while (true) {
                int best_score;
                move = get_best_move(sel, &best_score);
                if (!move) break;
                if (move == sel->hash_move[0] ||
                        move == sel->killers[0] ||
                        move == sel->killers[1] ||
                        move == sel->killers[2] ||
                        move == sel->killers[3] ||
                        move == sel->killers[4]) continue;
                if (!is_pseudo_move_legal(sel->pos, move)) continue;
                check_pseudo_move_legality(sel->pos, move);
                sel->moves_so_far++;
                return move;
            }
            break;
        /**/
        case PHASE_PV:
        case PHASE_NON_PV:
            assert(false);
        /**/
        case PHASE_QSEARCH:
        case PHASE_QSEARCH_CH:
            while (sel->current_move_index >= sel->ordered_moves) {
                move = sel->moves[sel->current_move_index++];
                if (move == NO_MOVE) break;
                if (move == sel->hash_move[0] ||
                        !is_pseudo_move_legal(sel->pos, move)) {
                    continue;
                }
                sel->moves_so_far++;
                return move;
            }
            if (sel->current_move_index >= sel->ordered_moves) break;

            while (true) {
                assert(sel->current_move_index <= sel->moves_end);
                int best_score;
                move = get_best_move(sel, &best_score);
                if (!move) break;
                if ((sel->generator == Q_CHECK_GEN ||
                     sel->generator == Q_GEN) &&
                    (!get_move_promote(move) ||
                     get_move_promote(move) != QUEEN) &&
                    best_score < MAX_HISTORY) continue;
                if (move == sel->hash_move[0] ||
                        !is_pseudo_move_legal(sel->pos, move)) continue;
                check_pseudo_move_legality(sel->pos, move);
                sel->moves_so_far++;
                return move;
            }
            break;
        default: assert(false);
    }

    assert(*sel->phase != PHASE_END);
    generate_moves(sel);
    return select_move(sel);
}

static move_t get_best_move(move_selector_t* sel, int* score)
{
    int offset = sel->current_move_index;
    move_t move = NO_MOVE;
    int best_score = INT_MIN;
    int index = -1;
    for (int i=offset; sel->moves[i] != NO_MOVE; ++i) {
        assert(i < sel->moves_end);
        if (sel->scores[i] > best_score) {
            best_score = sel->scores[i];
            index = i;
        }
    }
    if (index != -1) {
        move = sel->moves[index];
        best_score = sel->scores[index];
        sel->moves[index] = sel->moves[offset];
        sel->scores[index] = sel->scores[offset];
        sel->moves[offset] = move;
        sel->scores[offset] = best_score;
        sel->current_move_index++;
    }
    if (score) *score = best_score;
    return move;
}

/*
 * Take an unordered list of pseudo-legal moves and score them according
 * to how good we think they'll be. This just identifies a few key classes
 * of moves and applies scores appropriately. Moves are then selected
 * by |select_move|.
 * TODO: separate scoring functions for different phases.
 */
static void score_moves(move_selector_t* sel)
{
    move_t* moves = sel->moves;
    int* scores = sel->scores;

    const int grain = MAX_HISTORY;
    const int hash_score = 1000 * grain;
    const int killer_score = 700 * grain;
    for (int i=0; moves[i] != NO_MOVE; ++i) {
        const move_t move = moves[i];
        int score = 0;
        if (move == sel->hash_move[0]) {
            score = hash_score;
        } else if (move == sel->mate_killer) {
            score = hash_score-1;
        } else if (get_move_capture(move) || get_move_promote(move)) {
            score = score_tactical_move(sel->pos, move);
        } else if (move == sel->killers[0]) {
            score = killer_score;
        } else if (move == sel->killers[1]) {
            score = killer_score-1;
        } else if (move == sel->killers[2]) {
            score = killer_score-2;
        } else if (move == sel->killers[3]) {
            score = killer_score-3;
        } else {
            score = root_data.history.history[history_index(move)];
        }
        scores[i] = score;
    }
}

static void score_tactics(move_selector_t* sel)
{
    for (int i=0; sel->moves[i]; ++i) {
        move_t move = sel->moves[i];
        piece_type_t piece = get_move_piece_type(move);
        piece_type_t promote = get_move_promote(move);
        piece_type_t capture = piece_type(get_move_capture(move));
        int good_tactic_bonus = 0;
        if (promote != NONE && promote != QUEEN) good_tactic_bonus = -1000;
        else if (capture != NONE && piece <= capture) good_tactic_bonus =
            material_value(capture) - material_value(piece);
        sel->scores[i] = 6*capture - piece + good_tactic_bonus;
    }
}

static void score_quiet(move_selector_t* sel)
{
    for (int i=0; sel->moves[i]; ++i) {
        move_t move = sel->moves[i];
        sel->scores[i] = root_data.history.history[history_index(move)];
    }
}

/*
 * Determine a score for a capturing or promoting move.
 */
static int score_tactical_move(position_t* pos, move_t move)
{
    const int grain = MAX_HISTORY;
    const int good_tactic_score = 800 * grain;
    const int bad_tactic_score = -800 * grain;
    bool good_tactic;
    piece_type_t piece = get_move_piece_type(move);
    piece_type_t promote = get_move_promote(move);
    piece_type_t capture = piece_type(get_move_capture(move));
    if (promote != NONE && promote != QUEEN) good_tactic = false;
    else if (capture != NONE && piece <= capture) good_tactic = true;
    else good_tactic = (static_exchange_eval(pos, move) >= 0);
    return 6*capture - piece + 5 +
        (good_tactic ? good_tactic_score : bad_tactic_score);
}

/*
 * Sort moves at the root based on total nodes searched under that move.
 * Since the moves are sorted into position, |sel->scores| is not used to
 * select moves during root move selection.
 */
static void sort_root_moves(move_selector_t* sel)
{
    int i;
    uint64_t scores[256];
    for (i=0; root_data.root_moves[i].move != NO_MOVE; ++i) {
        sel->moves[i] = root_data.root_moves[i].move;
        if (sel->depth <= 2) {
            scores[i] = root_data.root_moves[i].qsearch_score;
        } else if (options.multi_pv > 1) {
            scores[i] = root_data.root_moves[i].score;
        } else {
            scores[i] = root_data.root_moves[i].nodes;
        }
        if (sel->moves[i] == sel->hash_move[0]) scores[i] = UINT64_MAX;
    }
    sel->moves_end = i;
    sel->moves[i] = NO_MOVE;

    for (i=0; sel->moves[i] != NO_MOVE; ++i) {
        move_t move = sel->moves[i];
        uint64_t score = scores[i];
        int j = i-1;
        while (j >= 0 && scores[j] < score) {
            scores[j+1] = scores[j];
            sel->moves[j+1] = sel->moves[j];
            --j;
        }
        scores[j+1] = score;
        sel->moves[j+1] = move;
    }
}

