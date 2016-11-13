#define _POSIX_C_SOURCE 200809L
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <SDL.h>
#include <SDL_keyboard.h>

#define NUM_ROWS 20
#define NUM_COLS 10
#define BLOCK_WIDTH 30
#define BLOCK_HEIGHT 30

Uint8 black[4] = { 0, 0, 0, 255 };
Uint8 white[4] = { 255, 255, 255, 255 };
Uint8 gray[4] = { 200, 200, 200, 255 };
Uint8 red[4] = { 255, 0, 0, 255 };

typedef int piece_tp[4][4];

#define I_PIECE 0
#define J_PIECE 1
#define L_PIECE 2
#define O_PIECE 3
#define S_PIECE 4
#define T_PIECE 5
#define Z_PIECE 6

piece_tp pieces[] = {
        [I_PIECE] = {
                { 0, 0, 0, 0 },
                { 1, 1, 1, 1 },
                { 0, 0, 0, 0 },
                { 0, 0, 0, 0 },
        },

        [J_PIECE] = {
                { 0, 0, 0, 0 },
                { 0, 1, 1, 1 },
                { 0, 1, 0, 0 },
                { 0, 0, 0, 0 },
        },

        [L_PIECE] = {
                { 0, 0, 0, 0 },
                { 1, 1, 1, 0 },
                { 0, 0, 1, 0 },
                { 0, 0, 0, 0 },
        },

        [O_PIECE] = {
                { 0, 0, 0, 0 },
                { 0, 1, 1, 0 },
                { 0, 1, 1, 0 },
                { 0, 0, 0, 0 },
        },

        [S_PIECE] = {
                { 0, 0, 0, 0 },
                { 0, 0, 1, 1 },
                { 0, 1, 1, 0 },
                { 0, 0, 0, 0 },
        },

        [T_PIECE] = {
                { 0, 0, 0, 0 },
                { 0, 1, 0, 0 },
                { 1, 1, 1, 0 },
                { 0, 0, 0, 0 }
        },

        [Z_PIECE] = {
                { 0, 0, 0, 0 },
                { 1, 1, 0, 0 },
                { 0, 1, 1, 0 },
                { 0, 0, 0, 0 },
        },
};

int board[NUM_ROWS * NUM_COLS];
SDL_Window *sdl_window;
SDL_Renderer *sdl_renderer;
int win_y;
int win_x;

int user_quit;
piece_tp piece;
piece_tp nextpiece;
int piece_y;
int piece_x;

struct timespec timer_goal;

int errmsg(const char *fmt, ...)
{
        int r;
        va_list ap;

        va_start(ap, fmt);
        r = vfprintf(stderr, fmt, ap);
        va_end(ap);
        return r;
}

int set_timer_goal(int ms_remain)
{
        int r;

        r = clock_gettime(CLOCK_MONOTONIC, &timer_goal);
        if (r != 0) {
                perror("clock_gettime()");
                return -1;
        }

        timer_goal.tv_nsec += (long) ms_remain * 1000000;
        if (timer_goal.tv_nsec > 1000000000) {
                timer_goal.tv_nsec -= 1000000000;
                timer_goal.tv_sec += 1;
        }

        return 0;
}

int get_timer_goal(int *ms_remain)
{
        int r;
        struct timespec now;

        r = clock_gettime(CLOCK_MONOTONIC, &now);
        if (r != 0) {
                perror("clock_gettime()");
                return -1;
        }

        *ms_remain = (int) ((timer_goal.tv_sec - now.tv_sec) * 1000
                + (timer_goal.tv_nsec - now.tv_nsec) / 1000000);
        return 0;
}

void random_piece(piece_tp out)
{
        int p = rand() % (sizeof pieces / sizeof pieces[0]);
        memcpy(out, pieces[p], sizeof (piece_tp));
}

void piece_rotate_left(piece_tp in, piece_tp out)
{
        int i, j;
        for (i = 0; i < 4; i++)
                for (j = 0; j < 4; j++)
                        out[i][j] = in[j][4-i-1];
}

void piece_rotate_right(piece_tp in, piece_tp out)
{
        int i, j;
        for (i = 0; i < 4; i++)
                for (j = 0; j < 4; j++)
                        out[i][j] = in[4-j-1][i];
}

int board_get(int y, int x)
{
        return board[y * NUM_COLS + x];
}

void board_set(int y, int x, int v)
{
        board[y * NUM_COLS + x] = v;
}

int collides(void)
{
        int i, j;
        for (i = 0; i < 4; i++) {
                for (j = 0; j < 4; j++) {
                        if (piece[i][j]) {
                                if (piece_y + i < 0)
                                        return 1;
                                if (piece_y + i >= NUM_ROWS)
                                        return 1;
                                if (piece_x + j < 0)
                                        return 1;
                                if (piece_x + j >= NUM_COLS)
                                        return 1;
                                if (board_get(piece_y + i, piece_x + j))
                                        return 1;
                        }
                }
        }
        return 0;
}

void fix_piece(void)
{
        int i, j;
        for (i = 0; i < 4; i++) {
                for (j = 0; j < 4; j++) {
                        if (piece[i][j]) {
                                board_set(piece_y + i, piece_x + j, 1);
                        }
                }
        }
}

void kill_rows(void)
{
        int i, j;
        int y;
        for (i = y = NUM_ROWS; i --> 0;) {
                int anyempty = 0;
                for (j = 0; j < NUM_COLS; j++) {
                        if (!board_get(i, j))
                                anyempty = 1;
                }
                if (anyempty) {
                        y--;
                        for (j = 0; j < NUM_COLS; j++)
                                board_set(y, j, board_get(i, j));
                }
        }
        while (y --> 0) {
                for (j = 0; j < NUM_COLS; j++)
                        board_set(y, j, 0);
        }
}

int move_to(int y, int x)
{
        int oldy = piece_y;
        int oldx = piece_x;
        piece_y = y;
        piece_x = x;
        if (collides()) {
                piece_y = oldy;
                piece_x = oldx;
                return 0;
        }
        return 1;
}

int move_down(void)
{
        return move_to(piece_y + 1, piece_x);
}

int move_left(void)
{
        return move_to(piece_y, piece_x - 1);
}

int move_right(void)
{
        return move_to(piece_y, piece_x + 1);
}

int rotate_left(void)
{
        piece_tp oldpiece;

        memcpy(oldpiece, piece, sizeof piece);
        piece_rotate_left(oldpiece, piece);
        if (collides()) {
                memcpy(piece, oldpiece, sizeof piece);
                return 0;
        }
        return 1;
}

int rotate_right(void)
{
        piece_tp oldpiece;
        memcpy(oldpiece, piece, sizeof piece);
        piece_rotate_right(oldpiece, piece);
        if (collides()) {
                memcpy(piece, oldpiece, sizeof piece);
                return 0;
        }
        return 1;
}

void let_fall(void)
{
        while (move_down()) {
        }
}

void tick(void)
{
        if (!move_down()) {
                fix_piece();
                kill_rows();
                memcpy(piece, nextpiece, sizeof (piece_tp));
                random_piece(nextpiece);
                piece_x = NUM_COLS / 2 - 2;
                piece_y = 0;
        }
}

int init_game(void)
{
        int i, j;

        for (i = 0; i < NUM_ROWS; i++)
                for (j = 0; j < NUM_COLS; j++)
                        board_set(i, j, 0);

        random_piece(piece);
        random_piece(nextpiece);
        piece_x = NUM_COLS / 2 - 2;
        piece_y = 0;
        user_quit = 0;
        return 0;
}

void exit_game(void)
{
        return;
}

int set_color(Uint8 *color)
{
        int r;
        r = SDL_SetRenderDrawColor(
                sdl_renderer, color[0], color[1], color[2], color[3]
        );
        if (r != 0) {
                errmsg("%p\n", sdl_renderer);
                errmsg("SDL_SetRenderDrawColor(): %s\n", SDL_GetError());
                return -1;
        }
        return 0;
}

int draw_block(int y, int x, Uint8 *color)
{
        int r;
        SDL_Rect sdl_rect;

        sdl_rect.y = y * BLOCK_HEIGHT;
        sdl_rect.x = x * BLOCK_WIDTH;
        sdl_rect.h = BLOCK_HEIGHT;
        sdl_rect.w = BLOCK_WIDTH;

        r = set_color(color);
        if (r != 0)
                return -1;
        r = SDL_RenderFillRect(sdl_renderer, &sdl_rect);
        if (r != 0) {
                errmsg("SDL_RenderFillRect(): %s\n", SDL_GetError());
                return -1;
        }

        r = set_color(gray);
        if (r != 0)
                return -1;
        r = SDL_RenderDrawRect(sdl_renderer, &sdl_rect);
        if (r != 0) {
                errmsg("SDL_RenderDrawRect(): %s\n", SDL_GetError());
                return -1;
        }


        return 0;
}

int draw_board(void)
{
        int r;
        int i, j;
        for (i = 0; i < NUM_ROWS; i++) {
                for (j = 0; j < NUM_COLS; j++) {
                        r = draw_block(i, j, board_get(i, j) ? red : black);
                        if (r != 0) {
                                return -1;
                        }
                }
        }
        return 0;
}

int draw_piece(int y, int x, piece_tp piece)
{
        int r;
        int i, j;
        for (i = 0; i < 4; i++) {
                for (j = 0; j < 4; j++) {
                        if (piece[i][j]) {
                                r = draw_block(y+i, x+j, red);
                                if (r != 0) {
                                        return -1;
                                }
                        }
                }
        }
        return 0;
}

int draw(void)
{
        int r;

        set_color(white);
        SDL_RenderClear(sdl_renderer);

        r = draw_board();
        if (r != 0)
                return -1;
        r = draw_piece(piece_y, piece_x, piece);
        if (r != 0)
                return -1;

        SDL_RenderPresent(sdl_renderer);
        return 0;
}

int init_graphics(void)
{
        int r = SDL_Init(SDL_INIT_VIDEO);
        if (r != 0) {
                errmsg("Error initializing SDL: %s\n", SDL_GetError());
                return -1;
        }

        sdl_window = SDL_CreateWindow(
                "Tetris",
                SDL_WINDOWPOS_UNDEFINED,
                SDL_WINDOWPOS_UNDEFINED,
                480, 640,
                SDL_WINDOW_OPENGL
        );
        if (sdl_window == NULL) {
                errmsg("Error creating SDL window: %s\n", SDL_GetError());
                SDL_Quit();
                return -1;
        }

        sdl_renderer = SDL_CreateRenderer(sdl_window, -1, 0);
        if (sdl_renderer == NULL) {
                errmsg("Error creating SDL renderer: %s\n", SDL_GetError());
                SDL_Quit();
                return -1;
        }

        return 0;
}

void exit_graphics(void)
{
        SDL_DestroyRenderer(sdl_renderer);
        SDL_DestroyWindow(sdl_window);
        SDL_Quit();
}

int process_event(SDL_Event *event)
{
        if (event->type != SDL_KEYDOWN)
                return 0;

        SDL_KeyboardEvent *key = (SDL_KeyboardEvent *) event;

        switch (key->keysym.sym) {
        case SDLK_LEFT: move_left(); break;
        case SDLK_RIGHT: move_right(); break;
        case SDLK_DOWN: move_down(); break;
        case SDLK_a: rotate_left(); break;
        case SDLK_d: rotate_right(); break;
        case SDLK_SPACE: let_fall(); break;
        case SDLK_q: user_quit = 1; break;
        case SDLK_r: init_game(); break; // XXX
        default: break;
        }
        return 0;
}

int mainloop(void)
{
        int r;
        SDL_Event sdl_event;

        SDL_ClearError();
        while (!user_quit) {
                int ms = 350;
                while (ms > 0) {
                        r = set_timer_goal(ms);
                        if (r != 0)
                                return -1;
                        if (*SDL_GetError() != 0) {
                                errmsg("strange error: %s\n",
                                       SDL_GetError());
                        }
                        r = SDL_WaitEventTimeout(&sdl_event, ms);
                        if (r == 0 && *SDL_GetError() != 0) {
                                errmsg("SDL_WaitEventTimeout(): %s\n",
                                       SDL_GetError());
                                return -1;
                        }

                        if (r == 0)
                                break;

                        r = process_event(&sdl_event);
                        if (r != 0)
                                return -1;
                        r = draw();
                        if (r != 0)
                                return -1;
                        r = get_timer_goal(&ms);
                        if (r != 0)
                                return -1;
                }
                tick();
                r = draw();
                if (r != 0)
                        return -1;
        }
        return 0;
}

int run(void)
{
        int r;

        r = init_graphics();
        if (r != 0)
                return -1;

        r = init_game();
        if (r != 0) {
                exit_graphics();
                return -1;
        }

        r = mainloop();
        if (r != 0) {
                exit_game();
                exit_graphics();
                return -1;
        }

        exit_game();
        exit_graphics();
        return 0;
}

int main(void)
{
        int r = run();

        return r != 0;
}
