#include "compat.h"
#include <string.h>

char *gets_s(char *str, size_t n) {
	int l;
	fgets(str, n, stdin);
	l = strlen(str);
	if(l > 0 && str[l-1] == '\n') str[l-1] = 0;
	return str;
}

#ifdef WIN32
int isMSYS() {
	char *msystem = getenv("MSYSTEM");
	if(!msystem) return 0;
	if(!strncmp(msystem, "MINGW", 5)) return 1;
	if(!strncmp(msystem, "MSYS", 4)) return 1;
	return 0;
}

void waitkey(char *argv0) {
	// If we have an absolute path, the second
	// character will be ':'.
	// However, MinGW always uses an
	// absolute path, so we try to detect MinGW
	if (argv0[1] == ':' && !isMSYS()) {
		// Executed by Double-Clicking (or Drap and Drop)
		if (_kbhit())
			_getch();
		_getch();
	}
}
#else
// on unixes, do nothing
void waitkey(char *argv0) {
}
#endif
