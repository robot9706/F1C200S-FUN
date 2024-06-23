#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/unistd.h>

#undef errno
extern int errno;

extern void putchar_(char c);

int _close(int file) {
    return -1;
}

int _fstat(int file, struct stat* st) {
    return 0;
}

int _isatty(int file) {
    return 0;
}

int _lseek(int file, int ptr, int dir) {
    return 0;
}

int _getpid(void) {
    return 1;
}

int _kill(int pid, int sig) {
    errno = EINVAL;
    return -1;
}

void _exit(int status) {
    _kill(status, -1);
    while(1) {
    }
}

caddr_t _sbrk_r(struct _reent* r, int incr) {
    errno = ENOMEM;
    return (void*)-1;
}

int _read(int file, char* ptr, int len) {
    return 0;
}

int _write(int file, char* ptr, int len) {
    for(uint32_t i = 0; i < len; i++) {
        if(ptr[i] == '\n') {
            putchar_('\r');
        }
        putchar_(ptr[i]);
    }
    return len;
}
