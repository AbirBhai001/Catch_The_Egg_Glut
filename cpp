#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Window
const int WINDOW_W = 800;
const int WINDOW_H = 600;

// Game
enum GameState { STATE_MENU, STATE_PLAYING, STATE_PAUSED, STATE_GAMEOVER };
GameState gState = STATE_MENU;

int score = 0;
int highscore = 0;
int timeRemaining = 60;     // seconds
bool muted = false;

// Basket
struct Basket {
    float x;
    float y;
    float width;
    float height;
    float defaultWidth;
} basket;

// Chicken (simple)
struct Chicken {
    float x;
    float y;
    float speed;
    int dir; // -1 left, +1 right
} chicken;

// Egg & Powerup
enum EggType { EGG_NORMAL = 0, EGG_BLUE, EGG_GOLD, EGG_POOP, EGG_COUNT };

struct Egg {
    float x, y;
    float vy;   // vertical speed
    EggType type;
};

struct Powerup {
    float x, y;
    float vy;
    int type; // 0=enlarge,1=slow,2=+time
    float duration; // for temporary effects
};

std::vector<Egg> eggs;
std::vector<Powerup> powerups;

// Timers / spawn control
int spawnIntervalMs = 800; // spawn egg every N ms (will decrease over time)
int lastSpawnTime = 0;
int lastPowerupSpawn = 0;

// Effects
bool effectEnlarged = false;
int enlargeRemainMs = 0;
bool effectSlowed = false;
int slowRemainMs = 0;

// Utility
std::string highscoreFile = "highscore.txt";

void loadHighscore() {
    std::ifstream fin(highscoreFile.c_str());
    if (fin.is_open()) {
        fin >> highscore;
        fin.close();
    } else highscore = 0;
}

void saveHighscore() {
    if (score > highscore) {
        std::ofstream fout(highscoreFile.c_str());
        if (fout.is_open()) {
            fout << score;
            fout.close();
            highscore = score;
        }
    }
}

void drawText(int x, int y, const std::string &s) {
    glRasterPos2i(x, y);
    for (char c : s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
}

void drawFilledCircle(float cx, float cy, float r, int num_segments = 20) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int ii = 0; ii <= num_segments; ii++) {
        float theta = 2.0f * M_PI * float(ii) / float(num_segments);
        float px = r * cosf(theta);
        float py = r * sinf(theta);
        glVertex2f(cx + px, cy + py);
    }
    glEnd();
}

void drawBasket() {
    glPushMatrix();
    // basket body
    glColor3f(0.6f, 0.35f, 0.1f);
    glBegin(GL_QUADS);
    glVertex2f(basket.x - basket.width/2, basket.y);
    glVertex2f(basket.x + basket.width/2, basket.y);
    glVertex2f(basket.x + basket.width/2, basket.y + basket.height);
    glVertex2f(basket.x - basket.width/2, basket.y + basket.height);
    glEnd();

    // rim
    glColor3f(0.4f, 0.2f, 0.05f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(basket.x - basket.width/2, basket.y + basket.height);
    glVertex2f(basket.x + basket.width/2, basket.y + basket.height);
    glVertex2f(basket.x + basket.width/2, basket.y + basket.height + 6);
    glVertex2f(basket.x - basket.width/2, basket.y + basket.height + 6);
    glEnd();
    glPopMatrix();
}

void drawChicken() {
    glPushMatrix();
    glTranslatef(chicken.x, chicken.y, 0);

    // body
    glColor3f(1.0f, 0.9f, 0.6f);
    drawFilledCircle(0, 0, 20, 24);

    // head
    glColor3f(1.0f, 0.95f, 0.8f);
    drawFilledCircle(26, 10, 12, 18);

    // beak
    glColor3f(1.0f, 0.6f, 0.0f);
    glBegin(GL_TRIANGLES);
    glVertex2f(38, 10);
    glVertex2f(48, 14);
    glVertex2f(38, 6);
    glEnd();

    // eye
    glColor3f(0,0,0);
    drawFilledCircle(28, 16, 2, 8);
    glPopMatrix();
}

void spawnEggNow() {
    Egg e;
    e.x = chicken.x + (rand()%41 - 20); // small offset
    e.y = chicken.y - 30;
    e.vy = 1.5f + (rand()%30)/10.0f;
    int r = rand() % 100;
    if (r < 60) e.type = EGG_NORMAL;
    else if (r < 80) e.type = EGG_BLUE;
    else if (r < 95) e.type = EGG_GOLD;
    else e.type = EGG_POOP;
    eggs.push_back(e);
}

void spawnPowerupNow() {
    Powerup p;
    p.x = (float)(50 + rand() % (WINDOW_W - 100));
    p.y = WINDOW_H - 60;
    p.vy = 1.0f + (rand()%20)/20.0f;
    p.type = rand() % 3;
    p.duration = 8000; // ms
    powerups.push_back(p);
}

void resetGame() {
    score = 0;
    timeRemaining = 60;
    eggs.clear();
    powerups.clear();
    effectEnlarged = false;
    enlargeRemainMs = 0;
    effectSlowed = false;
    slowRemainMs = 0;
    basket.width = basket.defaultWidth;
    chicken.x = WINDOW_W / 2.0f;
    chicken.dir = (rand()%2)?1:-1;
    spawnIntervalMs = 800;
    lastSpawnTime = glutGet(GLUT_ELAPSED_TIME);
    lastPowerupSpawn = lastSpawnTime;
}

void applyPowerup(const Powerup &p) {
    if (p.type == 0) { // enlarge
        effectEnlarged = true;
        enlargeRemainMs = 10000; // 10 sec
        basket.width = basket.defaultWidth * 1.6f;
    } else if (p.type == 1) { // slow
        effectSlowed = true;
        slowRemainMs = 8000;
    } else if (p.type == 2) { // +time
        timeRemaining += 10;
    }
}

void updatePhysics(int msDelta) {
    int now = glutGet(GLUT_ELAPSED_TIME);

    // chicken moves back and forth
    chicken.x += chicken.speed * chicken.dir * (msDelta / 16.0f);
    if (chicken.x < 60) { chicken.x = 60; chicken.dir = 1; }
    if (chicken.x > WINDOW_W - 60) { chicken.x = WINDOW_W - 60; chicken.dir = -1; }

    // spawn eggs based on interval
    if (now - lastSpawnTime >= spawnIntervalMs) {
        spawnEggNow();
        lastSpawnTime = now;
        // gradually increase difficulty
        if (spawnIntervalMs > 350 && rand()%10==0) spawnIntervalMs -= 10;
    }

    // rare powerup spawn
    if (now - lastPowerupSpawn >= 12000 + rand()%8000) {
        spawnPowerupNow();
        lastPowerupSpawn = now;
    }

    // update eggs
    float globalSlowFactor = effectSlowed ? 0.5f : 1.0f;
    for (auto it = eggs.begin(); it != eggs.end();) {
        it->y -= it->vy * globalSlowFactor * (msDelta / 16.0f);
        // catch check
        if (it->y <= basket.y + basket.height && it->y >= basket.y) {
            if (it->x >= basket.x - basket.width/2 && it->x <= basket.x + basket.width/2) {
                // caught
                switch (it->type) {
                    case EGG_NORMAL: score += 1; break;
                    case EGG_BLUE: score += 5; break;
                    case EGG_GOLD: score += 10; break;
                    case EGG_POOP: score -= 10; break;
                    default: break;
                }
                it = eggs.erase(it);
                continue;
            }
        }
        // miss -> if below 0, remove
        if (it->y < -20) {
            it = eggs.erase(it);
            continue;
        } else ++it;
    }

    // update powerups
    for (auto it = powerups.begin(); it != powerups.end();) {
        it->y -= it->vy * (msDelta / 16.0f);
        if (it->y <= basket.y + basket.height && it->y >= basket.y) {
            if (it->x >= basket.x - basket.width/2 && it->x <= basket.x + basket.width/2) {
                applyPowerup(*it);
                it = powerups.erase(it);
                continue;
            }
        }
        if (it->y < -20) it = powerups.erase(it);
        else ++it;
    }

    // update effect timers
    if (effectEnlarged) {
        enlargeRemainMs -= msDelta;
        if (enlargeRemainMs <= 0) {
            effectEnlarged = false;
            basket.width = basket.defaultWidth;
        }
    }
    if (effectSlowed) {
        slowRemainMs -= msDelta;
        if (slowRemainMs <= 0) {
            effectSlowed = false;
        }
    }
}

// GLUT timers & display
int lastFrameTime = 0;

void drawEgg(const Egg &e) {
    switch (e.type) {
        case EGG_NORMAL: glColor3f(1,1,1); break;
        case EGG_BLUE: glColor3f(0.2f,0.4f,0.9f); break;
        case EGG_GOLD: glColor3f(1.0f,0.85f,0.2f); break;
        case EGG_POOP: glColor3f(0.45f,0.25f,0.08f); break;
        default: glColor3f(1,1,1); break;
    }
    drawFilledCircle(e.x, e.y, 12, 20);
}

void drawPowerup(const Powerup &p) {
    if (p.type == 0) { // enlarge -> box with E
        glColor3f(0.6f, 0.9f, 0.6f);
        glBegin(GL_QUADS);
        glVertex2f(p.x-10,p.y-10); glVertex2f(p.x+10,p.y-10);
        glVertex2f(p.x+10,p.y+10); glVertex2f(p.x-10,p.y+10);
        glEnd();
        glColor3f(0,0,0);
        drawText((int)(p.x-6),(int)p.y-4,"E");
    } else if (p.type == 1) { // slow -> S
        glColor3f(0.6f,0.6f,0.9f);
        drawFilledCircle(p.x,p.y,10,12);
        glColor3f(0,0,0); drawText((int)p.x-5,(int)p.y-5,"S");
    } else { // +time -> T
        glColor3f(0.95f,0.8f,0.6f);
        glBegin(GL_TRIANGLES);
        glVertex2f(p.x,p.y+10); glVertex2f(p.x-10,p.y-10); glVertex2f(p.x+10,p.y-10);
        glEnd();
        glColor3f(0,0,0); drawText((int)p.x-5,(int)p.y-5,"T");
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (gState == STATE_MENU) {
        glColor3f(1,1,1);
        drawText(300, 420, "Catch The Eggs");
        drawText(300, 380, "Press ENTER to Start");
        drawText(300, 350, "Right-click for menu (Resume/Exit)");
        drawText(300, 320, "Highscore: " + std::to_string(highscore));
    }
    else if (gState == STATE_PLAYING || gState == STATE_PAUSED) {
        // game area
        // draw chicken
        drawChicken();

        // draw eggs
        for (const Egg &e : eggs) drawEgg(e);

        // draw powerups
        for (const Powerup &p : powerups) drawPowerup(p);

        // draw basket
        drawBasket();

        // HUD
        glColor3f(1,1,1);
        drawText(10, WINDOW_H - 25, "Score: " + std::to_string(score));
        drawText(WINDOW_W-160, WINDOW_H - 25, "Time: " + std::to_string(timeRemaining));
        drawText(WINDOW_W-320, WINDOW_H - 25, "High: " + std::to_string(highscore));
        if (effectEnlarged) drawText(10, WINDOW_H - 50, "Enlarged!");
        if (effectSlowed) drawText(140, WINDOW_H - 50, "Slowed!");

        if (gState == STATE_PAUSED) {
            glColor3f(1,0.8f,0.2f);
            drawText(WINDOW_W/2-60, WINDOW_H/2, "Game Paused");
            drawText(WINDOW_W/2-120, WINDOW_H/2 - 30, "Press P to Resume or ENTER to Restart");
        }
    }
    else if (gState == STATE_GAMEOVER) {
        glColor3f(1,0.6f,0.6f);
        drawText(WINDOW_W/2 - 90, WINDOW_H/2 + 40, "Game Over!");
        drawText(WINDOW_W/2 - 90, WINDOW_H/2 + 10, ("Your score: " + std::to_string(score)));
        drawText(WINDOW_W/2 - 90, WINDOW_H/2 - 20, ("Highscore: " + std::to_string(highscore)));
        drawText(WINDOW_W/2 - 90, WINDOW_H/2 - 50, "Press ENTER to Play Again or ESC to Exit");
    }

    glutSwapBuffers();
}

void idleTimer(int) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    if (lastFrameTime == 0) lastFrameTime = now;
    int msDelta = now - lastFrameTime;
    lastFrameTime = now;

    if (gState == STATE_PLAYING) {
        updatePhysics(msDelta);
    }

    glutPostRedisplay();
    glutTimerFunc(16, [](int){ idleTimer(0); }, 0);
}

// game clock: reduces timeRemaining each second when playing
void gameClock(int) {
    if (gState == STATE_PLAYING) {
        timeRemaining--;
        if (timeRemaining <= 0) {
            gState = STATE_GAMEOVER;
            saveHighscore();
        }
    }
    glutTimerFunc(1000, [](int){ gameClock(0); }, 0);
}

// Input
void keyboard(unsigned char key, int x, int y) {
    if (key == 27) { // ESC
        saveHighscore();
        exit(0);
    } else if (key == 13) { // ENTER
        if (gState == STATE_MENU || gState == STATE_GAMEOVER) {
            resetGame();
            gState = STATE_PLAYING;
        } else if (gState == STATE_PAUSED) {
            // restart
            resetGame();
            gState = STATE_PLAYING;
        }
    } else if (key == 'p' || key == 'P') {
        if (gState == STATE_PLAYING) gState = STATE_PAUSED;
        else if (gState == STATE_PAUSED) gState = STATE_PLAYING;
    } else if (key == 'a' || key == 'A' || key == 75) { // left / 'a'
        basket.x -= 30;
        if (basket.x - basket.width/2 < 0) basket.x = basket.width/2;
    } else if (key == 'd' || key == 'D' || key == 77) { // right / 'd'
        basket.x += 30;
        if (basket.x + basket.width/2 > WINDOW_W) basket.x = WINDOW_W - basket.width/2;
    }
}

void specialKeys(int key, int x, int y) {
    if (key == GLUT_KEY_LEFT) {
        basket.x -= 30;
        if (basket.x - basket.width/2 < 0) basket.x = basket.width/2;
    } else if (key == GLUT_KEY_RIGHT) {
        basket.x += 30;
        if (basket.x + basket.width/2 > WINDOW_W) basket.x = WINDOW_W - basket.width/2;
    }
}

void passiveMotion(int x, int y) {
    // GLUT mouse y is top-left; our coords are bottom-left origin
    int yy = WINDOW_H - y;
    basket.x = x;
    if (basket.x - basket.width/2 < 0) basket.x = basket.width/2;
    if (basket.x + basket.width/2 > WINDOW_W) basket.x = WINDOW_W - basket.width/2;
}

// Menu (right-click)
void menuHandler(int choice) {
    if (choice == 0) { // Resume/Start
        if (gState == STATE_MENU || gState == STATE_GAMEOVER) {
            resetGame();
            gState = STATE_PLAYING;
        } else if (gState == STATE_PAUSED) {
            gState = STATE_PLAYING;
        }
    } else if (choice == 1) { // Pause
        if (gState == STATE_PLAYING) gState = STATE_PAUSED;
    } else if (choice == 2) { // Exit
        saveHighscore();
        exit(0);
    }
}

void createRightClickMenu() {
    int menu = glutCreateMenu(menuHandler);
    glutAddMenuEntry("Start/Resume", 0);
    glutAddMenuEntry("Pause", 1);
    glutAddMenuEntry("Exit", 2);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void reshape(int w, int h) {
    // For simplicity we keep fixed logical size; resize does not change coords
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_W, 0, WINDOW_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// Init
void initGL() {
    glClearColor(0.12f, 0.18f, 0.22f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_W, 0, WINDOW_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    srand((unsigned)time(NULL));

    // initialize basket and chicken
    basket.x = WINDOW_W / 2.0f;
    basket.y = 40;
    basket.defaultWidth = 120;
    basket.width = basket.defaultWidth;
    basket.height = 22;

    chicken.x = WINDOW_W/2.0f;
    chicken.y = WINDOW_H - 70;
    chicken.speed = 1.6f + (rand()%10)/10.0f;
    chicken.dir = (rand()%2)?1:-1;

    loadHighscore();
    lastSpawnTime = glutGet(GLUT_ELAPSED_TIME);
    lastPowerupSpawn = lastSpawnTime;
    lastFrameTime = 0;
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_W, WINDOW_H);
    glutInitWindowPosition(200, 50);
    glutCreateWindow("Catch The Eggs (Furnished Version)");

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutPassiveMotionFunc(passiveMotion);

    // timers
    glutTimerFunc(16, [](int){ idleTimer(0); }, 0);
    glutTimerFunc(1000, [](int){ gameClock(0); }, 0);

    createRightClickMenu();

    glutMainLoop();
    return 0;
}
