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
const int INITIAL_MAX_WORMLENGTH = 5;

// global enums
enum gamemode {single, local_multi, network_multi};

// global variables -----------------------------------------------------------
int max_x, max_y;
int input_x=0, input_y=1;
int* playground = 0;
player* player1;
food* foodlist = NULL;
bool running = true;
bool paused = false;
bool is_head;
WINDOW *play_window;
WINDOW *score_window;

//-----------------------------------------------------------------------------
class wormpiece {
  public:
    wormpiece(int x, int y);
    wormpiece(player* this_player);
    void draw();
    int pos_x, pos_y;
    wormpiece* connected_to;
};

//-----------------------------------------------------------------------------
class player {
  public:
    player(int num, int x, int y);
    ~player(void);
    void move(void);
    bool collision(void);

    int number;
    int move_x;
    int move_y;
    int wormlength;
    int max_wormlength;
		wormpiece* head;
    int score;
    bool is_alive;
};

wormpiece::wormpiece(int x, int y) {
  pos_x = x;
  pos_y = y;
  connected_to = NULL;
}

wormpiece::wormpiece(player* this_player) {
  pos_x = this_player->head->pos_x + this_player->move_x;
  pos_y = this_player->head->pos_y + this_player->move_y;
  // check if we crossed the playground border
  if(pos_x > max_x) pos_x = pos_x - max_x;
  if(pos_x < 1) pos_x = max_x;
  if(pos_y > max_y) pos_y = pos_y - max_y;
  if(pos_y < 1) pos_y = max_y;
  // attach to old worm
  connected_to = this_player->head;
}

void wormpiece::draw() {
  //TODO draw different integers=colors for each player (e.g. number*10+WORM)
  if(is_head) {
    playground[xy(pos_x, pos_y)] = WORMHEAD;
  }
  else {
    playground[xy(pos_x, pos_y)] = WORM;
  }
}

player::player(int num, int x, int y) {
  this->number = num;
	this->move_x = 1;
	this->move_y = 0;
  this->head = new wormpiece(x, y);
  this->max_wormlength = INITIAL_MAX_WORMLENGTH;
  this->score = 0;
  this->is_alive = true;
}

void player::move() {
  // get movement direction
  this->move_x = input_x;
  this->move_y = input_y;
  // grow a new wormpiece in movement direction and make it the new head
  this->head = new wormpiece(this);
  // put the worm in the playground
  is_head = true; // the head is special for collision detection
  wormpiece* piece = this->head;
  wormlength = 0;
  do {
    piece->draw();
    this->wormlength++;
    if(this->wormlength == this->max_wormlength) {
      delete piece->connected_to;
      piece->connected_to = 0;
    }
    piece = piece->connected_to;
    is_head = false;
  } while (piece);
}

bool player::collision(void) {
  if( (playground[xy(this->head->pos_x, this->head->pos_y)] == WALL) ||
      (playground[xy(this->head->pos_x, this->head->pos_y)] == WORM) ) {
    this->is_alive = false;
    return true;
  }
  else {
    return false;
  }
}

player::~player(void){
  // delete all worm objects
  wormpiece* piece = this->head;
  while(piece) {
    wormpiece* nextpiece = piece->connected_to;
    delete piece;
    piece = nextpiece;
  }
  this->head = NULL;
}

//-----------------------------------------------------------------------------
class food {
  public:
    food(int x, int y);
    ~food();
    void draw();
    unsigned int countdown;
    unsigned int pos_x, pos_y;
    food* next;
    food* prev;
};

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

//-----------------------------------------------------------------------------
class timer {
  private:
    unsigned long begTime;
  public:
    void start() {
      begTime = clock();
    }
    unsigned long elapsedTime() {
      return ((unsigned long) clock() - begTime);
    }
    bool isTimeout(unsigned long seconds) {
      return seconds >= elapsedTime();
    }
};

//-----------------------------------------------------------------------------
void quit(void) {
  endwin();
}

int xy(int x, int y) {
  // the idea here is that the playground array can be a one-dimensional array.
  // xy(3,1) returns 2. (third element in the array)
  // xy(3,4) would return 32 if the playground had 10 columns.
  return max_x*(y-1) + x - 1;
}

void draw_level(int level) {
  // draw borders
  if(level==1 || level ==3) {
    for(int x = 1; x <= max_x; x++) {
      playground[xy(x,1)] = WALL;
    }
    for(int x = 1; x <= max_x; x++) {
      playground[xy(x,max_y)] = WALL;
    }
    for(int y = 1; y <= max_y; y++) {
      playground[xy(1,y)] = WALL;
    }
    for(int y = 1; y <= max_y; y++) {
      playground[xy(max_x,y)] = WALL;
    }
  }
  // draw central block
  if(level==2 || level ==3) {
    for(int j = max_y*3/7 +1; j <= max_y * 4/7; j++) {
      for(int i = max_x*2/7 +1; i <= max_x * 5/7; i++) {
        playground[xy(i,j)] = WALL;
      }
    }
  }
}

void clean_up(void) {
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
  unsigned int gamespeed = 200;
  unsigned int highscore = 0;
  timer t;
  t.start();
  paused = false;
  chrono::milliseconds ms100(100);

  // game loop
  while(running) {
    // TODO show a menu to choose single or (local/nw) multiplayer
    //
    // choose one of four different levels
    int level = (rand() % 4);
    if(level==0 || level ==2) wbkgd(play_window, COLOR_PAIR(8));
    else wbkgd(play_window, COLOR_PAIR(9));
    // create first wormpiece
    srand(time(0));
    player1 = new player(1, 3, 3);
    int points_per_food = 10;
    chrono::milliseconds dura(gamespeed);

    // TODO check here if all players died .. stuff
    while(player1->is_alive && running) {
      // are we in pause mode?
      while(paused && running) {
        this_thread::sleep_for(ms100);
      }

      // clear the hole window
      wclear(play_window);
      // clear the playground
      for(int p = 0; p < (max_x*max_y); p++) {
          playground[p] = 0;
      }

      // move the player(s)
      player1->move();

      // put the food in the playground. remove old food. detect collision with wormhead.
      food* foodpiece = foodlist;
      while(foodpiece) {
        food* nextpiece = foodpiece->next;
        if (foodpiece->countdown > 0) {
          // worm eating food?
          if(foodpiece->pos_x == player1->head->pos_x && foodpiece->pos_y == player1->head->pos_y) {
            player1->max_wormlength += 5;
            player1->score += points_per_food;
            if(player1->max_wormlength<50) {
              dura = dura * 9/10;
              points_per_food *= 2;
            }
            delete foodpiece;
          }
          else { // not eaten yet? => count down!
            foodpiece->countdown--;
            foodpiece->draw();
          }
        }
        else { // countdown is over => delete from list!
          delete foodpiece;
        }
        foodpiece = nextpiece;
      }

      // put borders in the playground
      draw_level(level);

      // detect collisions
      player1->collision();

      // draw the playground in the window
      for(int y = 1; y <= max_y; y++) {
        for(int x = 1; x <= max_x; x++) {
          switch(playground[xy(x, y)]) {
            case WALL:
              wcolor_set(play_window, WALL, 0);
              if(level==2) wcolor_set(play_window, 9, 0);
              mvwaddstr(play_window, y-1, (x-1)*2, "  ");
              break;
            case WORMHEAD:
              wcolor_set(play_window, WORMHEAD, 0);
              //if(move_x==1) mvwaddstr(play_window, y-1, (x-1)*2, ": ");
              //if(move_x==-1) mvwaddstr(play_window, y-1, (x-1)*2, " :");
              //if(move_y==1) mvwaddstr(play_window, y-1, (x-1)*2, "..");
              //if(move_y==-1) mvwaddstr(play_window, y-1, (x-1)*2, "..");
              mvwaddstr(play_window, y-1, (x-1)*2, "  ");
              break;
            case WORM:
              wcolor_set(play_window, WORM, 0);
              mvwaddstr(play_window, y-1, (x-1)*2, "  ");
              break;
            case FOOD:
              wcolor_set(play_window, FOOD, 0);
              mvwaddstr(play_window, y-1, (x-1)*2, "  ");
              break;
          }
        }
      }

      // refresh the window. until now nothing was updated.
      wrefresh(play_window);

      // refresh score window
      if(player1->score > highscore) {highscore = player1->score;}
      mvwprintw(score_window, 0, 1, "Score: %010d\n", player1->score);
      mvwprintw(score_window, 1, 1, "Best : %010d\n", highscore);
      wrefresh(score_window);

      // randomly create new food for the next iteration
      for(int i=0; i<3; i++) {
        if(!(rand() % 50)) {
          int rand_x = (rand() % max_x);
          int rand_y = (rand() % max_y);
          if(playground[xy(rand_x, rand_y)] != WORM && playground[xy(rand_x, rand_y)] != WALL) {
            new food(rand_x, rand_y);
          }
        }
      }

      // clean up dynamic memory if worm died
      if(!(player1->is_alive)) {
        clean_up();
      }

      // wait for a while
      this_thread::sleep_for(dura);
    }
  }
  clean_up();
  return;
}

void controlling(void) {
  while(running) {
    switch (getch()) {
      case KEY_UP:
      case 'w':
      case 'W':
        if(!(player1->move_y)) {
        input_x = 0;
        input_y = -1;
        }
        break;
      case KEY_DOWN:
      case 's':
      case 'S':
        if(!player1->move_y) {
        input_x = 0;
        input_y = +1;
        }
        break;
      case KEY_LEFT:
      case 'a':
      case 'A':
        if(!player1->move_x) {
        input_x = -1;
        input_y = 0;
        }
        break;
      case KEY_RIGHT:
      case 'd':
      case 'D':
        if(!player1->move_x) {
        input_x = +1;
        input_y = 0;
        }
        break;
      case 'p':
      case 'P':
        paused = !paused;
        break;
      case 'q':
      case 'Q':
        running = false;
        break;
    }
  }
  return;
}

//-----------------------------------------------------------------------------
int main(void) {
  initscr();
  atexit(quit);
  noecho();
  curs_set(0);
  cbreak();
  keypad(stdscr, TRUE);
  start_color();
  clear();

  // set color pairs: id, chars_color, back_color
  init_pair(WALL, COLOR_BLUE, COLOR_BLACK);
  init_pair(WORMHEAD, COLOR_BLACK, COLOR_YELLOW);
  init_pair(WORM, COLOR_CYAN, COLOR_YELLOW);
  init_pair(FOOD, COLOR_WHITE, COLOR_BLUE);
  init_pair(8, COLOR_WHITE, COLOR_BLACK);
  init_pair(9, COLOR_BLACK, COLOR_WHITE);
  // set background of main window
  bkgd(COLOR_PAIR(9));
  color_set(1, 0);
  refresh();

  // create new window for playground
  getmaxyx(stdscr, max_y, max_x);
  play_window = newwin(max_y-10, max_x-10, 5, 5);
  wbkgd(play_window, COLOR_PAIR(9));
  // create new window for score
  score_window = newwin(2, max_x-10, 1, 1);
  wbkgd(score_window, COLOR_PAIR(9));
  wattrset(score_window, A_BOLD);

  // save playground size
  getmaxyx(play_window, max_y, max_x);
  max_x = max_x/2; // a piece of worm, food or wall is 2 chars wide (but only 1 char high)

  // create and save playground
  playground = new int [max_x * max_y];

  // lauch the actual game routines
  thread timeThread (timing);
  thread controlThread (controlling);   // bar,0 spawns new thread that calls bar(0)
  // wait for threads to finish ...TODO only one of these lines should be needed
  controlThread.join();
  timeThread.join();

  // do last clean up ... maybe better in quit()
  delwin(play_window);
  delwin(score_window);
  endwin();
  delete[] playground;
  playground = NULL;
  return 0;
}

