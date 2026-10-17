// custom stdbool.h to for 1-byte bool types

#ifndef _CSTM_STDBOOL_H_
#define	_CSTM_STDBOOL_H_

#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
// C++ and C23 already provide bool, true and false.

#define	false	0x00
#define	true	0x01

// the MS VC++ 6 compiler uses a one-byte-type (unsigned char, to be exact), so I'll reproduce this here
typedef	unsigned char	bool;

#endif // pre-C23 C only

#endif // !_CSTM_STDBOOL_H_
