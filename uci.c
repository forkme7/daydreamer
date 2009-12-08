
#include "daydreamer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern search_data_t root_data;

static void uci_get_input(void);
static void uci_handle_command(char* command);
static void uci_position(char* uci_pos);
static void uci_go(char* command);
static void calculate_search_time(int wtime,
        int btime,
        int winc,
        int binc,
        int movestogo);
static void uci_handle_ext(char* command);
static int bios_key(void);

void uci_read_stream(FILE* stream)
{
    char command[4096];
    while (fgets(command, 4096, stream)) uci_handle_command(command);
}

static void uci_handle_command(char* command)
{
    if (!command) exit(0);

    if (!strncasecmp(command, "isready", 7)) printf("readyok\n");
    else if (!strncasecmp(command, "quit", 4)) exit(0);
    else if (!strncasecmp(command, "stop", 4)) {
        root_data.engine_status = ENGINE_ABORTED;
    } else if (strncasecmp(command, "ponderhit", 9) == 0) {
        root_data.engine_status = ENGINE_THINKING;
    } else if (!strncasecmp(command, "uci", 3)) {
        printf("id name %s %s\n", ENGINE_NAME, ENGINE_VERSION);
        printf("id author %s\n", ENGINE_AUTHOR);
        print_uci_options();
        printf("uciok\n");
    } else if (!strncasecmp(command, "ucinewgame", 10)) {
    } else {
        // strip trailing newline.
        char* c = command;
        while (*c) ++c;
        while (*--c == '\n') *c = '\0';

        if (!strncasecmp(command, "position", 8)) uci_position(command+9);
        else if (!strncasecmp(command, "go", 2)) uci_go(command+3);
        else if (!strncasecmp(command, "setoption name", 14)) {
            set_uci_option(command+15);
        }  else {
            uci_handle_ext(command);
        }
    }
    // TODO: handling for debug
}

/*
 * Parse a uci position command and set the board appropriately.
 */
static void uci_position(char* uci_pos)
{
    while (isspace(*uci_pos)) ++uci_pos;
    if (!strncasecmp(uci_pos, "startpos", 8)) {
        set_position(&root_data.root_pos, FEN_STARTPOS);
        uci_pos += 8;
    } else if (!strncasecmp(uci_pos, "fen", 3)) {
        uci_pos += 3;
        while (*uci_pos && isspace(*uci_pos)) ++uci_pos;
        uci_pos = set_position(&root_data.root_pos, uci_pos);
    }
    while (isspace(*uci_pos)) ++uci_pos;
    if (!strncasecmp(uci_pos, "moves", 5)) {
        uci_pos += 5;
        while (isspace(*uci_pos)) ++uci_pos;
        while (*uci_pos) {
            move_t move = coord_str_to_move(&root_data.root_pos, uci_pos);
            if (move == NO_MOVE) {
                printf("Warning: could not parse %s\n", uci_pos);
                print_board(&root_data.root_pos, true);
                return;
            }
            undo_info_t dummy_undo;
            do_move(&root_data.root_pos, move, &dummy_undo);
            while (*uci_pos && !isspace(*uci_pos)) ++uci_pos;
            while (isspace(*uci_pos)) ++uci_pos;
        }
    }
}

/*
 * Parse the uci go command and start searching.
 */
static void uci_go(char* command)
{
    assert(command);
    char* info;
    int wtime=0, btime=0, winc=0, binc=0, movestogo=0, movetime=0;
    bool ponder = false;

    init_search_data(&root_data);
    if ((info = strcasestr(command, "searchmoves"))) {
        info += 11;
        int move_index=0;
        while (isspace(*info)) ++info;
        while (*info) {
            move_t move = coord_str_to_move(&root_data.root_pos, info);
            if (move == NO_MOVE) {
                break;
            }
            if (!is_move_legal(&root_data.root_pos, move)) {
                printf("%s is not a legal move\n", info);
            }
            init_root_move(&root_data.root_moves[move_index++], move);
            while (*info && !isspace(*info)) ++info;
            while (isspace(*info)) ++info;
        }
    }
    if ((strcasestr(command, "ponder"))) {
        ponder = true;
    }
    if ((info = strcasestr(command, "wtime"))) {
        sscanf(info+5, " %d", &wtime);
    }
    if ((info = strcasestr(command, "btime"))) {
        sscanf(info+5, " %d", &btime);
    }
    if ((info = strcasestr(command, "winc"))) {
        sscanf(info+4, " %d", &winc);
    }
    if ((info = strcasestr(command, "binc"))) {
        sscanf(info+4, " %d", &binc);
    }
    if ((info = strcasestr(command, "movestogo"))) {
        sscanf(info+9, " %d", &movestogo);
    }
    if ((info = strcasestr(command, "depth"))) {
        sscanf(info+5, " %d", &root_data.depth_limit);
    }
    if ((info = strcasestr(command, "nodes"))) {
        sscanf(info+5, " %"PRIu64, &root_data.node_limit);
    }
    if ((info = strcasestr(command, "mate"))) {
        sscanf(info+4, " %d", &root_data.mate_search);
    }
    if ((info = strcasestr(command, "movetime"))) {
        sscanf(info+8, " %d", &movetime);
        root_data.time_target = root_data.time_limit = movetime;
    }
    if ((strcasestr(command, "infinite"))) {
        root_data.infinite = true;
    }

    if (!movetime && !root_data.infinite) {
        calculate_search_time(wtime, btime, winc, binc, movestogo);
    }
    print_board(&root_data.root_pos, true);
    deepening_search(&root_data);
}

/*
 * Given uci time management parameters, determine how long to spend on this
 * move. We compute both a target time (the amount of time we'd like to spend)
 * that can be ignored if the position needs more time (e.g. we just failed
 * high at the root) and a higher time limit that should not be exceeded.
 */
static void calculate_search_time(int wtime,
        int btime,
        int winc,
        int binc,
        int movestogo)
{
    // TODO: cool heuristics for time mangement.
    // For now, just use a simple static rule and look at our own time only
    color_t side = root_data.root_pos.side_to_move;
    int inc = side == WHITE ? winc : binc;
    int time = side == WHITE ? wtime : btime;
    if (!movestogo) {
        // x+y time control
        root_data.time_target = time/40 + inc;
        root_data.time_limit = MAX(time/5, inc-250);
    } else {
        // x/y time control
        root_data.time_target = movestogo == 1 ?
            time/2 :
            time/MIN(movestogo, 20);
        root_data.time_limit = movestogo == 1 ?
            MAX(time-250, time*3/4) :
            MIN(time/4, time*4/movestogo);
    }
    if (get_option_bool("Ponder")) {
        root_data.time_target =
            MIN(root_data.time_limit, root_data.time_target * 5 / 4);
    }
    // TODO: adjust polling interval based on time remaining?
    // this might help for really low time-limit testing.
}

/*
 * Handle any ready uci commands. Called periodically during search.
 */
void uci_check_for_command()
{
    /*
    char input[4096];
    int data = bios_key();
    if (data) {
        if (!fgets(input, 4096, stdin))
            strcpy(input, "quit\n");
        if (!strncasecmp(input, "quit", 4)) {
            root_data.engine_status = ENGINE_ABORTED;
        } else if (!strncasecmp(input, "stop", 4)) {
            root_data.engine_status = ENGINE_ABORTED;
        } else if (strncasecmp(input, "ponderhit", 9) == 0) {
            // TODO: handle ponderhits
        }
    }
    */
    char command[4096];
    if (bios_key()) {
        fgets(command, 4096, stdin);
        uci_handle_command(command);
    }
}

/*
 * Wait for the next uci command. Called when we're done pondering but
 * haven't gotten a ponder hit or miss yet.
 */
void uci_wait_for_command()
{
    char command[4096];
    fgets(command, 4096, stdin);
    uci_handle_command(command);
}

/*
 * Handle non-standard uci extensions. These are diagnostic and debugging
 * commands that print more information about a position or more or execute
 * test suites.
 */
static void uci_handle_ext(char* command)
{
    position_t* pos = &root_data.root_pos;
    if (!strncasecmp(command, "perftsuite", 10)) {
        command+=10;
        while (isspace(*command)) command++;
        perft_testsuite(command);
    } else if (!strncasecmp(command, "perft", 5)) {
        int depth=1;
        sscanf(command+5, " %d", &depth);
        perft(pos, depth, false);
    } else if (!strncasecmp(command, "divide", 6)) {
        int depth=1;
        sscanf(command+6, " %d", &depth);
        perft(pos, depth, true);
    } else if (!strncasecmp(command, "bench", 5)) {
        int depth = 1;
        sscanf(command+5, " %d", &depth);
        benchmark(depth, 0);
    } else if (!strncasecmp(command, "see", 3)) {
        command += 3;
        while (isspace(*command)) command++;
        move_t move = coord_str_to_move(pos, command);
        printf("see: %d\n", static_exchange_eval(pos, move));
    } else if (!strncasecmp(command, "epd", 3)) {
        char filename[256];
        int time_per_move = 5;
        sscanf(command+3, " %s %d", filename, &time_per_move);
        time_per_move *= 1000;
        epd_testsuite(filename, time_per_move);
    } else if (!strncasecmp(command, "print", 5)) {
        print_board(pos, false);
        move_t moves[255];
        printf("moves: ");
        generate_legal_moves(pos, moves);
        for (move_t* move = moves; *move; ++move) {
            char san[8];
            move_to_san_str(pos, *move, san);
            printf("%s ", san);
        }
        printf("\nordered moves: ");
        move_selector_t sel;
        init_move_selector(&sel, pos, PV_GEN, NULL, NO_MOVE, 0, 0);
        for (move_t move = select_move(&sel); move != NO_MOVE;
                move = select_move(&sel)) {
            char san[8];
            move_to_san_str(pos, move, san);
            printf("%s ", san);
        }
        printf("\n");
    }
}

/*
 * Boilerplate code to see if data is available to be read on stdin.
 * Cross-platform for unix/windows.
 *
 * Many thanks to the original author(s). I've seen minor variations on this
 * in Scorpio, Viper, Beowulf, Olithink, and others, so I don't know where
 * it's from originally. I'm just glad I didn't have to figure out how to
 * do this on windows.
 */
#ifndef _WIN32
/* Unix version */
int bios_key(void)
{
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(fileno(stdin), &readfds);
    /* Set to timeout immediately */
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    select(16, &readfds, 0, 0, &timeout);

    return (FD_ISSET(fileno(stdin), &readfds));
}

#else
/* Windows version */
#include <conio.h>
int bios_key(void)
{
    static int init=0, pipe=0;
    static HANDLE inh;
    DWORD dw;
    /*
     * If we're running under XBoard then we can't use _kbhit() as the input
     * commands are sent to us directly over the internal pipe
     */
#if defined(FILE_CNT)
    if (stdin->_cnt > 0) return stdin->_cnt;
#endif
    if (!init) {
        init = 1;
        inh = GetStdHandle(STD_INPUT_HANDLE);
        pipe = !GetConsoleMode(inh, &dw);
        if (!pipe) {
            SetConsoleMode(inh,
                    dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
            FlushConsoleInputBuffer(inh);
        }
    }
    if (pipe) {
        if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) {
            return 1;
        }
        return dw;
    } else {
        GetNumberOfConsoleInputEvents(inh, &dw);
        return dw <= 1 ? 0 : dw;
    }
}
#endif

