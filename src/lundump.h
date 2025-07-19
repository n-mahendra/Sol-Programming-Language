/*
** $Id: lundump.h $
** load precompiled Sol chunks
** See Copyright Notice in sol.h
*/

#ifndef lundump_h
#define lundump_h

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define SOLC_DATA	"\x19\x93\r\n\x1a\n"

#define SOLC_INT	0x5678
#define SOLC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define SOLC_VERSION  (((SOL_VERSION_NUM / 100) * 16) + SOL_VERSION_NUM % 100)

#define SOLC_FORMAT	0	/* this is the official format */

/* load one chunk; from lundump.c */
SOLI_FUNC LClosure* solU_undump (sol_State* L, ZIO* Z, const char* name);

/* dump one chunk; from ldump.c */
SOLI_FUNC int solU_dump (sol_State* L, const Proto* f, sol_Writer w,
                         void* data, int strip);

#endif
