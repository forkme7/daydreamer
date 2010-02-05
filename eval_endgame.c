
#include "daydreamer.h"

static void scale_krkp(const position_t* pos, eval_data_t* ed, int scale[2]);
static void scale_krpkr(const position_t* pos, eval_data_t* ed, int scale[2]);
static void scale_knpk(const position_t* pos, eval_data_t* ed, int scale[2]);
static void scale_kpkb(const position_t* pos, eval_data_t* ed, int scale[2]);
static void scale_kbpk(const position_t* pos, eval_data_t* ed, int scale[2]);
static void scale_kpk(const position_t* pos, eval_data_t* ed, int scale[2]);

static int score_win(const position_t* pos, eval_data_t* ed);
static int score_draw(const position_t* pos, eval_data_t* ed);
static int score_kbnk(const position_t* pos, eval_data_t* ed);

eg_scale_fn eg_scale_fns[] = {
    NULL,           //EG_NONE,
    NULL,           //EG_WIN,
    NULL,           //EG_DRAW,
    NULL,           //EG_KQKQ,
    NULL,           //EG_KQKP,
    NULL,           //EG_KRKR,
    NULL,           //EG_KRKB,
    NULL,           //EG_KRKN,
    &scale_krkp,    //EG_KRKP,
    /*&scale_krpkr*/NULL,   //EG_KRPKR,
    NULL,           //EG_KRPPKRP,
    NULL,           //EG_KBBKN,
    NULL,           //EG_KBNK,
    NULL,           //EG_KBPKB,
    NULL,           //EG_KBPKN,
    /*&scale_kpkb*/NULL,    //EG_KPKB,
    NULL,           //EG_KBPPKB,
    &scale_knpk,    //EG_KNPK,
    &scale_kbpk,    //EG_KBPK,
    &scale_kpk,     //EG_KPK,
    NULL,           //EG_LAST
};

eg_score_fn eg_score_fns[] = {
    NULL,           //EG_NONE,
    &score_win,     //EG_WIN,
    &score_draw,    //EG_DRAW,
    NULL,           //EG_KQKQ,
    NULL,           //EG_KQKP,
    NULL,           //EG_KRKR,
    NULL,           //EG_KRKB,
    NULL,           //EG_KRKN,
    NULL,           //EG_KRKP,
    NULL,           //EG_KRPKR,
    NULL,           //EG_KRPPKRP,
    NULL,           //EG_KBBKN,
    &score_kbnk,    //EG_KBNK,
    NULL,           //EG_KBPKB,
    NULL,           //EG_KBPKN,
    NULL,           //EG_KPKB,
    NULL,           //EG_KBPPKB,
    NULL,           //EG_KNPK,
    NULL,           //EG_KBPK,
    NULL,           //EG_KPK,
    NULL,           //EG_LAST
};

bool endgame_score(const position_t* pos, eval_data_t* ed, int* score)
{
    eg_score_fn fn = eg_score_fns[ed->md->eg_type];
    if (fn) {
        *score = fn(pos, ed);
        return true;
    }
    return false;
}

void determine_endgame_scale(const position_t* pos,
        eval_data_t* ed,
        int endgame_scale[2])
{
    endgame_scale[WHITE] = ed->md->scale[WHITE];
    endgame_scale[BLACK] = ed->md->scale[BLACK];
    eg_scale_fn fn = eg_scale_fns[ed->md->eg_type];
    if (fn) fn(pos, ed, endgame_scale);
}

static void scale_krkp(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    assert(pos->num_pieces[strong_side] == 2);
    assert(pos->num_pawns[strong_side] == 0);
    assert(pos->num_pieces[weak_side] == 1);
    assert(pos->num_pawns[weak_side] == 1);

    square_t bp = pos->pawns[weak_side][0];
    square_t wr = pos->pieces[strong_side][1];
    square_t wk = pos->pieces[strong_side][0];
    square_t bk = pos->pieces[weak_side][0];
    square_t prom_sq = square_file(bp);
    int tempo = pos->side_to_move == strong_side ? 1 : 0;

    if (strong_side == BLACK) {
        wr = mirror_rank(wr);
        wk = mirror_rank(wk);
        bk = mirror_rank(bk);
        bp = mirror_rank(bp);
    }

    if ((wk < bp && square_file(wk) == prom_sq) ||
            (distance(wk, prom_sq) + 1 - tempo < distance(bk, prom_sq)) ||
            (distance(bk, bp) - (tempo^1) >= 3 && distance(bk, wr) >= 3)) {
        scale[strong_side] = 16;
        scale[weak_side] = 0;
        return;
    }
    int dist = MAX(1, distance(bk, prom_sq)) + distance(bp, prom_sq);
    if (bk == bp + S) {
        if (prom_sq == A1 || prom_sq == H1) return;
        dist++;
    }
    if (square_file(wr)!=square_file(bp) && square_rank(wr)!=RANK_1) dist--;
    if (!tempo) dist--;
    if (distance(wk, prom_sq) > dist) {
        scale[0] = scale[1] = 0;
    }
}

static void scale_krpkr(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    assert(pos->num_pieces[strong_side] == 2);
    assert(pos->num_pawns[strong_side] == 1);
    assert(pos->num_pieces[weak_side] == 2);
    assert(pos->num_pawns[weak_side] == 0);

    square_t wp = pos->pawns[strong_side][0];
    square_t wk = pos->pieces[strong_side][0];
    square_t wr = pos->pieces[strong_side][1];
    square_t bk = pos->pieces[weak_side][0];
    square_t br = pos->pieces[weak_side][1];

    if (strong_side == BLACK) {
        wr = mirror_rank(wr);
        wk = mirror_rank(wk);
        wp = mirror_rank(wp);
        bk = mirror_rank(bk);
        br = mirror_rank(br);
    }

    square_t wp_file = square_file(wp);
    square_t wp_rank = square_rank(wp);
    square_t br_file = square_file(br);
    square_t prom_sq = wp_file + A8;
    if (bk == prom_sq) {
        if (br_file > wp_file) scale[0] = scale[1] = 0;
    } else if (square_file(bk) == wp_file && square_rank(bk) > wp_rank) {
        scale[0] = scale[1] = 0;
    } else if (wr == prom_sq && wp_rank == RANK_7 && br_file == wp_file &&
            (bk == A7 || bk == B7 || bk == G7 || bk == H7) &&
            ((square_rank(br) <= RANK_3 && distance(wk, wp) > 1) ||
             (distance(wk, wp) > 2))) {
        scale[0] = scale[1] = 0;
    }
}

static void scale_kpkb(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    assert(pos->num_pieces[strong_side] == 1);
    assert(pos->num_pawns[strong_side] == 1);
    assert(pos->num_pieces[weak_side] == 2);
    assert(pos->num_pawns[weak_side] == 0);

    square_t wp = pos->pawns[strong_side][0];
    square_t wk = pos->pieces[strong_side][0];
    square_t bk = pos->pieces[weak_side][0];
    square_t bb = pos->pieces[weak_side][1];
    square_t prom_sq = square_file(wp) + A8;

    if (strong_side == BLACK) {
        wp = mirror_rank(wp);
        wk = mirror_rank(wk);
        bk = mirror_rank(bk);
        bb = mirror_rank(bb);
    }

    for (square_t to = wp+N; to != prom_sq; to += N) {
        if (to == bb) {
            scale[0] = scale[1] = 0;
            return;
        }

        if (possible_attack(bb, to, WB)) {
            int dir = direction(bb, to);
            square_t sq;
            for (sq=bb+dir; sq != to && sq != bk; sq+=dir) {}
            if (sq == to) scale[0] = scale[1] = 0;
            return;
        }
    }
}

static void scale_knpk(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    assert(pos->num_pieces[strong_side] == 2);
    assert(pos->num_pawns[strong_side] == 1);
    assert(pos->num_pieces[strong_side^1] == 0);
    assert(pos->num_pawns[strong_side^1] == 0);

    square_t p = pos->pawns[strong_side][0];
    square_t wk = pos->pieces[strong_side^1][0];
    if (strong_side == BLACK) {
        wk = mirror_rank(wk);
        p = mirror_rank(p);
    }
    if (square_file(p) == FILE_H) {
        wk = mirror_file(wk);
        p = mirror_file(p);
    }
    if (p == A7 && distance(wk, A8) <= 1) {
        scale[0] = scale[1] = 0;
    }
}

static void scale_kbpk(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    assert(pos->num_pieces[strong_side] == 2);
    assert(pos->num_pawns[strong_side] == 1);
    assert(pos->num_pieces[strong_side^1] == 1);
    assert(pos->num_pawns[strong_side^1] == 0);

    file_t pf = square_file(pos->pawns[strong_side][0]);
    color_t bc = square_color(pos->pieces[strong_side][1]);
    if (pf == FILE_H) {
        pf = FILE_A;
        bc ^= 1;
    }

    if (pf == FILE_A &&
            distance(pos->pieces[strong_side^1][0], A8*(strong_side^1)) <= 1 &&
            bc != strong_side) {
        scale[0] = scale[1] = 0;
    }
}

static void scale_kpk(const position_t* pos, eval_data_t* ed, int scale[2])
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    bool sstm = pos->side_to_move == strong_side;
    assert(pos->num_pieces[strong_side] == 1);
    assert(pos->num_pawns[strong_side] == 1);
    assert(pos->num_pieces[weak_side] == 1);
    assert(pos->num_pawns[weak_side] == 0);

    square_t sk, wk, p;
    p = pos->pawns[strong_side][0];
    if (square_file(p) < FILE_E) {
        sk = pos->pieces[strong_side][0];
        wk = pos->pieces[weak_side][0];
    } else {
        sk = mirror_file(pos->pieces[strong_side][0]);
        wk = mirror_file(pos->pieces[weak_side][0]);
        p = mirror_file(p);
    }

    square_t push = pawn_push[strong_side];
    rank_t p_rank = relative_rank[strong_side][square_rank(p)];
    bool draw = false;
    if (wk == p + push) {
        if (p_rank <= RANK_6) {
            draw = true;
        } else {
            if (sstm) {
                if (sk == p-push-1 || sk == p-push+1) draw = true;
            } else if (sk != p-push-1 && sk != p-push+1) draw = true;
        }
    } else if (wk == p + 2*push) {
        if (p_rank <= RANK_5) draw = true;
        else {
            assert(p_rank == RANK_6);
            if (!sstm || (sk != p-1 && sk != p+1)) draw = true;
        }
    } else if (sk == p-1 || sk == p+1) {
        if (wk == sk + 2*push && sstm) draw = true;
    } else if (sk >= p+push-1 && sk <= p+push+1) {
        if (p_rank <= RANK_4 && wk == sk + 2*push && sstm) draw = true;
    }

    if (!draw && square_file(p) == FILE_A) {
        if (distance(wk, strong_side == WHITE ? A8 : A1) <= 1) draw = true;
        else if (square_file(sk) == FILE_A &&
                square_file(wk) == FILE_C &&
                    relative_rank[strong_side][square_rank(wk)] >
                    p_rank + (p_rank == RANK_2)) draw = true;
    }

    if (draw) scale[0] = scale[1] = 0;
}
 
static int score_win(const position_t* pos, eval_data_t* ed)
{
    return WON_ENDGAME * (ed->md->strong_side == pos->side_to_move ? 1 : -1);
}

static int score_draw(const position_t* pos, eval_data_t* ed)
{
    (void)pos; (void)ed;
    return DRAW_VALUE;
}

static int score_kbnk(const position_t* pos, eval_data_t* ed)
{
    color_t strong_side = ed->md->strong_side;
    color_t weak_side = strong_side^1;
    assert(pos->num_pieces[strong_side] == 3);
    assert(pos->num_pawns[strong_side] == 0);
    assert(pos->num_pieces[weak_side] == 1);
    assert(pos->num_pawns[weak_side] == 0);

    square_t wk = pos->pieces[strong_side][0];
    square_t wb = pos->pieces[strong_side][1];
    square_t bk = pos->pieces[weak_side][0];
    assert(piece_type(pos->board[wb]) == BISHOP);
    color_t bc = square_color(wb);
    square_t t1 = bc == WHITE ? A8 : A1;
    square_t t2 = bc == WHITE ? H1 : H8;
    int corner_dist = MIN(distance(bk, t1), distance(bk, t2)) +
        MIN(square_rank(bk), square_file(bk));
    return (WON_ENDGAME - 10*corner_dist - distance(wk, bk)) *
        (strong_side == pos->side_to_move ? 1 : -1);
}

