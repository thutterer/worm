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
class wormpiece;
class food;

// global constants -----------------------------------------------------------
const int WALL = 1;
const int WORMHEAD = 2;
const int WORM = 3;
const int FOOD = 4;
const int INITIAL_MAX_WORMLENGTH = 5;

// global variables -----------------------------------------------------------
int max_x, max_y;
int move_x=0, move_y=1;
int input_x=0, input_y=1;
int* playground = 0;
wormpiece* wormhead = NULL;
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
    wormpiece(wormpiece* piece);
    void draw();
    int pos_x, pos_y;
    wormpiece* connected_to;
};

wormpiece::wormpiece(int x, int y) {
  pos_x = x;
  pos_y = y;
  connected_to = NULL;
}

wormpiece::wormpiece(wormpiece* piece) {
  // allow crossing borders
  pos_x = piece->pos_x + move_x;
  if(pos_x > max_x) pos_x = pos_x - max_x;
  if(pos_x < 1) pos_x = max_x;
  pos_y = piece->pos_y + move_y;
  if(pos_y > max_y) pos_y = pos_y - max_y;
  if(pos_y < 1) pos_y = max_y;
  // attach to old worm
  connected_to = piece;
}

void wormpiece::draw() {
  if(is_head) {
    playground[xy(pos_x, pos_y)] = WORMHEAD;
  }
  else {
    playground[xy(pos_x, pos_y)] = WORM;
  }
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
  // delete all worm objects
  wormpiece* piece = wormhead;
  while(piece) {
    wormpiece* nextpiece = piece->connected_to;
    delete piece;
    piece = nextpiece;
  }
  wormhead = NULL;
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
  unsigned long seconds = 10;
  unsigned int gamespeed = 200;
  unsigned int wormlength = 1;
  unsigned int max_wormlength;
  unsigned int highscore = 0;
  bool is_dead;
  timer t;
  t.start();
  paused = false;
  chrono::milliseconds ms100(100);

  while(running) {
    // choose one of four different levels
    int level = (rand() % 4);
    if(level==0 || level ==2) wbkgd(play_window, COLOR_PAIR(8));
    else wbkgd(play_window, COLOR_PAIR(9));
    // create first wormpiece
    srand(time(0));
    wormhead = new wormpiece(3, 3);
    is_dead = false;
    int score = 0;
    int points_per_food = 10;
    chrono::milliseconds dura(gamespeed);
    max_wormlength = INITIAL_MAX_WORMLENGTH;
    input_x = 1;
    input_y = 0;

    // game loop
    while(!is_dead && running) {
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

      // get movement direction
      move_x = input_x;
      move_y = input_y;

      // grow a new wormpiece in movement direction and make it the new wormhead
      wormpiece* new_piece = new wormpiece(wormhead);
      wormhead = new_piece;

      // put the worm in the playground
      is_head = true; // the head is special for collision detection
      wormpiece* piece = wormhead;
      wormlength = 0;
      do {
        piece->draw();
        wormlength++;
        if(wormlength == max_wormlength) {
          delete piece->connected_to;
          piece->connected_to = 0;
        }
        piece = piece->connected_to;
        is_head = false;
      } while (piece);

      // put the food in the playground. remove old food. detect collision with wormhead.
      food* foodpiece = foodlist;
      while(foodpiece) {
        food* nextpiece = foodpiece->next;
        if (foodpiece->countdown > 0) {
          // worm eating food?
          if(foodpiece->pos_x == wormhead->pos_x && foodpiece->pos_y == wormhead->pos_y) {
            max_wormlength += 5;
            dura = dura * 9/10;
            score += points_per_food;
            points_per_food *= 2; 
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
      if( (playground[xy(wormhead->pos_x, wormhead->pos_y)] == WALL) || 
          (playground[xy(wormhead->pos_x, wormhead->pos_y)] == WORM) ) {
        is_dead = true;
      }
      
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
              if(move_x==1) mvwaddstr(play_window, y-1, (x-1)*2, ": ");
              if(move_x==-1) mvwaddstr(play_window, y-1, (x-1)*2, " :");
              if(move_y==1) mvwaddstr(play_window, y-1, (x-1)*2, "..");
              if(move_y==-1) mvwaddstr(play_window, y-1, (x-1)*2, "..");
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
      if(score > highscore) {highscore = score;}
      mvwprintw(score_window, 0, 1, "Score: %010d\n", score);
      mvwprintw(score_window, 1, 1, "Best : %010d\n", highscore);
      wrefresh(score_window);

      // randomly create new food for the next iteration
      for(int i=0; i<3; i++) {
        if(!(rand() % 50)) {
          int rand_x = (rand() % max_x);
          int rand_y = (rand() % max_y);
          if(playground[xy(rand_x, rand_y)] != WORM && playground[xy(rand_x, rand_y)] != WALL) {
            food * new_food = new food(rand_x, rand_y);
          }
        }
      }

      // clean up dynamic memory if worm died
      if(is_dead) {
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
        if(!move_y) {
        input_x = 0;
        input_y = -1;
        }
        break;
      case KEY_DOWN:
      case 's':
      case 'S':
        if(!move_y) {
        input_x = 0;
        input_y = +1;
        }
        break;
      case KEY_LEFT:
      case 'a':
      case 'A':
        if(!move_x) {
        input_x = -1;
        input_y = 0;
        }
        break;
      case KEY_RIGHT:
      case 'd':
      case 'D':
        if(!move_x) {
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
  max_y = max_y;
  max_x = max_x/2; // a piece of worm, food or wall is 2 chars wide (but only 1 char high)

  // create and save playground
  playground = new int [max_x * max_y];

  // lauch the actual game routines
  thread timeThread (timing);
  thread controlThread (controlling);  // bar,0 spawns new thread that calls bar(0)
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

