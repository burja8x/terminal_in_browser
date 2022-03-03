// Parts of the code are from:
// Embedded Web Server https://mongoose.ws/tutorials/websocket-server/
// book "Advanced Programming in the Unix Environment" http://www.apuebook.com/code3e.html

#include <errno.h>
#include <termios.h>
#include "apue.h"
#include "mongoose.h"


#define BUFFSIZE 1024 * 100
static void tty();
static void parseSize(const char *str);
char *substring(char *destination, const char *source, int beg, int n);
static bool prefix(const char *pre, const char *str);
void exit_all();

static struct mg_connection *con;

static const char *s_listen_on = "ws://0.0.0.0:8000";
static const char *s_web_root = ".";
static int fdm;
static int is_tty_over = 1;
static int size_rows = 30;
static int size_cols = 120;

int nread;
char buf[BUFFSIZE];

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_OPEN) {
  } else if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    if (mg_http_match_uri(hm, "/websocket")) {
      // Upgrade to websocket. From now on, a connection is a full-duplex
      // Websocket connection, which will receive MG_EV_WS_MSG events.
      mg_ws_upgrade(c, hm, NULL);
    } else if (mg_http_match_uri(hm, "/rest")) {
      // Serve REST response
      mg_http_reply(c, 200, "", "{\"result\": %d}\n", 123);
    } else {
      // Serve static files
      struct mg_http_serve_opts opts = {.root_dir = s_web_root};
      mg_http_serve_dir(c, ev_data, &opts);
    }
  } else if (ev == MG_EV_WS_MSG) {
    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;

    if (wm->data.len == 23 &&
        prefix("size: c:", wm->data.ptr)) {  // setting window size
      parseSize(wm->data.ptr);
    } else {
      con = c;

      if (is_tty_over) {
        tty();
      } else {
        if (writen(fdm, wm->data.ptr, wm->data.len) != (ssize_t) wm->data.len)
          err_sys("writen error to stdout");
      }
    }
  }
  (void) fn_data;
}

static bool prefix(const char *pre, const char *str) {
  return strncmp(pre, str, strlen(pre)) == 0;
}

static void parseSize(const char *str) {
  char rows_str[7];
  char cols_str[7];
  substring(cols_str, str, 8, 6);
  substring(rows_str, str, 17, 6);

  printf("new size: rows:'%s'  cols:'%s'\n", rows_str, cols_str);
  fflush(stdout);
  int row_i = atoi(rows_str);
  int col_i = atoi(cols_str);

  if (row_i > 2 && col_i > 6) {
    size_rows = row_i;
    size_cols = col_i;
  }
  printf("%d %d\n", size_rows, size_cols);
  fflush(stdout);
}
char *substring(char *destination, const char *source, int beg, int n) {
  while (n > 0) {
    *destination = *(source + beg);

    destination++;
    source++;
    n--;
  }
  *destination = '\0';
  return destination;
}

static void tty() {
  int interactive;
  pid_t pid;
  char slave_name[20];
  struct termios orig_termios;
  struct winsize size;

  interactive = isatty(STDIN_FILENO);

  if (interactive) { /* fetch current termios and window size */
    if (tcgetattr(STDIN_FILENO, &orig_termios) < 0)
      err_sys("tcgetattr error on stdin");
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, (char *) &size) < 0)
      err_sys("TIOCGWINSZ error");

    size.ws_row = size_rows;
    size.ws_col = size_cols;
    pid = pty_fork(&fdm, slave_name, sizeof(slave_name), &orig_termios, &size);
  } else {
    pid = pty_fork(&fdm, slave_name, sizeof(slave_name), NULL, NULL);
  }

  if (pid < 0) {
    err_sys("fork error");
  } else if (pid == 0) { /* child */
    printf("Run tmux new");
    char *argument_list[] = {"tmux", "new", "-As0", NULL};
    if (execvp(argument_list[0], argument_list) < 0)
      err_sys("can't execute: %s", argument_list);
  }

  if (interactive) {
    if (tty_raw(STDIN_FILENO) < 0) /* user's tty to raw mode */
      err_sys("tty_raw error");
    if (atexit(tty_atexit) < 0) /* reset user's tty on exit */
      err_sys("atexit error");
  }

  // non blocking .... becouse terminal block as (read fun.)
  int oldflags = fcntl(fdm, F_GETFL, 0);
  oldflags |= O_NONBLOCK;
  fcntl(fdm, F_SETFL, oldflags);

  is_tty_over = 0;
}

int main(void) {
  struct mg_mgr mgr;  // Event manager
  mg_mgr_init(&mgr);  // Initialise event manager
  printf("Starting WS listener on %s/websocket\n", s_listen_on);
  mg_http_listen(&mgr, s_listen_on, fn, NULL);  // Create HTTP listener
  for (;;) {
    mg_mgr_poll(&mgr, 10);  // Infinite event loop 10 ms
    if (is_tty_over == 0) {
      nread = read(fdm, buf, BUFFSIZE);

      if (nread == -1) {   // no data
        if (errno == EIO){  // exit linux
          mg_ws_send(con, buf, nread, WEBSOCKET_OP_CLOSE);
          mg_mgr_free(&mgr);
          return 0;
        }
      } else if (nread <= 0) {  // exit mac os
        mg_ws_send(con, buf, nread, WEBSOCKET_OP_CLOSE);
        mg_mgr_free(&mgr);
        return 0;
      } else {
        mg_ws_send(con, buf, nread, WEBSOCKET_OP_TEXT);
      }
    }
  }
  mg_mgr_free(&mgr);
  return 0;
}
