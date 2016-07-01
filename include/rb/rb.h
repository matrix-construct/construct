#ifndef _RB_H
#define _RB_H
#define RB_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include "include.h"
#include "system.h"


typedef socklen_t rb_socklen_t;
typedef void log_cb(const char *buffer);
typedef void restart_cb(const char *buffer);
typedef void die_cb(const char *buffer);

char *rb_ctime(const time_t, char *, size_t);
char *rb_date(const time_t, char *, size_t);
void rb_lib_log(const char *, ...);
void rb_lib_restart(const char *, ...) __attribute__((noreturn));
void rb_lib_die(const char *, ...);
void rb_set_time(void);
const char *rb_lib_version(void);
void rb_lib_init(log_cb * xilog, restart_cb * irestart, die_cb * idie, int closeall, int maxfds,  size_t dh_size, size_t fd_heap_size);
void rb_lib_loop(long delay) __attribute__((noreturn));
time_t rb_current_time(void);
const struct timeval *rb_current_time_tv(void);
pid_t rb_spawn_process(const char *, const char **);
char *rb_strtok_r(char *, const char *, char **);
int rb_gettimeofday(struct timeval *, void *);
void rb_sleep(unsigned int seconds, unsigned int useconds);
char *rb_crypt(const char *, const char *);
unsigned char *rb_base64_encode(const unsigned char *str, int length);
unsigned char *rb_base64_decode(const unsigned char *str, int length, int *ret);
int rb_kill(pid_t, int);
char *rb_strerror(int);
int rb_setenv(const char *, const char *, int);
pid_t rb_waitpid(pid_t pid, int *status, int options);
pid_t rb_getpid(void);
//unsigned int rb_geteuid(void);
void *const *rb_backtrace(int *len);                 // writes to and returns static vector (*len indicates element count)
const char *const *rb_backtrace_symbols(int *len);   // translates rb_backtrace(), all static
void rb_backtrace_log_symbols(void);                 // rb_backtrace_symbols piped to rb_lib_log()


#include "tools.h"
#include "dlink.h"
#include "memory.h"
#include "commio.h"
#include "balloc.h"
#include "linebuf.h"
#include "event.h"
#include "helper.h"
#include "rawbuf.h"
#include "patricia.h"
#include "dictionary.h"
#include "radixtree.h"

#ifdef __cplusplus
}       // extern "C"
#endif

#include "cpp.h"


#endif  // _RB_H
