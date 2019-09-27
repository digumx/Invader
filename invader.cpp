/*
 * A basic alien invasion game.
 */

/*
 * Frametime target in ms
 */
#define INV_FRAMETIME_MS 10

/*
 * Color pairs
 */
#define INV_CP_PLAYER 1
#define INV_CP_ENEMY 2
#define INV_CP_BULLET 3
#define INV_CP_EXPLOSION 4
#define INV_CP_LINE 5

/*
 * Number of frames to wait for between enemy updates
 */
#define INV_ENEMY_FRAMES 20

/*
 * Initial value of enemy spawn interval
 */
#define INV_INITIAL_INTERV 400

/*
 * number of enemies to skip before reducing interval to INV_REDUCTION_PERCENT%th of original
 */
#define INV_REDUCTION_ENEMIES 12
#define INV_REDUCTION_PERCENT 60

/*
 * Amount bullets move every frame
 */
#define INV_BULLET_SPEED 1

/*
 * Amount player moves every frame
 */
#define INV_PLAYER_SPEED 1

/*
 * Explosion speed / 5.
 */
#define INV_EXPL_TIME 5

/*
 * Number of frames to wait between explosion propagation for GO anim
 */
#define INV_GO_TIME 1

/*
 * Number of frames before a LEVEL UP blink
 */
#define INV_LU_BLINK_FRAMES 200

/*
 * Multiplier applied to spawn margin. Loosen to allow quirky possibilities.
 */
#define INV_SPAWN_MARGIN_MULTIPLIER 3

/*
 * Margin in which enemies do not spawn
 */
#define INV_SPAWN_MARGIN 1




/*
 * use this to turn on lag prints
 */
//#define INV_LAG_PRINT

/*
 * Use this to select which explosion to draw
 */
//#define INV_WIDE_EXPL




#include <chrono>
#include <iostream>
#include <string>
#include <deque>

#include <stdlib.h>

#include <ncurses.h>




/*
 * Definition for sprite node struct
 */
struct Sprite
{
        int row;
        int col;
        Sprite* nxt = NULL;
};

struct Bullet : public Sprite
{
        int typ;
        Bullet* nxt = NULL;
        Bullet(int t = 0) : typ(t) {}
};

struct Explosion
{
        int row;
        int col;
        int tim;
        Explosion() : tim(INV_EXPL_TIME*3) {}
};





/*
 * Player's x_position. 1 - max_x-2.
 */
int posX;

/*
 * Head of enemy sprite chain.
 */
Sprite* enemies = NULL;

/*
 * Should game continue
 */
bool cont = true;

/*
 * Frame counter for process enemies
 */
int frames = INV_ENEMY_FRAMES;

/*
 * Frame counter and interval for spawning enemies
 */
int spawnInterv = INV_INITIAL_INTERV;
int spawnFrames = 0;

/*
 * Number of enemies with current interval
 */
int nenemies = 0;

/*
 * Bullets
 */
Bullet* bullets = NULL;

/*
 * Exoplosions
 */
std::deque<Explosion> explosions;

/*
 * Score
 */
int score = 0;
int lv = 1;
int max_score = 0;

/*
 * Markers for game over animation
 */
int gol, gor;
int f = 0;

/*
 * Game mode is normal or game over
 */
bool gm = true;

/*
 * Are we drawing LEVEL UP? How many times has it blinked? how many frames since last blink?
 */
bool drawLU;
int nBlinkLU;
int fLastLU;

/*
 * Last enemy spawn
 */
int lstEnem = 40;

/*
 * Speed
 */
int spd = 0;




void initNcurses()
{
        initscr();
        cbreak();                   // No character buffering, but allow user to ^C.
        noecho();                   // Do not echo keystrokes.
        keypad(stdscr, TRUE);       // For arrow keys. 
        nodelay(stdscr, TRUE);      // No delay to key processing
        scrollok(stdscr, TRUE);
        if(curs_set(0)==ERR)        // Cursor invisible
        {
                printw("Cursor problem");
                while(getch() == ERR);
        }
        start_color();
        init_pair(INV_CP_PLAYER, COLOR_BLUE, COLOR_BLACK);
        init_pair(INV_CP_ENEMY, COLOR_RED, COLOR_BLACK);
        init_pair(INV_CP_BULLET, COLOR_GREEN, COLOR_BLACK);
        init_pair(INV_CP_EXPLOSION, COLOR_YELLOW, COLOR_BLACK);
        init_pair(INV_CP_LINE, COLOR_CYAN, COLOR_BLACK);
}

void cleanup()
{
        endwin();
}

/*
 * Draw the player
 */
void drawPlayer()
{
        int max_x, max_y;
        getmaxyx(stdscr, max_y, max_x);
        if(posX < 1 || posX > max_x-2) return;          // Don't draw out of boubnds
        int x_p, y_p;
        getyx(stdscr, y_p, x_p);
        attron(COLOR_PAIR(INV_CP_PLAYER));
        mvaddch(max_y-1, posX-1,    '/'  | A_BOLD);
        mvaddch(max_y-1, posX,      '|'  | A_BOLD);
        mvaddch(max_y-1, posX+1,    '\\' | A_BOLD);
        mvaddch(max_y-2, posX,      '|'  | A_BOLD);
        attroff(COLOR_PAIR(INV_CP_PLAYER));
        move(y_p, x_p);
}

/*
 * Draw enemies.
 */
void drawEnemies()
{
        Sprite* si = enemies;
        int y_p, x_p;
        getyx(stdscr, y_p, x_p);
        attron(COLOR_PAIR(INV_CP_ENEMY));
        while(si != NULL)
        {
                mvaddch(si->row, si->col-1, '\\' | A_BOLD);
                mvaddch(si->row, si->col, '/' | A_BOLD);
                si = si->nxt;
        }
        attroff(COLOR_PAIR(INV_CP_ENEMY));
}

void drawBullets()
{

        Bullet* si = bullets;
        int y_p, x_p;
        getyx(stdscr, y_p, x_p);
        attron(COLOR_PAIR(INV_CP_BULLET));
        while(si != NULL)
        {
                chtype ch = si->typ < 0 ? '\\' : (si->typ > 0 ? '/' : '|');
                mvaddch(si->row, si->col, ch | A_BOLD);
                si = si->nxt;
        }
        attroff(COLOR_PAIR(INV_CP_BULLET));
}

void drawExplosions()
{
#ifdef INV_WIDE_EXPL
        int x, y;
        getyx(stdscr, y, x);
        int mx, my;
        getmaxyx(stdscr, y, x);
        for(std::deque<Explosion>::iterator i = explosions.begin(); i != explosions.end(); ++i)
        {
                attron(COLOR_PAIR(INV_CP_EXPLOSION));
                int xx = i->col;
                int yy = i->row;
                if(i->tim >= INV_EXPL_TIME) mvaddch(yy, xx, '*' | A_BOLD);
                for(int p = xx-2; p <= xx+1; p++)
                        for(int q = yy-1; q <= yy+1; q++)
                                if(i->tim < 2*INV_EXPL_TIME)
                                {
                                        if((p == xx || p == xx-1) && q == yy);   
                                        else if(p == xx || p == xx-1)           mvaddch(q, p, '|');
                                        else if(q == yy)                        mvaddch(q, p, '-' | A_BOLD);
                                        else if((p-xx) * (q-yy) >= 0)           mvaddch(q, p, '\\');
                                        else                                    mvaddch(q, p, '/');
                                }
                attroff(COLOR_PAIR(INV_CP_EXPLOSION));

                i->tim--;
        }

        while(!explosions.empty() && explosions.back().tim <= 0) explosions.pop_back();
#endif

#ifndef INV_WIDE_EXPL
        int x, y;
        getyx(stdscr, y, x);
        int mx, my;
        getmaxyx(stdscr, y, x);
        for(std::deque<Explosion>::iterator i = explosions.begin(); i != explosions.end(); ++i)
        {
                attron(COLOR_PAIR(INV_CP_EXPLOSION));

                int xx = i->col;
                int yy = i->row;
                if(i->tim >= INV_EXPL_TIME) mvaddch(yy, xx, '*' | A_BOLD);
                for(int p = xx-1; p <= xx+1; p++)
                        for(int q = yy-1; q <= yy+1; q++)
                                if(i->tim < 2*INV_EXPL_TIME)
                                {
                                        if(p == xx && q == yy); //mvaddch(q, p, '*' | A_BOLD);  
                                        else if(p == xx)        mvaddch(q, p, '|');
                                        else if(q == yy)        mvaddch(q, p, '-' | A_BOLD);
                                        else if(p-xx == q-yy)   mvaddch(q, p, '\\');
                                        else                    mvaddch(q, p, '/');
                                }
                attroff(COLOR_PAIR(INV_CP_EXPLOSION));

                i->tim--;
        }

        while(!explosions.empty() && explosions.back().tim <= 0) explosions.pop_back();
#endif
}

void drawLevelUp()
{
        if(fLastLU > 0) // && nBlinkLU > 0)
        {
                int mx, my;
                getmaxyx(stdscr, my, mx);
                my /= 2;
                mx /= 2;
                mx -= 4;
                mvprintw(my, mx, "LEVEL UP");
        }
        fLastLU--;
        /*if(fLastLU >= INV_LU_BLINK_FRAMES)
        {
                drawLU != drawLU;
                fLastLU = 0;
                //nBlinkLU--;
        }*/
}

/*
 * Move enemies down, and delete out of bounds enemies. also compute end condition. Do this every INV_ENEMY_FRAMES frames.
 */
void processEnemies()
{
        frames--;
        if(frames >= 0) return;
        frames = INV_ENEMY_FRAMES;
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        while(enemies != NULL)
        {
                enemies->row += 1;
                if(enemies->row >= max_y-2)
                {
                        gm = false;
                        gor = enemies->col;
                        gol = gor;
                }
                if(enemies->row < 0 || enemies->row >= max_y-2 || enemies->col < 0 || enemies->col >= max_x)
                {
                        Sprite* e = enemies;
                        enemies = enemies->nxt;
                        delete e;
                        continue;
                }
                else break;
        }
        if(enemies == NULL) return;
        Sprite* p = enemies;
        Sprite* si = p->nxt;
        while(si != NULL)
        {
                si->row += 1;
                if(si -> row >= max_y-2)
                {
                        gm = false;
                        gor = si -> col;
                        gol = gor;
                }
                if(enemies->row < 0 || enemies->row >= max_y-2 || enemies->col < 0 || enemies->col >= max_x)
                {
                        p->nxt = si->nxt;
                        Sprite* s = si;
                        si = si->nxt;
                        delete s;
                }
                else
                {
                        p = si;
                        si = si->nxt;
                }
        }
}

int max(int a, int b) { return a >= b ? a : b; }
int min(int a, int b) { return a <= b ? a : b; }

void spawnEnemy()
{
        spawnFrames++;
        if(spawnFrames < spawnInterv) return;
        spawnFrames = 0;
        int max_x, max_y;
        getmaxyx(stdscr, max_y, max_x);
        Sprite* s = new Sprite();
        int mrg = INV_PLAYER_SPEED * spawnInterv * INV_SPAWN_MARGIN_MULTIPLIER;      // Margin. make large, but possible.
        s->col = rand() % max_x;
        while(s->col > lstEnem + mrg || s->col < lstEnem - mrg)
                s->col = rand() % max_x;
        s->row = 0;
        s->nxt = enemies;
        enemies = s;
        nenemies++;
        if(nenemies >= INV_REDUCTION_ENEMIES)
        {
                lv++;
                fLastLU = INV_LU_BLINK_FRAMES;
                nenemies = 0;
                spawnInterv *= INV_REDUCTION_PERCENT;
                spawnInterv /= 100;
        }
}

void addExplosion(int x, int y)
{
        Explosion e;
        e.row = y;
        e.col = x;
        explosions.push_front(e);
}
/*
 * An utility for ccd.
 */
bool ccdCheck(int x, int y, int xs, int ys, int p, int q)
{
        return ((x == p || x == p-1) && y == q); 
}

/* 
 * Also checks if bullet is out of bounds
 */
bool checkEnemyCollisions(int x, int y, int xs, int ys)
{
        int mx, my;
        getmaxyx(stdscr, my, mx);
        if(x < 0 || x >= mx || y < 0 || y >= my) return true;
        bool ret = false;
        while(enemies != NULL)
                if(ccdCheck(x, y, xs, ys, enemies->col, enemies->row))
                {
                        ret = true;
                        enemies = enemies->nxt;
                        continue;
                }
                else    break;
        if(enemies != NULL)
        {
                Sprite* p = enemies;
                Sprite* i = p->nxt;
                while(i != NULL)
                        if(ccdCheck(x, y, xs, ys, i->col, i->row))
                        {
                                ret = true;
                                Sprite* t = i;
                                i = i->nxt;
                                p->nxt = i;
                            delete t;
                        }
                        else
                        {
                                p = i;
                                i = i->nxt;
                        }
        }

        // Add an explosion
        if(ret)
        {
                score++;
                addExplosion(x, y);
        }

        return ret;
}

void processBullets()
{
        while(bullets != NULL)
        {
                bullets->row -= INV_BULLET_SPEED;
                bullets->col += INV_PLAYER_SPEED * bullets->typ;
                if(checkEnemyCollisions(bullets->col, bullets->row, INV_PLAYER_SPEED, -INV_BULLET_SPEED))
                {
                        Bullet* t = bullets;
                        bullets = bullets->nxt;
                        delete t;
                        continue;
                }
                else    break;
        }
        if(bullets == NULL)    return;
        Bullet* p = bullets;
        Bullet* i = p->nxt;
        while( i != NULL)
        {
                i->row -= INV_BULLET_SPEED;
                i->col += INV_PLAYER_SPEED * i->typ;
                if(checkEnemyCollisions(i->col, i->row, INV_PLAYER_SPEED, -INV_BULLET_SPEED))
                {
                        Bullet* t = i;
                        i = i->nxt;
                        p->nxt = i;
                        delete t;
                }
                else
                {
                        p = i;
                        i = i->nxt;
                }
        }
}



void spawnBullet(int typ)
{
        Bullet* s = new Bullet(typ);
        int mx, my;
        getmaxyx(stdscr, my, mx);
        s->row = my - 3;
        s->col = posX;
        s->nxt = bullets;
        bullets = s;
}

void reset()
{
        while(enemies != NULL)
        {
                Sprite* t = enemies;
                enemies = enemies->nxt;
                delete t;
        }
        while(bullets != NULL)
        {
                Bullet* t = bullets;
                bullets = bullets->nxt;
                delete t;
        }
        explosions.clear();
        posX = 5;
        cont = true;
        score = 0;
        frames = INV_ENEMY_FRAMES;
        spawnInterv = INV_INITIAL_INTERV;
        spawnFrames = 0;
        gm = true;
        f = 0;
        lv = 1;
}



int main()
{

        posX = 5;

        initNcurses();
        
        while(true)
        {
        
        while(cont)
        {
                std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
                clear();
                            
                int mx, my;
                getmaxyx(stdscr, my, mx);
                attron(COLOR_PAIR(INV_CP_LINE));
                for(int i = 0; i < mx; i++)
                        mvaddch(my-2, i, '-' | A_BOLD);
                attroff(COLOR_PAIR(INV_CP_LINE));
                drawPlayer();
                drawBullets();
                drawEnemies();
                drawExplosions();
                drawLevelUp();
                move(0, 0);
                printw("SCORE: ");
                printw(std::to_string(score).c_str());
                printw(" LEVEL: ");
                printw(std::to_string(lv).c_str());
#ifdef INV_DEBUG
                printw(" MARGIN: ");
                printw(std::to_string(INV_PLAYER_SPEED * spawnInterv * INV_SPAWN_MARGIN_MULTIPLIER).c_str());
#endif
                refresh();
            if(gm)
            {
                int max_x, max_y;
                getmaxyx(stdscr, max_y, max_x);
                int ch = getch();
                if(ch == 97 && posX > 1)           // 'a' = left++
                {
                        spd = INV_PLAYER_SPEED * 2;
                        posX -= spd;
                }
                if(ch == 121 && posX > 1)           // 'y' = left+shoot
                {
                        spd = INV_PLAYER_SPEED;
                        posX -= spd;
                        spawnBullet(-1);
                }
                if(ch == 117)                       // 'u' = shoot
                {
                        spawnBullet(0);
                        spd = 0;
                }
                if(ch == 105 && posX < max_x-2)     // 'i' = right+shoot
                {
                        spd = INV_PLAYER_SPEED;
                        posX += spd;
                        spawnBullet(1);
                }
                if(ch == 100 && posX < max_x-2)     // 'd' = right++
                {
                        spd = INV_PLAYER_SPEED * 2;
                        posX += spd;
                }
                processBullets();
                processEnemies();
                spawnEnemy();
            }
            else
            {
                    f++;
                    if(f == INV_GO_TIME)
                    {
                            f = 0;
                            gor++;
                            if(gor < mx)
                                    addExplosion(gor, my-2);
                            if(gor >= posX-1 && gor <= posX+1)
                                    addExplosion(gor, my-1);
                            gol--;
                            if(gol >= 0)
                                    addExplosion(gol, my-2);
                            if(gol >= posX-1 && gol <= posX+1)
                                    addExplosion(gol, my-1);
                            if(gor >= mx && gol < 0)
                                    cont = false;
                    }
            }
#ifdef INV_LAG_PRINT
                bool lag = true;
#endif
                while(std::chrono::steady_clock::now() - t1 
                                < std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                        std::chrono::milliseconds(INV_FRAMETIME_MS)))
                {
#ifdef INV_LAG_PRINT
                        lag = false;
#endif
                }

#ifdef INV_LAG_PRINT
                if(lag)
                {
                        printw("LAAG");
                        refresh();
                }
#endif
        }

        int x, y;
        max_score = score > max_score ? score : max_score;
        getmaxyx(stdscr, y, x);
        y /= 2;
        x /= 2;
        x -= 4;
        mvprintw(y, x, "GAME OVER");
        y += 1;
        mvprintw(y, x, "SCORE:");
        printw(std::to_string(score).c_str());
        y += 1;
        mvprintw(y, x, "LEVEL:");
        printw(std::to_string(lv).c_str());
        x -= 2;
        y += 1;
        mvprintw(y, x, "MAX SCORE:");
        printw(std::to_string(max_score).c_str());
        std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
        while(std::chrono::steady_clock::now() - t1 
                                < std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::seconds(2)))
        {
                getch();
        }
        x -= 5;
        y += 1;
        mvprintw(y, x, "PRESS ANY KEY TO REPEAT");

        while(getch() == ERR);
        reset();
        }

        cleanup();
        return 0;
}
