/*
** $Id: linit.c $
** Initialization of libraries for sol.c and other clients
** See Copyright Notice in sol.h
*/


#define linit_c
#define SOL_LIB

/*
** If you embed Sol in your program and need to open the standard
** libraries, call solL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  solL_getsubtable(L, SOL_REGISTRYINDEX, SOL_PRELOAD_TABLE);
**  sol_pushcfunction(L, solopen_modname);
**  sol_setfield(L, -2, modname);
**  sol_pop(L, 1);  // remove PRELOAD table
*/

#include "lprefix.h"


#include <stddef.h>

#include "sol.h"

#include "sollib.h"
#include "lauxlib.h"


/*
** these libs are loaded by sol.c and are readily available to any Sol
** program
*/
static const solL_Reg loadedlibs[] = {
  {SOL_GNAME, solopen_base},
  {SOL_LOADLIBNAME, solopen_package},
  {SOL_COLIBNAME, solopen_coroutine},
  {SOL_TABLIBNAME, solopen_table},
  {SOL_IOLIBNAME, solopen_io},
  {SOL_OSLIBNAME, solopen_os},
  {SOL_STRLIBNAME, solopen_string},
  {SOL_MATHLIBNAME, solopen_math},
  {SOL_UTF8LIBNAME, solopen_utf8},
  {SOL_DBLIBNAME, solopen_debug},
  {NULL, NULL}
};


SOLLIB_API void solL_openlibs (sol_State *L) {
  const solL_Reg *lib;
  /* "require" functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    solL_requiref(L, lib->name, lib->func, 1);
    sol_pop(L, 1);  /* remove lib */
  }
}

