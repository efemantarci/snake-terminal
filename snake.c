#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#define CTRL_KEY(k) ((k) & 0x1f)

struct stringBuffer{
    char *pointer;
    int length;
};
struct snakeCoord{
    int x;
    int y;
};

enum directions{
    UP = 0,
    DOWN,
    LEFT,
    RIGHT,
};

void stringBufferAppend(struct stringBuffer *buffer,char *str,int len){
    char *new = realloc(buffer->pointer,buffer->length + len);
    if(new == NULL)return;
    memcpy(&new[buffer->length],str,len);
    buffer->pointer = new;
    buffer->length += len;
}

void stringBufferFree(struct stringBuffer *buffer){
    free(buffer->pointer);
}

struct snakeCoord createCoord(int x,int y){
    struct snakeCoord new;
    new.x = x;
    new.y = y;
    return new;
}

#define ABUF_INIT {NULL,0}
struct EditorConfig{
    struct termios old_termios;
    int screenrows;
    int screencols;
    int **screen;
    struct snakeCoord snakeArr[100];
    int snakeSize;
    int moveDirection;
    bool isDead;
    bool appleExist;
    int appleX;
    int appleY;
};

struct EditorConfig E;
void die(char *s){
    perror(s);
    exit(1);
}
void exitRawMode(){
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&E.old_termios);
}
void enterRawMode(){
    tcgetattr(STDIN_FILENO, &E.old_termios);
    atexit(exitRawMode);
    struct termios raw = E.old_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int drawBorders(struct stringBuffer *buffer){
    //SCORE : 120
    // 9 + scoreLength
    char buf[12];
    snprintf(buf,12,"SCORE : %d",E.snakeSize);
    stringBufferAppend(buffer,(char *)buf,12);
    //Clear out others
    return strlen(buf);
}
void draw(struct stringBuffer *buffer){
    int scoreSize = drawBorders(buffer);
    int y;
    for(int i = 0;i < E.snakeSize;i++){
        if(E.isDead && i == 0)continue;
        E.screen[E.snakeArr[i].y][E.snakeArr[i].x] = 2;
    }
    char *endMessages[] = {"YOU ARE DEAD", "TO QUIT, PRESS 'q'","TO RESTART PRESS 'r'"};
    int endMessageSizes[] = {13,19,21};
    int endMessageCount = 3;
    for (y = 0; y < E.screenrows - 1; y++) {
        for(int x = 0;x < E.screencols - 1;x++){
            //For printing score
            if(y == 0 && x < scoreSize)continue;
            //End message
            if(E.isDead && y >= E.screenrows / 2 && y < E.screenrows / 2 + endMessageCount){
                int endMessageIndex = y - E.screenrows / 2;
                int padding = (E.screencols - endMessageSizes[endMessageIndex]) / 2;
                if(x >= padding && x < padding + endMessageSizes[endMessageIndex]){
                    stringBufferAppend(buffer,&endMessages[endMessageIndex][x - padding],1);
                    continue;
                }
            }
            /*
            if(E.isDead && y == E.screenrows / 2 && (x >= padding && x < padding + endMessageSize)){
                stringBufferAppend(buffer,&endMessage[x - padding],1);
                continue;
            }
            */
            //int val = E.screen[y * (E.screencols) + x];
            int val = E.screen[y][x];
            switch (val)
            {
            case 2:
                stringBufferAppend(buffer, "\x1b[7m", 4);
                stringBufferAppend(buffer," ",1);
                stringBufferAppend(buffer, "\x1b[m", 3);
                break;
            case 1:
                stringBufferAppend(buffer, "\x1b[1m", 4);
                stringBufferAppend(buffer,"#",1);
                stringBufferAppend(buffer, "\x1b[m", 3);
                break;
            default:
                stringBufferAppend(buffer," ",1);
                break;
            }
        }
        //stringBufferAppend(buffer, "\x1b[K", 3);
        if (y < E.screenrows - 1) {
            stringBufferAppend(buffer, "\r\n", 2);
        }
  }
}

void refreshScreen(){
    struct stringBuffer buffer = ABUF_INIT;
    //Hide cursor
    if(!E.isDead)stringBufferAppend(&buffer, "\x1b[?25l", 6);
    stringBufferAppend(&buffer,"\x1b[H",3);
    draw(&buffer);
    stringBufferAppend(&buffer,"\x1b[H",3);
    //Un-hide cursor
    if(E.isDead)stringBufferAppend(&buffer, "\x1b[?25h", 6);
    write(STDOUT_FILENO,buffer.pointer,buffer.length);
}
void addApple(){
    bool success = false;
    int appleY;
    int appleX;
    while(!success){
        success = true;
        int r = rand();
        appleY = r % E.screenrows;
        appleX = r % E.screencols;
        for(int i = 0;i < E.snakeSize;i++){
            if(E.snakeArr[i].x == appleX && E.snakeArr[i].y == appleY){
                success = false;
                break;
            }
        }
    }
    E.screen[appleY][appleX] = 1;
    E.appleExist = true;
    E.appleX = appleX;
    E.appleY = appleY;

}
int getCursorPosition(int *rows, int *cols){
	char buf[32];
	unsigned int i = 0;

	if(write(STDOUT_FILENO,"\x1b[6n",4) != 4)return -1;
	while(i < sizeof(buf) - 1){
		if(read(STDIN_FILENO, &buf[i],1) != 1)break;
		if(buf[i] == 'R')break;
		i++;
	}
	buf[i] = '\0';
	if(buf[0] != '\x1b' || buf[1] != '[')return -1;
	if(sscanf(&buf[2],"%d;%d",rows,cols) != 2)return -1;
	return 0;
}

int getWindowSize(int *rows, int *cols){
	struct winsize ws;
	if(ioctl(STDOUT_FILENO,TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B",12) != 12)return -1;
		return getCursorPosition(rows,cols);
	}else{
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

char editorReadKey(){
	char c;
	int num_bytes_read = read(STDIN_FILENO, &c, 1);
    return c;
}

void snakeMove(){
    int tempY = E.snakeArr[E.snakeSize - 1].y;
    int tempX = E.snakeArr[E.snakeSize - 1].x;
    E.screen[tempY][tempX] = 0;
    for(int i = E.snakeSize - 1;i > 0;i--){
        E.snakeArr[i].x = E.snakeArr[i - 1].x;
        E.snakeArr[i].y = E.snakeArr[i - 1].y; 
    }
    int *headX = &E.snakeArr[0].x;
    int *headY = &E.snakeArr[0].y;
    switch (E.moveDirection)
    {
    case UP:
        *headY -= 1;
        break;
    case LEFT:
        *headX -= 1;
        break;
    case RIGHT:
        *headX += 1;
        break;
    case DOWN:
        *headY += 1;
        break;
    }
    //Border detection
    if(*headX < 0 || *headX >= E.screencols || *headY < 0 || *headY >= E.screenrows){
        E.isDead = true;
        return;
    }
    //Collision detection
    for(int i = 1;i < E.snakeSize - 1;i++){
        if(E.snakeArr[i].x == *headX && E.snakeArr[i].y == *headY){
            E.isDead = true;
            return;
        }
    }
    //Apple detection
    if(*headX == E.appleX && *headY == E.appleY){
        E.appleExist = false;
        E.snakeSize++;
        E.snakeArr[E.snakeSize - 1] = createCoord(tempX,tempY);
        return;
    }
}

void init(){
    if(!E.isDead)enterRawMode();
    getWindowSize(&E.screenrows,&E.screencols);
    E.screen = (int **)malloc(E.screenrows * sizeof(int *));
    for(int i = 0;i < E.screenrows;i++){
        E.screen[i] = (int *)malloc(E.screencols * sizeof(int));
    }
    E.snakeSize = 4;
    for(int i = 0;i < E.snakeSize;i++){
        E.snakeArr[i] = createCoord(i,2);
    }
    E.moveDirection = DOWN;
    E.isDead = false;
    E.appleExist = false;
    srand(time(NULL));
}

/*** input ***/
void editorProcessKeypress(){
	char c = editorReadKey();
	switch(c){
        case '\x1b':
            char buf[3];
            if(read(STDIN_FILENO,&buf[0],1) != 1)return;
            if(read(STDIN_FILENO,&buf[1],1) != 1)return;
            if(buf[0] == '['){
                switch (buf[1])
                {
                        case 'A':
                            //Prevent player from killing itself accidentally
                            if(E.moveDirection != DOWN)E.moveDirection = UP;
                            break;
						case 'B':
                            if(E.moveDirection != UP)E.moveDirection = DOWN;
                            break;
						case 'C':
                            if(E.moveDirection != LEFT)E.moveDirection = RIGHT;
                            break;
						case 'D':
                            if(E.moveDirection != RIGHT)E.moveDirection = LEFT;
                            break;
                }
            }
            break;
		case 'q':
			write(STDOUT_FILENO,"\x1b[2J",4);
			write(STDOUT_FILENO,"\x1b[H",3);
			exit(0);
			break;
        case 'r':
            if(E.isDead){
                init();
                break;
            }
        case 'w':
        case 'W':
            //Prevent player from killing itself accidentally
            if(E.moveDirection != DOWN)E.moveDirection = UP;
            break;
        case 'A':
        case 'a':
            if(E.moveDirection != RIGHT)E.moveDirection = LEFT;
            break;
        case 'S':
        case 's':
            if(E.moveDirection != UP)E.moveDirection = DOWN;
            break;
        case 'D':
        case 'd':
            if(E.moveDirection != LEFT)E.moveDirection = RIGHT;
            break;
	}
}

int main(){
    init();
    while (1)
    {
        refreshScreen();
        if(!E.appleExist)addApple();
        editorProcessKeypress();
        if(!E.isDead)snakeMove();
    }
    return 0;
}