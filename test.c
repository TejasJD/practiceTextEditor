/*** INCLUDES ***/
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

/*** DEFINES ***/
// CTRL_KEY macro would now allow me to convert normal char to CTRL + char
// to do so we only keep the first five bits from lsb and strip all other bits
// hence we do & with (1 << 5) - 1 = 32 - 1 = 31 = in binary would be 00011111
#define CTRL_KEY(k) ((k) & ((1 << 5) - 1))

/*** DATA ***/
struct termios orig_termios;

/*** TERMINAL ***/
void die(const char *s) {
    perror(s);
    exit(1);
}

// we disable raw mode by setting terminal to its original terminal attributes
void disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) die("tcsetattr");
}

// we change terminal from canonical mode to raw mode
void enableRawMode() {
    // get terminal attributes into orig_termios
    if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");

    // registers function to be called when we exit program
    atexit(disableRawMode);

    // we save orig termios into raw, and then we modify raw
    struct termios raw = orig_termios;
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

char editorReadKey() {
    int rd;
    char c;
    while((rd = read(STDIN_FILENO, &c, 1)) != 1) {
        if(rd == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

/*** INPUT ***/
// this function process the pressed keys
void editorProcessKeyPress() {
    char c = editorReadKey();

    switch (c) {
    case CTRL_KEY('q'):
        exit(0);
        break;
    }
}

/*** OUTPUT ***/
// this function refreshes the screen for text editor
void editorRefreshScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
}

/*** INIT ***/
int main() {
    enableRawMode();
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