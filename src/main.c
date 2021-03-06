// \todo Implement callback hook which should be called only once.
//       Will be used to initialize variabels.
//
// \todo Implement closure/contex object to pass variables around without using globals.
//       (Which is what I'm doing now).
//
// \todo Native state support:
// \code
//   engine_state_set_callback(state, callback);
//   ...
//   engine_state_set(MainMenu);
//
//   engine_loop() {
//     for (;;) {
//       callbacks[state](ptr, dt);
//     }
//   }
// \endcode
//
// \todo Implement 'parent' attribute for all widgets.

#include <dz/engine.h>
#include <dz/mesh.h>
#include <dz/net/connection.h>
#include <dz/ui/button.h>
#include <dz/ui/statusbar.h>
#include <dz/ui/window.h>
#include <dz/utf8.h>

#include "collision.h"
#include "field.h"
#include "game_state.h"
#include "tetramino.h"
#include "tetris.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"

/*
typedef struct {
  union {
    char *str;
    int   num;
    bool  val;
    void (*fn)();
  } data;

  char *title;

  enum {
    String,
    Integer,
    Checkbox,
  } type;
} entry_t;

typedef struct {
  rect_t   box;
  entry_t *entries;
} dialog_t;

entry_t entries[] = {
    (entry_t){.type = String, .title = "IP"},
    (entry_t){.type = Integer, .title = "Port"},
};

dialog_t dialog = {
    .box     = {0, 0, 10, 10},
    .entries = entries,
};
*/

engine_t *e;

client_t *client;

button_t *b_singleplayer;
button_t *b_multiplayer;
button_t *b_exit;

game_state *gs_p1;
game_state *gs_p2;

window_t    *w;
statusbar_t *sb;

void
hook_keyboard_singleplayer(void *ptr, kb_event_t ev)
{
  engine_t *e = ptr;

  switch (ev.key) {
  case ' ': game_state_drop_piece(gs_p1); break;
  case KEY_UP: game_state_try_rotate(gs_p1); break;
  case KEY_DOWN: game_state_try_move_down(gs_p1); break;
  case KEY_LEFT: game_state_try_move_left(gs_p1); break;
  case KEY_RIGHT: game_state_try_move_right(gs_p1); break;
  case 'c': game_state_try_swap(gs_p1); break;
  case 'q': e->run = false; break;
  }
}

void
hook_keyboard_multiplayer(void *ptr, kb_event_t ev)
{
  engine_t *e = ptr;

  switch (ev.key) {
  case 'q': e->run = false; break;
  }
}

void
hook_keyboard_mainmenu(void *ptr, kb_event_t ev)
{
  engine_t *e = ptr;

  switch (ev.key) {
  case 'q': e->run = false; break;
  }
}

typedef enum {
  MainMenu,
  Singleplayer,
  Multiplayer,
} state_e;

state_e state = MainMenu;

void
hook_keyboard(void *ptr, kb_event_t ev)
{
  switch (state) {
  case MainMenu: hook_keyboard_mainmenu(ptr, ev); break;
  case Singleplayer: hook_keyboard_singleplayer(ptr, ev); break;
  case Multiplayer: hook_keyboard_singleplayer(ptr, ev); break;
  }
}

void
hook_mouse_mainmenu(void *ptr, mouse_event_t ev)
{
  button_check(b_singleplayer, ev.at);
  button_check(b_multiplayer, ev.at);
  button_check(b_exit, ev.at);
}

void
hook_mouse_singleplayer(void *ptr, mouse_event_t ev)
{
  engine_t *e = ptr;

  statusbar_fupdate(sb, "%d:%d", ev.at.x, ev.at.y);
}

void
hook_mouse(void *ptr, mouse_event_t ev)
{
  switch (state) {
  case MainMenu: hook_mouse_mainmenu(ptr, ev); break;
  case Singleplayer: hook_mouse_singleplayer(ptr, ev); break;
  case Multiplayer: hook_mouse_singleplayer(ptr, ev); break;
  }
}

void
hook_loop_mainmenu(void *ptr, long dt)
{
  static long last = 0;
  engine_t   *e    = ptr;

  button_draw(b_singleplayer, e->screen);
  button_draw(b_multiplayer, e->screen);
  button_draw(b_exit, e->screen);

  window_draw(w, e->screen);
}

void
hook_loop_singleplayer(void *ptr, long dt)
{
  static long last = 0;
  engine_t   *e    = ptr;

  if (gs_p1->game_over) {
    /*
    state = MainMenu;
    free(gs_p1);
    gs_p1 = game_state_new();
    game_state_step_pieces(gs_p1);
    */
    screen_draw_rect(e->screen,
                     (rect_t){
                         .x     = 10,
                         .y     = 10,
                         .w     = 90,
                         .h     = 1,
                         .color = color_white,
                     });
    screen_draw_text(e->screen,
                     (rect_t){
                         .x = 11,
                         .y = 11,
                         .w = 84,
                         .h = 1,
                     },
                     "Game Over (The developer was too lazy to implement this, so just press 'q' to exit).",
                     sizeof("Game Over (The developer was too lazy to implement this, so just press 'q' to exit)."),
                     color_white);
  } else {
    game_state_tick(gs_p1, dt);
    game_state_draw_everything(gs_p1, e);
  }
}

void
hook_multiplayer_loop(void *ptr, long dt)
{
  static long last = 0;
  engine_t   *e    = ptr;

  if (gs_p1->game_over) {
    /* This plus a whole lot more code should do the trick...
    free(gs_p1);
    gs_p1 = game_state_new();
    game_state_step_pieces(gs_p1);
    state = MainMenu;
    */
    tetris_client_sendgameover(client);

    screen_draw_rect(e->screen,
                     (rect_t){
                         .x     = 10,
                         .y     = 10,
                         .w     = 90,
                         .h     = 1,
                         .color = color_white,
                     });
    screen_draw_text(e->screen,
                     (rect_t){
                         .x = 11,
                         .y = 11,
                         .w = 84,
                         .h = 1,
                     },
                     "Game Over (The developer was too lazy to implement this, so just press 'q' to exit).",
                     sizeof("Game Over (The developer was too lazy to implement this, so just press 'q' to exit)."),
                     color_white);
  } else if (tetris_client_isready(client)) {
    if (game_state_tick(gs_p1, dt)) {
      tetris_client_setstate(
          client, gs_p1->field->data, gs_p1->mask->data, gs_p1->holding, gs_p1->next, NEXT_QUEUE_SIZE, gs_p1->score, gs_p1->total_cleared, gs_p1->level);
    }

    game_state_draw_everything(gs_p1, e);

    getstate_t state;
    tetris_client_getstate(client, &state);

    /*fprintf(stderr, "%d %d %d\n", state.level, state.score, state.cleared);*/

    gs_p2->holding = tetraminos[state.holding];

    for (int i = 0; i < NEXT_QUEUE_SIZE; i++) {
      gs_p2->next[i] = tetraminos[state.tetraminos[i] - 1];
    }

    gs_p2->level = state.level;
    gs_p2->score = state.score;
    gs_p2->total_cleared = state.cleared;

    for (int i = 0; i < 150; i++) {
      gs_p2->field->data[i] = state.field[i];
    }

    game_state_draw_dont_compute(gs_p2, e);

  } else {
    screen_draw_rect(e->screen,
                     (rect_t){
                         .x     = 10,
                         .y     = 10,
                         .w     = 28,
                         .h     = 1,
                         .color = color_white,
                     });
    screen_draw_text(e->screen,
                     (rect_t){
                         .x = 11,
                         .y = 11,
                         .w = 27,
                         .h = 1,
                     },
                     "Waiting for other player...",
                     sizeof("Waiting for other player..."),
                     color_white);
  }
}

void
hook_loop(void *ptr, long dt)
{
  switch (state) {
  case MainMenu: hook_loop_mainmenu(ptr, dt); break;
  case Singleplayer: hook_loop_singleplayer(ptr, dt); break;
  case Multiplayer: hook_multiplayer_loop(ptr, dt); break;
  }
}

void
singleplayer_callback(void *_)
{
  state = Singleplayer;
}

int    argc;
char **argv;

void
multiplayer_callback(void *_)
{
  if (argc >= 5) {
    client = client_new(argv[1], atoi(argv[2]));

    tetris_client_auth(client, argv[3], strlen(argv[3]));
    tetris_client_join(client, argv[4], strlen(argv[4]));

    state = Multiplayer;
  } else {
    // TODO implement message boxes
    u8_printf("Missing arguments.\n");
    u8_printf("Usage: xtetris <ip> <port> <username> <room>\n");
    exit(1);
  }
}

void
exit_callback(void *_)
{
  e->run = false;
}

void
menu(screen_t *s)
{
  screen_draw_rect(s, (rect_t){.x = 0, .y = 0, .w = 20, .h = 20});
}

int
main(int argc_, char **argv_)
{
  argc = argc_;
  argv = argv_;

  int i;
  int j;

  /*
  client_t *c = client_new("127.0.0.1", 5000);
  if (!c) {
    client_close(c);
    exit(1);
  }

  int success;

  success = tetris_client_auth(c, argv[1], strlen(argv[1]));
  printf("auth: %d\n", success);
  listp_t *l = tetris_client_listp(c);

  for (int i = 0; i < l->num; i++) {
    printf("|%s,%s|\n", l->players[i].name, l->players[i].room);
  }

  printf("listp: %d\n", success);
  listp_free(l);

  listg_t *lg = tetris_client_listg(c);
  for (int i = 0; i < lg->num; i++) {
    printf("|%s,%s|\n", lg->rooms[i].player_1, lg->rooms[i].player_2);
  }
  listg_free(lg);

  client_close(c);

  exit(0);
  */

  e = engine_new(hook_keyboard, hook_mouse, hook_loop);

  gs_p1 = game_state_new();
  gs_p2 = game_state_new();

  b_singleplayer = button_new(
      (rect_t){
          .x     = 10,
          .y     = 10,
          .w     = 15,
          .h     = 1,
          .color = color_white,
      },
      "Singleplayer",
      singleplayer_callback);

  b_multiplayer = button_new(
      (rect_t){
          .x     = 10,
          .y     = 13,
          .w     = 15,
          .h     = 1,
          .color = color_white,
      },
      "Multiplayer",
      multiplayer_callback);

  b_exit = button_new(
      (rect_t){
          .x     = 10,
          .y     = 16,
          .w     = 15,
          .h     = 1,
          .color = color_white,
      },
      "Exit",
      exit_callback);

  w = window_new(
      (rect_t){
          .x     = 5,
          .y     = 5,
          .w     = 30,
          .h     = 15,
          .color = color_white,
      },
      "XTetris");

  srand(time(NULL));

  game_state_step_pieces(gs_p1);

  sb = statusbar_new("foobar");

  int ret = engine_loop(e);
  engine_free(e);

  statusbar_free(sb);
  return ret;
}
