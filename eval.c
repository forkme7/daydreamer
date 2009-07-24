
#include "daydreamer.h"

/* 
 * UFO simple evaluation function, as described at:
 * http://chessprogramming.wikispaces.com/Simplified+evaluation+function
 * Thanks to its originator, Tomasz Michniewski.
 */

#define PAWN_VAL      100
#define KNIGHT_VAL    320
#define BISHOP_VAL    330
#define ROOK_VAL      500
#define QUEEN_VAL     900
#define KING_VAL      20000

const int material_values[] = {
    0, PAWN_VAL, KNIGHT_VAL, BISHOP_VAL, ROOK_VAL, QUEEN_VAL, KING_VAL, 0,
    0, PAWN_VAL, KNIGHT_VAL, BISHOP_VAL, ROOK_VAL, QUEEN_VAL, KING_VAL, 0, 0
};

int piece_square_values[BK+1][0x80] = {
    {}, {}, {}, {}, {}, {}, {}, {}, {}, // empties to get indexing right
    { // pawn
     0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,
    50, 50, 50, 50, 50, 50, 50, 50, 0, 0, 0, 0, 0, 0, 0, 0,
    10, 10, 20, 30, 30, 20, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0,
     5,  5, 10, 25, 25, 10,  5,  5, 0, 0, 0, 0, 0, 0, 0, 0,
     0,  0,  0, 20, 20,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0,
     5, -5,-10,  0,  0,-10, -5,  5, 0, 0, 0, 0, 0, 0, 0, 0,
     5, 10, 10,-20,-20, 10, 10,  5, 0, 0, 0, 0, 0, 0, 0, 0,
     0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0 },

    { // knight
    -50,-40,-30,-30,-30,-30,-40,-50,  0,  0,  0,  0,  0,  0,  0,  0,
    -40,-20,  0,  0,  0,  0,-20,-40,  0,  0,  0,  0,  0,  0,  0,  0,
    -30,  0, 10, 15, 15, 10,  0,-30,  0,  0,  0,  0,  0,  0,  0,  0,
    -30,  5, 15, 20, 20, 15,  5,-30,  0,  0,  0,  0,  0,  0,  0,  0,
    -30,  0, 15, 20, 20, 15,  0,-30,  0,  0,  0,  0,  0,  0,  0,  0,
    -30,  5, 10, 15, 15, 10,  5,-30,  0,  0,  0,  0,  0,  0,  0,  0,
    -40,-20,  0,  5,  5,  0,-20,-40,  0,  0,  0,  0,  0,  0,  0,  0,
    -50,-40,-30,-30,-30,-30,-40,-50,  0,  0,  0,  0,  0,  0,  0,  0 },

    { // bishop
    -20,-10,-10,-10,-10,-10,-10,-20,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  0,  0,  0,  0,  0,  0,-10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  0,  5, 10, 10,  5,  0,-10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  5,  5, 10, 10,  5,  5,-10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  0, 10, 10, 10, 10,  0,-10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10, 10, 10, 10, 10, 10, 10,-10,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,  5,  0,  0,  0,  0,  5,-10,  0,  0,  0,  0,  0,  0,  0,  0,
    -20,-10,-10,-10,-10,-10,-10,-20,  0,  0,  0,  0,  0,  0,  0,  0 },
        
    { // rook
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,  0,  0,  0,  0,  0,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,  0,  0,  0,  0,  0,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,  0,  0,  0,  0,  0,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,  0,  0,  0,  0,  0,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,  0,  0,  0,  0,  0,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  5,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
        
    { // queen
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,  0,  0,  0,  0,  0,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,  0,  0,  0,  0,  0,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,  0,  0,  0,  0,  0,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,  0,  0,  0,  0,  0,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,  0,  0,  0,  0,  0,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  5,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },

    { // king
    -30,-40,-40,-50,-50,-40,-40,-30,  0,  0,  0,  0,  0,  0,  0,  0,
    -30,-40,-40,-50,-50,-40,-40,-30,  0,  0,  0,  0,  0,  0,  0,  0,
    -30,-40,-40,-50,-50,-40,-40,-30,  0,  0,  0,  0,  0,  0,  0,  0,
    -30,-40,-40,-50,-50,-40,-40,-30,  0,  0,  0,  0,  0,  0,  0,  0,
    -20,-30,-30,-40,-40,-30,-30,-20,  0,  0,  0,  0,  0,  0,  0,  0,
    -10,-20,-20,-20,-20,-20,-20,-10,  0,  0,  0,  0,  0,  0,  0,  0,
     20, 20,  0,  0,  0,  0, 20, 20,  0,  0,  0,  0,  0,  0,  0,  0,
     20, 30, 10,  0,  0, 10, 30, 20,  0,  0,  0,  0,  0,  0,  0,  0 }
        
    // mirror piece tables are filled in during init_eval
};


int king_endgame_values[2][0x80] = {
    {},
    { // king endgame, not used yet.
    -50,-40,-30,-20,-20,-30,-40,-50, 0, 0, 0, 0, 0, 0, 0, 0,
    -30,-20,-10,  0,  0,-10,-20,-30, 0, 0, 0, 0, 0, 0, 0, 0,
    -30,-10, 20, 30, 30, 20,-10,-30, 0, 0, 0, 0, 0, 0, 0, 0,
    -30,-10, 30, 40, 40, 30,-10,-30, 0, 0, 0, 0, 0, 0, 0, 0,
    -30,-10, 30, 40, 40, 30,-10,-30, 0, 0, 0, 0, 0, 0, 0, 0,
    -30,-10, 20, 30, 30, 20,-10,-30, 0, 0, 0, 0, 0, 0, 0, 0,
    -30,-30,  0,  0,  0,  0,-30,-30, 0, 0, 0, 0, 0, 0, 0, 0,
    -50,-30,-30,-30,-30,-30,-30,-50, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/*
 * Initialize all static evaluation data structures.
 */
void init_eval(void)
{
    for (piece_t piece=WP; piece<=WK; ++piece) {
        for (square_t square=A1; square<=H8; ++square) {
            if (!valid_board_index(square)) continue;
            piece_square_values[piece][square] =
                piece_square_values[piece+BP-1][flip_square(square)];
        }
    }
}

/*
 * Perform a simple position evaluation based just on material and piece
 * square bonuses.
 */
int simple_eval(const position_t* pos)
{
    color_t side = pos->side_to_move;
    return pos->material_eval[side] - pos->material_eval[side^1] +
        pos->piece_square_eval[side] - pos->piece_square_eval[side^1];
}

bool insufficient_material(const position_t* pos)
{
    return (pos->piece_count[WHITE][PAWN] == 0 &&
        pos->piece_count[BLACK][PAWN] == 0 &&
        pos->material_eval[WHITE] < ROOK_VAL &&
        pos->material_eval[BLACK] < ROOK_VAL);
}

bool is_draw(const position_t* pos)
{
    return pos->fifty_move_counter >= 100 ||
        insufficient_material(pos) ||
        is_repetition(pos, 3);
}

