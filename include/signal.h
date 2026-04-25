#ifndef C99CC_SIGNAL_H
#define C99CC_SIGNAL_H

typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)(void*)-1)

#define SIGABRT 6
#define SIGINT 2
#define SIGTERM 15

void* __c99cc_signal(int sig, sighandler_t handler);
#define signal(sig, handler) ((sighandler_t)__c99cc_signal((sig), (handler)))
int raise(int sig);

#endif
