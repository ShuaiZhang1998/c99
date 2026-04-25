#include <signal.h>
#include <stdlib.h>

static sighandler_t handlers[32];

static int valid_sig(int sig) {
  return sig > 0 && sig < (int)(sizeof(handlers) / sizeof(handlers[0]));
}

void* __c99cc_signal(int sig, sighandler_t handler) {
  if (!valid_sig(sig)) return (void*)-1;
  sighandler_t old = handlers[sig];
  handlers[sig] = handler;
  return (void*)old;
}

int raise(int sig) {
  if (!valid_sig(sig)) return -1;
  sighandler_t h = handlers[sig];
  if (h == SIG_IGN) return 0;
  if (h && h != SIG_DFL) {
    handlers[sig] = SIG_DFL;
    h(sig);
    return 0;
  }
  if (sig == SIGABRT) abort();
  return 0;
}
