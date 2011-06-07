#ifndef KEYBOARD_UTIL_H_
#define KEYBOARD_UTIL_H_

#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>

/**
 * Tecla ESC
 */
const int VK_ESCAPE = 27; 

/**
 * Implementa a função kbhit padrão da biblioteca conio.h que não existe no Mac OS X
 */
int kbhit();

#endif 
