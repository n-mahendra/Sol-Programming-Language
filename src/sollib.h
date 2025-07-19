/*
** $Id: sollib.h $
** Sol standard libraries
** See Copyright Notice in sol.h
*/


#ifndef sollib_h
#define sollib_h

#include "sol.h"


/* version suffix for environment variable names */
#define SOL_VERSUFFIX          "_" SOL_VERSION_MAJOR "_" SOL_VERSION_MINOR


SOLMOD_API int (solopen_base) (sol_State *L);

#define SOL_COLIBNAME	"coroutine"
SOLMOD_API int (solopen_coroutine) (sol_State *L);

#define SOL_TABLIBNAME	"table"
SOLMOD_API int (solopen_table) (sol_State *L);

#define SOL_IOLIBNAME	"io"
SOLMOD_API int (solopen_io) (sol_State *L);

#define SOL_OSLIBNAME	"os"
SOLMOD_API int (solopen_os) (sol_State *L);

#define SOL_STRLIBNAME	"string"
SOLMOD_API int (solopen_string) (sol_State *L);

#define SOL_UTF8LIBNAME	"utf8"
SOLMOD_API int (solopen_utf8) (sol_State *L);

#define SOL_MATHLIBNAME	"math"
SOLMOD_API int (solopen_math) (sol_State *L);

#define SOL_DBLIBNAME	"debug"
SOLMOD_API int (solopen_debug) (sol_State *L);

#define SOL_LOADLIBNAME	"package"
SOLMOD_API int (solopen_package) (sol_State *L);


/* open all previous libraries */
SOLLIB_API void (solL_openlibs) (sol_State *L);


#endif
