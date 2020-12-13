/*** INCLUDES ***/
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/*** DEFINES ***/
// CTRL_KEY macro would now allow me to convert normal char to CTRL + char
// to do so we only keep the first five bits from lsb and strip all other bits
// hence we do & with (1 << 5) - 1 = 32 - 1 = 31 = in binary would be 00011111
#define CTRL_KEY(k) ((k) & ((1 << 5) - 1))
#define TEST_VERSION "0.0.1"

/*** DATA ***/
struct editorConfig {
    int cx, cy;
    int rows;
    int columns;
    struct termios orig_termios;
};

struct editorConfig E;

/*** TERMINAL ***/
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

// we disable raw mode by setting terminal to its original terminal attributes
void disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");
}

// we change terminal from canonical mode to raw mode
void enableRawMode() {
    // get terminal attributes into orig_termios
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");

    // registers function to be called when we exit program
    atexit(disableRawMode);

    // we save orig termios into raw, and then we modify raw
    struct termios raw = E.orig_termios;
    // turn of a flag we do AND and NOT operation
    // use more than 1 flags, we do OR operation
    
    // c input flags
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | ISTRIP | INPCK );
    // c output flags
    raw.c_oflag &= ~(OPOST);
    // c control flags
    raw.c_cflag |= (CS8);
    // c local flags
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    // c
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    // we set the terminal to modified raw attributes
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

// this function waits for key press
char editorReadKey() {
    int rd;
    char c;
    while((rd = read(STDIN_FILENO, &c, 1)) != 1) {
        if(rd == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

// this function gets cursor position
int getCursorPosition(int *rows, int *cols) {
    // this write, we query terminal with n = device status report command
    // 6n means we query the terminal for position of cursor
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
    
    // upon reading the characters we receive an escape sequence from terminal
    // this is as follows \x1b[30;120R which indicates cursor position
    
    char buf[32];
    unsigned int i = 0;
    while(i < sizeof(buf) - 1) {
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    // i is 32 now so..
    buf[i] = '\0';
    // printf("\r\n'%s'\r\n", &buf[1]);
    // editorReadKey();
    if(buf[0] != '\x1b' || buf[1] != '[') return -1;
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

// window size is extracted with this function
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        // another strategy to get window size without relying on ioctl
        // we write two escape sequences to terminal
        // C = cursor forward command and B = cursor down command
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return 1;
        return getCursorPosition(rows, cols);
    }
    else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** APPEND BUFFER ***/
// our own string (like in other languages) dynamic char array
// write operations

struct string {
    char *buf;
    int len;
};

#define STRING_INIT {NULL, 0}

void append(struct string *str, const char *s, int len) {
    char *temp = realloc(str->buf, str->len + len);
    if(temp == NULL) return;
    memcpy(&temp[str->len], s, len);
    str->buf = temp;
    str->len += len;
}

void freeString(struct string *str) {
    free(str->buf);
}

/*** INPUT ***/
// this function process the pressed keys
void editorMoveCursor(char key) {
    switch (key) {
    case 'w':
        E.cy--;
        break;
    case 's':
        E.cy++;
        break;
    case 'a':
        E.cx--;
        break;
    case 'd':
        E.cx++;
        break;
    }
}

void editorProcessKeyPress() {
    char c = editorReadKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case 'w':
        case 's':
        case 'a':
        case 'd':
            editorMoveCursor(c);
            break;
            
    }
}

/*** OUTPUT ***/
// this function prints tildes similar to vim
void editorDrawRows(struct string *str) {
    int i;
    for(i = 0; i < E.rows; ++i) {
        if(i == E.rows / 3) {
            char welcome_msg[80];
            int welcome_len = snprintf(welcome_msg, sizeof(welcome_msg), 
            "Test Editor ** version: %s", TEST_VERSION);
            if(welcome_len > E.columns) welcome_len = E.columns;

            int padding = (E.columns - welcome_len) / 2;
            if(padding) {
                append(str, "~", 1);
                padding--;
            }
            while(padding--) append(str, " ", 1);
            append(str, welcome_msg, welcome_len);  
        }
        else append(str, "~", 1);
        // K = Erase In Line
        // this command erases a line default is <esc>[0K or <esc>[K
        // this clears the line from active position to end of line
        append(str, "\x1b[K", 3);
        if(i < E.rows - 1) append(str, "\r\n", 2);
    }
}

// this function refreshes the screen for text editor
void editorRefreshScreen() {
    struct string str = STRING_INIT;
    // these commands are based on VT100
    // all starting with \x1b are escape sequences,
    // they are commands that allow editor to do certain
    // settings based on needs of the program

    //command to hide/show cursor, h = set mode
    append(&str, "\x1b[?25l", 6);
    // command position cursor to top left default arguments for H = <esc>[1;1H = <esc>[H
    append(&str, "\x1b[H", 3);
    editorDrawRows(&str);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    append(&str, buf, strlen(buf));

    // l = reset mode
    append(&str, "\x1b[?25h", 6);
    write(STDOUT_FILENO, str.buf, str.len);
    freeString(&str);   
}

/*** INIT ***/
void initEditor() {
    // initialize the fields of E editor config struct
    E.cx = 0;
    E.cy = 0;
    if(getWindowSize(&E.rows, &E.columns) == -1) die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();
    while(1) {
        editorRefreshScreen();
        editorProcessKeyPress();
        // char c;
        // if(read(STDIN_FILENO, &c, 1) == -1) die("read");
        // if(iscntrl(c)) printf("%d\r\n", c);
        // else printf("%d ('%c')\r\n", c, c);
        // if(c == 'q') exit(0);  
    }
    return 0;
}