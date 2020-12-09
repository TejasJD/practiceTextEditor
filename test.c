/*** includes ***/
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & (0x1f))

/*** data ***/
struct termios orig_termios;

/*** terminal ***/
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

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    // we set the terminal to modified raw attributes
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}


char editorReadKey() {
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}
/*** init ***/
int main() {
    enableRawMode();
    while(1) {
        char c = '\0';
        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        if(iscntrl(c)) printf("%d\r\n", c);
        else printf("%d ('%c')\r\n", c, c);

        if(c == 'q') break;
    }
    return 0;
}