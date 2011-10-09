#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

//#include "Definitions.h"

using namespace std;

#ifndef STRINGUTIL_H_
#define STRINGUTIL_H_

// Se alterar aqui alterar também o MAKEFILE
#define TRUE_FILES_PATH "/usr/share/true/"
#define TRUE_BIN_PATH "/usr/bin/"

char* concatStrings(const char *path, const char *src);

char* concatStringToPathFiles(const char *src);

char* concatStringToPathBin(const char *src);

const char* replaceInString(char *src, const char *s1, const char *s2);

#endif /* STRINGUTIL_H_ */
