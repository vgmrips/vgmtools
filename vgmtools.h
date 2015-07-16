#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include "stdbool.h"
#include <string.h>
#include <limits.h>

#ifdef WIN32
#include <conio.h>
#include <windows.h>	// for GetTickCount
#else
#include <ctype.h>
#endif

#include "zlib.h"

#include "stdtype.h"
#include "VGMFile.h"

#ifndef INLINE
#define INLINE __inline
#endif

#define MAX_PATH PATH_MAX