#include "StringUtil.h"

char* concatStrings(const char *path, const char *src) {
	int lenght = strlen(path) + strlen(src) + 1;
	char *result = (char *) malloc(lenght * sizeof(char));
	strcpy(result, path);
	strcat(result, src);
	return result;
}

char* concatStringToPathFiles(const char *src) {
	return concatStrings(TRUE_FILES_PATH, src);
}

char* concatStringToPathBin(const char *src) {
	return concatStrings(TRUE_BIN_PATH, src);
}


