#include "KeyboardUtil.h"

/**
 * Implementa a função kbhit padrão da biblioteca conio.h que não existe no Mac OS X
 */
int kbhit(){
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}