#include "StringUtil.h"

char* concatStringToPathFiles(char *src) {
	char *path = getenv("TRUE_FILES_PATH");
	int lenght = strlen(path) + strlen(src) + 1;
	char *result = (char *) malloc(lenght * sizeof(char));
	strcpy(result, path);
	strcat(result, src);
	return result;
}


