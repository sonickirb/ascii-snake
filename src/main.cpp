#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>

#define DISPLAY_WIDTH 32
#define DISPLAY_HEIGHT 32
#define BUFFER_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT)

int clamp(int a, int b, int c) {
    int got = a;
    if (got < b) got = b;
    if (got > c) got = c;

    return got;
}

char *col[] = {
    "\033[1;30m", // black 0
    "\033[1;31m", // red 1
    "\033[1;32m", // green 2
    "\033[1;33m", // yellow 3
    "\033[1;34m", // blue 4
    "\033[1;35m", // magenta 5
    "\033[1;36m", // cyan 6
    "\033[1;37m", // white 7
};
#define BLACK 0
#define RED 1
#define GREEN 2
#define YELLOW 3
#define BLUE 4
#define MAGENTA 5
#define CYAN 6
#define WHITE 7

char *screen_buf[BUFFER_SIZE];
int   color_buf[BUFFER_SIZE];
int frame = 0;
int kb;

void set_pixel(int x, int y, char *to, int color) {
    if (x < 0 || y < 0 || x > DISPLAY_WIDTH-1 || y > DISPLAY_HEIGHT-1) return;
    color_buf[x * DISPLAY_WIDTH + y] = color;
    screen_buf[x * DISPLAY_WIDTH + y] = to;
}

void render() {
    system("clear");
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            printf(col[color_buf[x * DISPLAY_WIDTH + y]]);
            printf(screen_buf[x * DISPLAY_WIDTH + y]);
        }
        printf("\n");
    }
}

#define NB_ENABLE 1
#define NB_DISABLE 0

void nonblock(int state)
{
    struct termios ttystate;
 
    //get the terminal state
    tcgetattr(STDIN_FILENO, &ttystate);
 
    if (state==NB_ENABLE)
    {
        //turn off canonical mode
        ttystate.c_lflag &= ~ICANON;
        //minimum of number input read.
        ttystate.c_cc[VMIN] = 1;
    }
    else if (state==NB_DISABLE)
    {
        //turn on canonical mode
        ttystate.c_lflag |= ICANON;
    }
    //set the terminal attributes.
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

void clear(char* with, int color) {
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        for (int y = 0; y < DISPLAY_HEIGHT; y++) {
            color_buf[x * DISPLAY_WIDTH + y]  = color;
            screen_buf[x * DISPLAY_WIDTH + y] = with;
        }
    }
}

void draw_line(int color, int x1, int y1, int x2, int y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float m = dy / dx;
    for (int x = x1; x < x2; x+=clamp(dx,-1,1)) {
        int y = m * (x - x1) + y1;
        set_pixel(x, y, "█", color);
    }
    // incomes hacky solution for Y
    m = dx / dy;
    for (int y = y1; y < y2; y+=clamp(dy,-1,1)) {
        int x = m * (y - y1) + x1;
        set_pixel(x, y, "█", color);
    }
}

class Edge {
    public:
        int X1, Y1, X2, Y2;

        Edge(int x1, int y1, int x2, int y2);
};

Edge::Edge(int x1, int y1, 
           int x2, int y2) 
{
    if (y1 < y2) {
        X1 = x1;
        Y1 = y1;
        X2 = x2;
        Y2 = y2;
    } else {
        X1 = x2;
        Y1 = y2;
        X2 = x1;
        Y2 = y1;
    }
}

class Span {
    public:
        int X1, X2;

        Span(int x1, int x2);
};

Span::Span(int x1, int x2) {
    if (x1 < x2) {
        X1 = x1;
        X2 = x2;
    } else {
        X1 = x2;
        X2 = x1;
    }
};

void draw_span(int color, Span span, int y) {
    int xdiff = span.X2 - span.X1;
    if (xdiff == 0)
        return;

    float factor = 0.0f;
    float factorStep = 1.0f / (float)xdiff;

    for (int x = span.X1; x < span.X2; x++)
        set_pixel(x, y, "█", color);
}

void draw_spans_between_edges(int color, Edge e1, Edge e2) {
    float e1ydiff = (float)(e1.Y2 - e1.Y1);
    if (e1ydiff == 0.0f)
        return;
    
    float e2ydiff = (float)(e2.Y2 - e2.Y1);
    if (e2ydiff == 0.0f)
        return;

    float e1xdiff = (float)(e1.X2 - e1.X1);
    float e2xdiff = (float)(e2.X2 - e2.X1);

    float factor1 = (float)(e2.Y1 - e1.Y1) / e1ydiff;
    float factorStep1 = 1.0f / e1ydiff;
    float factor2 = 0.0f;
    float factorStep2 = 1.0f / e2ydiff;

    for (int y = e2.Y1; y < e2.Y2; y++) {
        Span span(e1.X1 + (int)(e1xdiff * factor1), e2.X1 + (int)(e2xdiff * factor2));
        draw_span(color, span, y);

        factor1 += factorStep1;
        factor2 += factorStep2;
    }
}

void draw_tri(int color, int x1, int y1, int x2, int y2, int x3, int y3) {
    Edge edges[3] = {
        Edge(x1, y1, x2, y2),
        Edge(x2, y2, x3, y3),
        Edge(x3, y3, x1, y1)
    };

    int maxLength = 0;
    int longEdge = 0;
    
    // find edge with the greatest length in the Y axis
    for (int i = 0; i < 3; i++) {
        int length = edges[i].Y2 - edges[i].Y1;
        if (length > maxLength) {
            maxLength = length;
            longEdge = i;
        }
    }

    int shortEdge1 = (longEdge + 1) % 3;
    int shortEdge2 = (longEdge + 2) % 3;

    draw_spans_between_edges(color, edges[longEdge], edges[shortEdge1]);
    draw_spans_between_edges(color, edges[longEdge], edges[shortEdge2]);
}

bool gameover = false;

class Player {
    public:
        int tx[BUFFER_SIZE];
        int ty[BUFFER_SIZE];
        int x = 0, y = 0;
        int length = 1;

        Player();

        void update() {
            int prevX = tx[0];
            int prevY = ty[0];
            int prev2X, prev2Y;
            tx[0] = x;
            ty[0] = y;

            for (int i = 0; i < length; i++) {
                prev2X = tx[i];
                prev2Y = ty[i];
                tx[i] = prevX;
                ty[i] = prevY;
                prevX = prev2X;
                prevY = prev2Y;
            }
            
            if (kb == 100) x++; // D
            if (kb == 97)  x--; // A
            if (kb == 119) y--; // W
            if (kb == 115) y++; // S

            if (x < 0) x = DISPLAY_WIDTH - 1;
            if (x > DISPLAY_WIDTH - 1) x = 0;
            if (y < 0) y = DISPLAY_HEIGHT - 1;
            if (y > DISPLAY_HEIGHT - 1) y = 0;
            
            for (int i = 0; i < length; i++)
                if (tx[i] == x && ty[i] == y)
                    gameover = true;
            
        }
        void draw() {
            set_pixel(x, y, "█", BLUE);
            for (int i = 1; i < length; i++) set_pixel(tx[i], ty[i], "█", WHITE);
        }
};
Player::Player() {
    x = 0, y = 0;
    for (int i = 0; i < length; i++) {
        tx[i] = i;
        ty[i] = 0;
    }
}

Player plr;

bool inPlayer(int x, int y) {
    // in head
    if (x == plr.x && y == plr.y) 
        return true;
    
    // in tail
    for (int i = 0; i < plr.length; i++)
        if (x == plr.tx[i] && y == plr.ty[i])
            return true;
    
    return false;
}

class Apple {
    public:
        int x, y;

        Apple();

        void moveToRandomPosition() {
            while (inPlayer(x, y)) {
                x = rand() % 32;
                y = rand() % 32;
            }
        }
        void collect() {
            moveToRandomPosition();
            plr.length++;
        }
        void update() {
            if (plr.x == x && plr.y == y) collect();
        }
        void draw() {
            set_pixel(x, y, "█", RED);
        }
};
Apple::Apple() { moveToRandomPosition(); }

Apple apple;

void resetGame() {
    gameover = false;

    plr = Player();
    apple = Apple();
}

int main() {

    nonblock(NB_ENABLE);
    while (true) {
        frame++;

        // input
        kb = fgetc(stdin);

        // update
        if (!gameover) {
            plr.update();
            apple.update();
        } else if (kb > 0) resetGame();

        // draw
        clear("█", BLACK);
        apple.draw();
        plr.draw();
        
        if (gameover) {
            set_pixel(0, 0, "O", RED);
            set_pixel(1, 0, "V", RED);
            set_pixel(2, 0, "E", RED);
            set_pixel(3, 0, "R", RED);
        }

        // render
        render();
    }
    nonblock(NB_DISABLE);

    system("clear");

    return 0;
}
