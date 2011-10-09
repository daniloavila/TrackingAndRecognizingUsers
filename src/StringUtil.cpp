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

const char* replaceInString(char *src, const char *s1, const char *s2) {
	if(src == NULL || strlen(src) == 0 || s1 == NULL || strlen(s1) == 0 || s2 == NULL || strlen(s2) == 0)
		return src;

	string str(src);

	int pos1 = str.find(s1);

	if(pos1 <= 0)
		return src;

	str.replace(pos1, strlen(s1), s2);

	return str.c_str();
}

