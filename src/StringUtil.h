#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

//#include "Definitions.h"

#ifndef STRINGUTIL_H_
#define STRINGUTIL_H_

#define TRUE_FILES_PATH "/usr/share/true/"

#define TRUE_BIN_PATH "/usr/bin/"

char* concatStrings(const char *path, const char *src);

char* concatStringToPathFiles(const char *src);

char* concatStringToPathBin(const char *src);

#endif /* STRINGUTIL_H_ */
