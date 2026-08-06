#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

// ascii bits [4:0] represent numerical order of alphabet
#define CTRL_KEY(k) (k & 0x1f)

struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};

struct editorConfig E;

/** terminal **/

void die(const char *s) {
 write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[1;1H", 6);

  // perror looks at global error variable and prints a message
  // prints s before printing error msg
  perror(s);
  // signal program failure
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcsetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;
  // input flags
  // IXON - pause/resume on terminal stdout
  // ICRNL - translate ^M (carriage return) to newline
  // BRKINT - send SIGINT signal with electrical break signal
  //   relevant on physical terminals (turning this off for tradition)
  // INPCK - parity checking (old, not usually used anymore)
  // ISTRIP - strip 8th bit (ASCII uses 7), not usually used anymore
  raw.c_iflag &= ~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);

  // output flags
  // OPOST - implementation-defined output processing
  //   (e.g. \n -> \r\n)
  //   need to do \r\n for the effect of regular \n now that we turned
  //   OPOST off
  raw.c_oflag &= ~(OPOST);

  // control flags
  // CS8 - sets character size to 8 (tradition, alr set on modern sys)
  raw.c_cflag |= (CS8);

  // local flags (misc)
  // ECHO - echo input
  // ICANON - canonical mode
  // ISIG - ^C and ^Z stop/suspend/quit
  // IEXTEN - implementation-defined input processing
  //   (e.g. ^V ^C -> ^C)
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

  // special characters
  // read can accept 0 characters
  raw.c_cc[VMIN] = 0;
  // read times out after 0.1 sec
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
  int nread = 0;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999B\x1b[999C", 12) != 12) return -1;
    return getCursorPosition(&E.screenrows, &E.screencols);
  }
  else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/** append buffer **/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

/** output **/

void editorDrawRows(struct abuf *ab) {
  for (int y = 0; y < E.screenrows - 1; y++) {
    abAppend(ab, "~\r\n", 3);
  }
  // I found that the final \n will push past the screen and then our
  // tildes will escape being cleared by the J escape sequence
  abAppend(ab, "~", 1);
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  // https://vt100.net/docs/vt100-ug/chapter3.html#ED
  abAppend(&ab, "\x1b[2J", 4);
  // https://vt100.net/docs/vt100-ug/chapter3.html#CUP
  abAppend(&ab, "\x1b[1;1H", 6);

  editorDrawRows(&ab);

  abAppend(&ab, "\x1b[1;1H", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/** input **/

void editorProcessKeypress() {
  char c = editorReadKey();

  switch(c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[1;1H", 6);
    exit(0);
    break;
  }
}

/** init **/

void initEditor() {
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die ("getWindowSize");
}

int main(void) {
  enableRawMode();
  initEditor();
  
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }

  return 0;
}
