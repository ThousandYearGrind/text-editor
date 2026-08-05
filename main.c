#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void die(const char *s) {
  // perror looks at global error variable and prints a message
  // prints s before printing error msg
  perror(s);
  // signal program failure
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcsetattr");
  atexit(disableRawMode);

  struct termios raw = orig_termios;
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

int main(void) {
  enableRawMode();
  
  while (1) {
    char c = '\0';
    // read timing out returns -1 w/ errno EAGAIN in Cygwin
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    }
    else {
      printf("%d ('%c')\r\n", c, c);
    }
    if (c == 'q') break;
  }

  return 0;
}
