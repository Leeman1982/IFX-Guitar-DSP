/* Minimal newlib syscalls (no console): silences the "not implemented"
 * link warnings and gives snprintf a heap. */
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>

extern uint8_t end, _estack;
extern uint32_t _Min_Stack_Size;
static uint8_t *s_brk = &end;

void *_sbrk(int incr) {
    uint8_t *limit = &_estack - (uint32_t)&_Min_Stack_Size;
    if (s_brk + incr > limit) { errno = ENOMEM; return (void *)-1; }
    uint8_t *prev = s_brk;
    s_brk += incr;
    return prev;
}
int _close(int fd) { (void)fd; return -1; }
int _lseek(int fd, int off, int whence) { (void)fd; (void)off; (void)whence; return 0; }
int _read(int fd, char *p, int n) { (void)fd; (void)p; (void)n; return 0; }
int _write(int fd, const char *p, int n) { (void)fd; (void)p; return n; }
int _fstat(int fd, struct stat *st) { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd) { (void)fd; return 1; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int code) { (void)code; for (;;) { } }
