/* vim: set tabstop=2:softtabstop=2:shiftwidth=2:expandtab */

/*
The MIT License (MIT)

Copyright (c) 2014 Thomas Hutterer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <curses.h>
#include <stdlib.h>
#include <time.h>
#include <thread>
#include <chrono>

#include "network.h"

using namespace std;

// prototypes -----------------------------------------------------------------
int xy(int x, int y);
class player;
class wormpiece;
class food;

// global constants -----------------------------------------------------------
const int WALL = 1;
const int WORMHEAD = 2;
const int WORM = 3;
const int FOOD = 4;
const int INITIAL_MAX_WORMLENGTH = 3;

// global enums
enum gamemodes {not_set, single, local_multi, network_host, network_client};
enum gamestates {starting, running, stopping, stopped};

// global variables -----------------------------------------------------------
int max_x, max_y;
int play_x, play_y;
int* playground = NULL;
player* player1 = NULL;
player* player2 = NULL;
food* foodlist = NULL;
bool no_quit_signal = true;
bool paused;
bool is_head;
bool in_menu;
//bool in_input;
WINDOW* play_window = NULL;
WINDOW* score_window = NULL;
WINDOW* menu_window = NULL;
WINDOW* input_window = NULL;
gamemodes gamemode;
gamestates gamestate;
network* nw_serv = NULL;
network* nw_client = NULL;
char ip_hostname[20];
char nw_port[5];

// classes --------------------------------------------------------------------
class wormpiece {
  public:
    wormpiece(int x, int y);
    wormpiece(player* this_player);

    int pos_x, pos_y;
    wormpiece* connected_to;
};

class player {
  public:
    player(int num);
    ~player(void);
    void move(void);
    bool collision(void);
    bool eats_food(food* this_food);

    int number;
    int move_x;
    int move_y;
    int wormlength;
    int input_x;
    int input_y;
    int max_wormlength;
		wormpiece* head;
    int score;
    int highscore;
    bool is_alive;
};

class food {
  public:
    food(int x, int y);
    ~food();
    void draw();
    int length();
    int countdown;
    int pos_x, pos_y;
    food* next;
    food* prev;
};

// class functions ------------------------------------------------------------
wormpiece::wormpiece(int x, int y) {
  pos_x = x;
  pos_y = y;
  connected_to = NULL;
}

wormpiece::wormpiece(player* this_player) {
  pos_x = this_player->head->pos_x + this_player->move_x;
  pos_y = this_player->head->pos_y + this_player->move_y;
  // check if we crossed the playground border
  if(pos_x > play_x) pos_x = pos_x - play_x;
  if(pos_x < 1) pos_x = play_x;
  if(pos_y > play_y) pos_y = pos_y - play_y;
  if(pos_y < 1) pos_y = play_y;
  // attach to old worm
  connected_to = this_player->head;
}

player::player(int num) {
  this->number = num;
  if(this->number == 1) {
	  this->input_x = 1;
	  this->input_y = 0;
    this->head = new wormpiece(3, 3);
  }
  else if(this->number == 2) {
	  this->input_x = -1;
	  this->input_y = 0;
    this->head = new wormpiece(play_x-2, play_y-3);
  }
  this->max_wormlength = INITIAL_MAX_WORMLENGTH;
  this->score = 0;
  this->is_alive = true;
}

void player::move() {
  // get movement direction
  this->move_x = this->input_x;
  this->move_y = this->input_y;
  // grow a new wormpiece in movement direction and make it the new head
  this->head = new wormpiece(this);
  // put the worm in the playground
  wormpiece* piece = this->head;
  wormlength = 0;

  while(piece) {
    if(piece == this->head) {
      // prevent player2 head overwriting player1 for a sane collision check
      if(playground[xy(piece->pos_x, piece->pos_y)] == 0) {
        playground[xy(piece->pos_x, piece->pos_y)] = WORMHEAD + this->number*10;
      }
    }
    else {
      playground[xy(piece->pos_x, piece->pos_y)] = WORM + this->number*10;
    }
    this->wormlength++;
    if(this->wormlength == this->max_wormlength) {
      if(piece->connected_to) delete piece->connected_to;
      piece->connected_to = 0;
    }
    piece = piece->connected_to;
  }
}

bool player::collision(void) {
  if( (playground[xy(this->head->pos_x, this->head->pos_y)] == WALL) ||
      (playground[xy(this->head->pos_x, this->head->pos_y)] % 10 == WORM) ) {
    this->is_alive = false;
    return true;
  }
  else {
    return false;
  }
}

bool player::eats_food(food* this_food) {
  if(this_food->pos_x == this->head->pos_x && this_food->pos_y == this->head->pos_y) {
    this->max_wormlength += 5;
    this->score += this->wormlength*5;
    delete this_food;
    return true;
  }
  else {return false;}
}

player::~player(void){
  // delete all wormpiece objects
  wormpiece* piece = this->head;
  while(piece) {
    wormpiece* nextpiece = piece->connected_to;
    delete piece;
    piece = nextpiece;
  }
  this->head = NULL;
}


food::food(int x, int y) {
  countdown = 100; // ticks, not seconds
  pos_x = x;
  pos_y = y;
  next = foodlist;
  prev = NULL;
  if(foodlist) foodlist->prev = this;
  foodlist = this;
}

food::~food() {
  if(this->prev) {this->prev->next = this->next;}
  else {foodlist = this->next;}
  if(this->next) {this->next->prev = this->prev;}
}

void food::draw() {
  playground[xy(pos_x, pos_y)] = FOOD;
}

int food::length() {
  food* that = this;
  int count = 0;
  while(that) {
    count++;
    that = that->next;
  }
  return count;
}

// functions ------------------------------------------------------------------
void input_box(char msg[20], char* result) {
  delwin(input_window);
  input_window = newwin(4, 24, max_y/2-2, max_x/2-10);
  wbkgd(input_window, COLOR_PAIR(10));
  wattrset(input_window, A_BOLD);
  wcolor_set(input_window, 10, 0);
  wclear(input_window);
  wborder(input_window, 0, 0, 0, 0, 0, 0, 0, 0);
  mvwaddstr(input_window, 1,2, msg);
  wrefresh(input_window);
  echo();
  wcolor_set(input_window, 11, 0);
  mvwaddstr(input_window, 2,2, "                    ");
  //TODO use getchar and check each time if the user typed to many chars
  do {
    mvwgetstr(input_window, 2,2, result);
  } while(result[0]=='\0');
  noecho();
}

void quit(void) {
  endwin();
}

int xy(int x, int y) {
  // the idea here is that the playground array can be a one-dimensional array.
  // xy(3,1) returns 2. (third element in the array)
  // xy(3,4) would return 32 if the playground had 10 columns.
  return play_x*(y-1) + x - 1;
}

bool in_multiplayer(void) {
  return (gamemode==local_multi || gamemode==network_host || gamemode==network_client);
}

void draw_level(int level) {
  // draw borders
  if(level==1 || level ==3) {
    for(int x = 1; x <= play_x; x++) {
      playground[xy(x,1)] = WALL;
    }
    for(int x = 1; x <= play_x; x++) {
      playground[xy(x,play_y)] = WALL;
    }
    for(int y = 1; y <= play_y; y++) {
      playground[xy(1,y)] = WALL;
    }
    for(int y = 1; y <= play_y; y++) {
      playground[xy(play_x,y)] = WALL;
    }
  }
  // draw central block
  if(level==2 || level ==3) {
    for(int j = play_y*3/7 +1; j <= play_y * 4/7; j++) {
      for(int i = play_x*2/7 +1; i <= play_x * 5/7; i++) {
        playground[xy(i,j)] = WALL;
      }
    }
  }
}

void clear_foodlist(void) {
  // delete all food objects
  food* foodpiece = foodlist;
  while(foodpiece) {
    food* nextpiece = foodpiece->next;
    delete foodpiece;
    foodpiece = nextpiece;
  }
  foodlist = NULL;
}

void timing(void) {
  int gamespeed = 200;
  int level;
  chrono::milliseconds ms100(100);
  srand(time(0));

  getmaxyx(stdscr, max_y, max_x);
  paused = true;
  in_menu = true;
  gamestate = stopped;

  // game loop
  while(no_quit_signal) {
    // measure time taken to substract it from gamedelay
    clock_t timer_start, timer_end;
    double cpu_time_used;
    timer_start = clock();

    // clear the main window
    if(!paused) {
      clear();
      refresh();
    }
    // is this the beginning of a new round?
    if(gamestate==starting) {

      getmaxyx(stdscr, max_y, max_x);

      // clear all old food objects
      clear_foodlist();

      // configure network if needed
      if(nw_serv) delete nw_serv; nw_serv = NULL;
      if(nw_client) delete nw_client; nw_client = NULL;
      if(gamemode==network_host)
        nw_serv = new server(nw_port);
      else if(gamemode==network_client)
        nw_client = new client(nw_port, ip_hostname);

      // negotiate the size of the playground between server and client
      if(gamemode==network_client) {
        nw_client->send_int(max_x);
        nw_client->send_int(max_y);
        nw_client->receive_int(max_x);
        nw_client->receive_int(max_y);
      }
      else if(gamemode==network_host) {
        int clients_max_x, clients_max_y;
        nw_serv->receive_int(clients_max_x);
        nw_serv->receive_int(clients_max_y);
        if(clients_max_x > max_x) 
          clients_max_x = max_x;
        else
          max_x = clients_max_x;
        if(clients_max_y > max_y)
          clients_max_y = max_y;
        else
          max_y = clients_max_y;
        nw_serv->send_int(clients_max_x);
        nw_serv->send_int(clients_max_y);
      }

      // (re)create game-window and -array
      delwin(play_window);
      if(playground) {delete[] playground; playground = NULL;}
      play_x = (max_x-10)/2;
      play_y = max_y-10;
      play_window = newwin(play_y, play_x*2, 5, 5);
      playground = new int [play_x * play_y];

      // choose one of four different levels (not on client)
      if(gamemode!=network_client) level = (rand() % 4);
      // tell the client which level we play in
      if(gamemode==network_host) nw_serv->send_int(level);
      if(gamemode==network_client) nw_client->receive_int(level);
      // => here is were the game halts when the other side isn't ready yet

      // choose colors depending on level
      if(level==0 || level ==2) wbkgd(play_window, COLOR_PAIR(8));
      else wbkgd(play_window, COLOR_PAIR(9));

      // (re)create new window for score
      delwin(score_window);
      score_window = newwin(3, play_x*2, 1, 5);
      wbkgd(score_window, COLOR_PAIR(9));
      wattrset(score_window, A_BOLD);

      // remove player object from last round
      if(player1) delete player1; player1 = NULL;
      if(player2) delete player2; player2 = NULL;

      // create players' worms
      player1 = new player(1);
      if(gamemode!=single) player2 = new player(2);

      // now all is ready to have the round running
      gamestate=running;
    }


    // the in-game stuff like moving the players happens in this block
    if(!paused && gamestate==running) {
      // clear the hole window
      wclear(play_window);
      // clear the playground
      for(int p = 0; p < (play_x*play_y); p++) {
          playground[p] = 0;
      }

      // send and receive movement infos via network
      if(gamemode==network_host) {
        nw_serv->send_input(player1->input_x, player1->input_y);
        nw_serv->receive_input(player2->input_x, player2->input_y);
      }
      else if(gamemode==network_client) {
        nw_client->receive_input(player1->input_x, player1->input_y);
        nw_client->send_input(player2->input_x, player2->input_y);
      }

      // move the player(s)
      if(player1->is_alive) player1->move();
      if(player2 && player2->is_alive) player2->move();

      // refresh food objects and check if a player is eating one (not on client side) FIXME: how does p2 grow on clientside???
      if(gamemode!=network_client) {
        food* foodpiece = foodlist;
        while(foodpiece) {
          food* nextpiece = foodpiece->next;
          if (foodpiece->countdown > 0) {
            // worm eating food?
            if( ! (player1->eats_food(foodpiece) || (player2 && player2->eats_food(foodpiece)))) {
              foodpiece->countdown--;
              foodpiece->draw();
            }
          }
          else { // countdown is over
            delete foodpiece;
          }
          foodpiece = nextpiece;
        }
        draw_level(level);
      }

      // sync playground to the client
      if(gamemode==network_host) {
        nw_serv->send_playground(playground, play_x, play_y);
      }
      else if(gamemode==network_client) {
        nw_client->receive_playground(playground, play_x, play_y);
      }

      // detect collisions
      if(player2 && player2->is_alive) player2->collision();
      if(player1->is_alive) player1->collision();
      // if none lives anymore then remember to exit this round
      if(! (player1->is_alive || (player2 && player2->is_alive))) gamestate = stopping;

      // draw the playground in the window
      for(int y = 1; y <= play_y; y++) {
        for(int x = 1; x <= play_x; x++) {
          switch(playground[xy(x, y)]) {
            case WALL:
              wcolor_set(play_window, WALL, 0);
              if(level==2) wcolor_set(play_window, 9, 0);
              mvwaddstr(play_window, y-1, (x-1)*2, "  ");
              break;
            case WORMHEAD+10:
              wcolor_set(play_window, WORMHEAD+10, 0);
              if(player1->move_x==1) mvwaddstr(play_window, y-1, (x-1)*2, ": ");
              if(player1->move_x==-1) mvwaddstr(play_window, y-1, (x-1)*2, " :");
              if(player1->move_y==1) mvwaddstr(play_window, y-1, (x-1)*2, "..");
              if(player1->move_y==-1) mvwaddstr(play_window, y-1, (x-1)*2, "..");
              break;
            case WORMHEAD+20:
              wcolor_set(play_window, WORMHEAD+20, 0);
              if(player2->move_x==1) mvwaddstr(play_window, y-1, (x-1)*2, ": ");
              if(player2->move_x==-1) mvwaddstr(play_window, y-1, (x-1)*2, " :");
              if(player2->move_y==1) mvwaddstr(play_window, y-1, (x-1)*2, "..");
              if(player2->move_y==-1) mvwaddstr(play_window, y-1, (x-1)*2, "..");
              break;
            case WORM+10:
              wcolor_set(play_window, WORM+10, 0);
              mvwaddstr(play_window, y-1, (x-1)*2, "  ");
              break;
            case WORM+20:
              wcolor_set(play_window, WORM+20, 0);
              mvwaddstr(play_window, y-1, (x-1)*2, "  ");
              break;
            case FOOD:
              wcolor_set(play_window, FOOD, 0);
              mvwaddstr(play_window, y-1, (x-1)*2, "  ");
              break;
          }
        }
      }

      // randomly create new food for the next iteration
      if(gamemode != network_client) {
        // never have more than 3 on the screen
        if(!foodlist || foodlist->length() < 3) {
          if(!(rand() % 10)) {
            int rand_x = (rand() % play_x);
            int rand_y = (rand() % play_y);
            if(playground[xy(rand_x, rand_y)] % 10 != WORM && playground[xy(rand_x, rand_y)] != WALL) {
              new food(rand_x, rand_y);
            }
          }
        }
      }

      // refresh the window. until now nothing was updated.
      wrefresh(play_window);

      // sync score to the client
      if(gamemode==network_host) {
        nw_serv->send_int(player1->score);
        nw_serv->send_int(player2->score);
      }
      else if(gamemode==network_client) {
        nw_client->receive_int(player1->score);
        nw_client->receive_int(player2->score);
      }

      // refresh score window
      wclear(score_window);
      if(player1->score > player1->highscore) {player1->highscore = player1->score;}
      mvwprintw(score_window, 0, 1, "PLAYER 1");
      if(!player1->is_alive) mvwprintw(score_window, 0, 10, "DEAD!");
      mvwprintw(score_window, 1, 1, "Score: %010d\n", player1->score);
      mvwprintw(score_window, 2, 1, "Best : %010d\n", player1->highscore);
      if(player2) {
        if(player2->score > player2->highscore) {player2->highscore = player2->score;}
        mvwprintw(score_window, 0, play_x*2 -17, "PLAYER 2");
        if(!player2->is_alive) mvwprintw(score_window, 0, max_x -10, "DEAD!");
        mvwprintw(score_window, 1, play_x*2 -17, "Score: %010d\n", player2->score);
        mvwprintw(score_window, 2, play_x*2 -17, "Best : %010d\n", player2->highscore);
      }
      wrefresh(score_window);
    }

    // refresh menu
    if(in_menu) {
      delwin(menu_window);
      menu_window = newwin(10, 28, max_y/2-5, max_x/2-14);
      wbkgd(menu_window, COLOR_PAIR(10));
      wattrset(menu_window, A_BOLD);
      wclear(menu_window);
      wborder(menu_window, 0, 0, 0, 0, 0, 0, 0, 0);
      mvwprintw(menu_window, 1, 3, "CurseWorm       v.0.8");
      mvwprintw(menu_window, 3, 3, "[1] singleplayer");
      mvwprintw(menu_window, 4, 3, "[2] local multiplayer");
      mvwprintw(menu_window, 5, 3, "[3] host network game"); //"this is a menu : %010d\n", highscore);
      mvwprintw(menu_window, 6, 3, "[4] join network game");
      mvwprintw(menu_window, 7, 3, "[q] quit");
      wrefresh(menu_window);
    }

    // exit to menu if there is no living player
    if(gamestate==stopping) {
      clear_foodlist();
      gamemode = not_set;
      in_menu = true;
      gamestate = stopped;
    }
    else {
    // wait for a while
      timer_end = clock();
      cpu_time_used = ((double) (timer_end - timer_start)) / CLOCKS_PER_SEC * 1000;
      chrono::milliseconds dura(gamespeed-(int)cpu_time_used);
      this_thread::sleep_for(dura);
    }
  }
  clear_foodlist();
  return;
}

void controlling(void) {
  // in singleplayer mode the player(1) can control the worm with WASD and the arrow keys
  // in local multiplayer the player1 contols its worm with WASD and player2 with arrows
  // in network server mode player1 controls its worm with WASD and arrows are disabled
  // in network client mode player2 controls its worm with arrows and WASD is disabled
  while(no_quit_signal) {
    switch (getch()) {
      case KEY_UP:
        if(gamemode==local_multi || gamemode==network_client){
          if(!(player2->move_y)) {
            player2->input_x = 0;
            player2->input_y = -1;
          }
          break;
        }
        if(gamemode==network_host) break;
      case 'w':
      case 'W':
        if(gamemode!=network_client) {
          if(!(player1->move_y)) {
            player1->input_x = 0;
            player1->input_y = -1;
          }
        }
        break;
      case KEY_DOWN:
        if(gamemode==local_multi || gamemode==network_client){
          if(!player2->move_y) {
            player2->input_x = 0;
            player2->input_y = +1;
          }
          break;
        }
        if(gamemode==network_host) break;
      case 's':
      case 'S':
        if(gamemode!=network_client) {
          if(!player1->move_y) {
            player1->input_x = 0;
            player1->input_y = +1;
          }
        }
        break;
      case KEY_LEFT:
        if(gamemode==local_multi || gamemode==network_client) {
          if(!player2->move_x) {
            player2->input_x = -1;
            player2->input_y = 0;
          }
          break;
        }
        if(gamemode==network_host) break;
      case 'a':
      case 'A':
        if(gamemode!=network_client) {
          if(!player1->move_x) {
            player1->input_x = -1;
            player1->input_y = 0;
          }
        }
        break;
      case KEY_RIGHT:
        if(gamemode==local_multi || gamemode==network_client) {
          if(!player2->move_x) {
            player2->input_x = +1;
            player2->input_y = 0;
          }
          break;
        }
        if(gamemode==network_host) break;
      case 'd':
      case 'D':
        if(gamemode!=network_client) {
          if(!player1->move_x) {
            player1->input_x = +1;
            player1->input_y = 0;
          }
        }
        break;
      case 'p':
      case 'P':
        paused = !paused;
        break;
      case 'q':
      case 'Q':
        if(in_menu) no_quit_signal = false;
        break;
      case '1':
        if(in_menu) {
          gamemode = single;
          in_menu = false;
          gamestate = starting;
          paused = false;
        }
        break;
      case '2':
        if(in_menu) {
          gamemode = local_multi;
          in_menu = false;
          gamestate = starting;
          paused = false;
        }
        break;
      case '3':
        if(in_menu) {
          paused = true; //without a pause segfault here...fix
          gamemode = network_host;
          ip_hostname[0] = '\0'; nw_port[0] = '\0';
          in_menu = false;
          input_box("Port:", nw_port);
          gamestate = starting;
          paused = false;
        }
        break;
      case '4':
        if(in_menu) {
          paused = true;
          gamemode = network_client;
          ip_hostname[0] = '\0'; nw_port[0] = '\0';
          in_menu = false;
          input_box("IP or hostname", ip_hostname);
          input_box("Port:", nw_port);
          gamestate = starting;
          paused = false;
        }
        break;
      case 27: //Esc-Key
        in_menu = !in_menu;
        break;
    }
  }
  return;
}

//-----------------------------------------------------------------------------
int main(void) {
  // init curses
  initscr();
  atexit(quit);
  noecho();
  curs_set(0);
  cbreak();
  keypad(stdscr, TRUE);
  start_color();

  // set color pairs: id, chars_color, back_color
  init_pair(WALL, COLOR_BLUE, COLOR_WHITE);
  // player 1 colors
  init_pair(WORMHEAD+10, COLOR_BLACK, COLOR_YELLOW);
  init_pair(WORM+10, COLOR_CYAN, COLOR_YELLOW);
  // player 2 colors
  init_pair(WORMHEAD+20, COLOR_BLACK, COLOR_GREEN);
  init_pair(WORM+20, COLOR_CYAN, COLOR_GREEN);
  init_pair(FOOD, COLOR_WHITE, COLOR_BLUE);
  //level backgrounds
  init_pair(9, COLOR_WHITE, COLOR_BLACK);
  init_pair(8, COLOR_BLACK, COLOR_WHITE);
  //menu colors
  init_pair(10, COLOR_WHITE, COLOR_BLACK);
  init_pair(11, COLOR_BLACK, COLOR_WHITE);

  // set background of main window
  bkgd(COLOR_PAIR(9));
  color_set(1, 0);

  // launch the actual game routines
  thread timeThread (timing);
  thread controlThread (controlling);   // bar,0 spawns new thread that calls bar(0)
  // wait for threads to finish ...TODO only one of these lines should be needed
  controlThread.join();
  timeThread.join();

  // do last clean up ... maybe better in quit()
  delwin(score_window);
  endwin();
  if(playground) {delete[] playground; playground = NULL;}
  return 0;
}

