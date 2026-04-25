// ARGS: -I include
// EXPECT: 0
#include <signal.h>

static int seen = 0;

static void on_signal(int sig) {
  seen = sig;
}

int main() {
  if (signal(SIGINT, on_signal) != SIG_DFL) return 1;
  if (raise(SIGINT) != 0) return 2;
  if (seen != SIGINT) return 3;

  if (signal(SIGTERM, SIG_IGN) != SIG_DFL) return 4;
  if (raise(SIGTERM) != 0) return 5;

  if (raise(99) == 0) return 6;

  return 0;
}
