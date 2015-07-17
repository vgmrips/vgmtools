#include "compat.h"
#include <string.h>

char *gets_s(char *str, size_t n) {
	int l;
	fgets(str, n, stdin);
	l = strlen(str);
	if(l > 0 && str[l-1] == '\n') str[l-1] = 0;
	return str;
}
