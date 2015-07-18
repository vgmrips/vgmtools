/* Include this file before <stdio.h> */

// VC6
#if _MSC_VER == 1200
// emulate stdint
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

// printf() formats
#define FMT_UINT64 "%I64u"
#define FMT_INT64 "%I64d"
#define FMT_XINT64 "%I64X"

// _getch()
#include <conio.h>

#else
// integer types
#include <stdint.h>

// printf() formats
#define FMT_UINT64 "%lu"
#define FMT_INT64 "%ld"
#define FMT_XINT64 "%lX"
#endif

#ifdef __STDC_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__
#include <stdio.h>
#else
#include <stdio.h>
#include <stdlib.h>
char *gets_s(char *str, size_t n);
#endif

#ifdef WIN32
#else
#define wcsicmp wcscasecmp
int _getch();
#endif

#ifdef _MSC_VER
#define strdup _strdup
#else
#define _getch getchar
#define _stricmp strcasecmp
#define stricmp strcasecmp
#endif