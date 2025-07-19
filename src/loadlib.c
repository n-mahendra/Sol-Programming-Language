/*
** $Id: loadlib.c $
** Dynamic library loader for Sol
** See Copyright Notice in sol.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define SOL_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sol.h"

#include "lauxlib.h"
#include "sollib.h"


/*
** SOL_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** SOL_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Sol loader.
*/
#if !defined(SOL_CSUBSEP)
#define SOL_CSUBSEP		SOL_DIRSEP
#endif

#if !defined(SOL_LSUBSEP)
#define SOL_LSUBSEP		SOL_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define SOL_POF		"solopen_"

/* separator for open functions in C libraries */
#define SOL_OFSEP	"_"


/*
** key for table in the registry that keeps handles
** for all loaded C libraries
*/
static const char *const CLIBS = "_CLIBS";

#define LIB_FAIL	"open"


#define setprogdir(L)           ((void)0)


/*
** Special type equivalent to '(void*)' for functions in gcc
** (to suppress warnings when converting function pointers)
*/
typedef void (*voidf)(void);


/*
** system-dependent functions
*/

/*
** unload library 'lib'
*/
static void lsys_unloadlib (void *lib);

/*
** load C library in file 'path'. If 'seeglb', load with all names in
** the library global.
** Returns the library; in case of error, returns NULL plus an
** error string in the stack.
*/
static void *lsys_load (sol_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static sol_CFunction lsys_sym (sol_State *L, void *lib, const char *sym);




#if defined(SOL_USE_DLOPEN)	/* { */
/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface.
** The dlfcn interface is available in Linux, SunOS, Solaris, IRIX, FreeBSD,
** NetBSD, AIX 4.2, HPUX 11, and  probably most other Unix flavors, at least
** as an emulation layer on top of native functions.
** =========================================================================
*/

#include <dlfcn.h>

/*
** Macro to convert pointer-to-void* to pointer-to-function. This cast
** is undefined according to ISO C, but POSIX assumes that it works.
** (The '__extension__' in gnu compilers is only to avoid warnings.)
*/
#if defined(__GNUC__)
#define cast_func(p) (__extension__ (sol_CFunction)(p))
#else
#define cast_func(p) ((sol_CFunction)(p))
#endif


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (sol_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    sol_pushstring(L, dlerror());
  return lib;
}


static sol_CFunction lsys_sym (sol_State *L, void *lib, const char *sym) {
  sol_CFunction f = cast_func(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    sol_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(SOL_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(SOL_LLE_FLAGS)
#define SOL_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of SOL_EXEC_DIR with the executable's path.
*/
static void setprogdir (sol_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    solL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    solL_gsub(L, sol_tostring(L, -1), SOL_EXEC_DIR, buff);
    sol_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (sol_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    sol_pushstring(L, buffer);
  else
    sol_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (sol_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, SOL_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static sol_CFunction lsys_sym (sol_State *L, void *lib, const char *sym) {
  sol_CFunction f = (sol_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
  if (f == NULL) pusherror(L);
  return f;
}

/* }====================================================== */


#else				/* }{ */
/*
** {======================================================
** Fallback for other systems
** =======================================================
*/

#undef LIB_FAIL
#define LIB_FAIL	"absent"


#define DLMSG	"dynamic libraries not enabled; check your Sol installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (sol_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  sol_pushliteral(L, DLMSG);
  return NULL;
}


static sol_CFunction lsys_sym (sol_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  sol_pushliteral(L, DLMSG);
  return NULL;
}

/* }====================================================== */
#endif				/* } */


/*
** {==================================================================
** Set Paths
** ===================================================================
*/

/*
** SOL_PATH_VAR and SOL_CPATH_VAR are the names of the environment
** variables that Sol check to set its paths.
*/
#if !defined(SOL_PATH_VAR)
#define SOL_PATH_VAR    "SOL_PATH"
#endif

#if !defined(SOL_CPATH_VAR)
#define SOL_CPATH_VAR   "SOL_CPATH"
#endif



/*
** return registry.SOL_NOENV as a boolean
*/
static int noenv (sol_State *L) {
  int b;
  sol_getfield(L, SOL_REGISTRYINDEX, "SOL_NOENV");
  b = sol_toboolean(L, -1);
  sol_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path
*/
static void setpath (sol_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = sol_pushfstring(L, "%s%s", envname, SOL_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    sol_pushstring(L, dft);  /* use default */
  else if ((dftmark = strstr(path, SOL_PATH_SEP SOL_PATH_SEP)) == NULL)
    sol_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    solL_Buffer b;
    solL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      solL_addlstring(&b, path, dftmark - path);  /* add it */
      solL_addchar(&b, *SOL_PATH_SEP);
    }
    solL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      solL_addchar(&b, *SOL_PATH_SEP);
      solL_addlstring(&b, dftmark + 2, (path + len - 2) - dftmark);
    }
    solL_pushresult(&b);
  }
  setprogdir(L);
  sol_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  sol_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (sol_State *L, const char *path) {
  void *plib;
  sol_getfield(L, SOL_REGISTRYINDEX, CLIBS);
  sol_getfield(L, -1, path);
  plib = sol_touserdata(L, -1);  /* plib = CLIBS[path] */
  sol_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (sol_State *L, const char *path, void *plib) {
  sol_getfield(L, SOL_REGISTRYINDEX, CLIBS);
  sol_pushlightuserdata(L, plib);
  sol_pushvalue(L, -1);
  sol_setfield(L, -3, path);  /* CLIBS[path] = plib */
  sol_rawseti(L, -2, solL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  sol_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (sol_State *L) {
  sol_Integer n = solL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    sol_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(sol_touserdata(L, -1));
    sol_pop(L, 1);  /* pop handle */
  }
  return 0;
}



/* error codes for 'lookforfunc' */
#define ERRLIB		1
#define ERRFUNC		2

/*
** Look for a C function named 'sym' in a dynamically loaded library
** 'path'.
** First, check whether the library is already loaded; if not, try
** to load it.
** Then, if 'sym' is '*', return true (as library has been loaded).
** Otherwise, look for symbol 'sym' in the library and push a
** C function with that symbol.
** Return 0 and 'true' or a function in the stack; in case of
** errors, return an error code and an error message in the stack.
*/
static int lookforfunc (sol_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    sol_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    sol_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    sol_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (sol_State *L) {
  const char *path = solL_checkstring(L, 1);
  const char *init = solL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    solL_pushfail(L);
    sol_insert(L, -2);
    sol_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
    return 3;  /* return fail, error message, and where */
  }
}



/*
** {======================================================
** 'require' function
** =======================================================
*/


static int readable (const char *filename) {
  FILE *f = fopen(filename, "r");  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}


/*
** Get the next name in '*path' = 'name1;name2;name3;...', changing
** the ending ';' to '\0' to create a zero-terminated string. Return
** NULL when list ends.
*/
static const char *getnextfilename (char **path, char *end) {
  char *sep;
  char *name = *path;
  if (name == end)
    return NULL;  /* no more names */
  else if (*name == '\0') {  /* from previous iteration? */
    *name = *SOL_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *SOL_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name goes until the end */
  *sep = '\0';  /* finish file name */
  *path = sep;  /* will start next search from here */
  return name;
}


/*
** Given a path such as ";blabla.so;blublu.so", pushes the string
**
** no file 'blabla.so'
**	no file 'blublu.so'
*/
static void pusherrornotfound (sol_State *L, const char *path) {
  solL_Buffer b;
  solL_buffinit(L, &b);
  solL_addstring(&b, "no file '");
  solL_addgsub(&b, path, SOL_PATH_SEP, "'\n\tno file '");
  solL_addstring(&b, "'");
  solL_pushresult(&b);
}


static const char *searchpath (sol_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  solL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = solL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  solL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  solL_addgsub(&buff, path, SOL_PATH_MARK, name);
  solL_addchar(&buff, '\0');
  pathname = solL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + solL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return sol_pushstring(L, filename);  /* save and return name */
  }
  solL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, sol_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (sol_State *L) {
  const char *f = searchpath(L, solL_checkstring(L, 1),
                                solL_checkstring(L, 2),
                                solL_optstring(L, 3, "."),
                                solL_optstring(L, 4, SOL_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    solL_pushfail(L);
    sol_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (sol_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  sol_getfield(L, sol_upvalueindex(1), pname);
  path = sol_tostring(L, -1);
  if (l_unlikely(path == NULL))
    solL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (sol_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    sol_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return solL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          sol_tostring(L, 1), filename, sol_tostring(L, -1));
}


static int searcher_Sol (sol_State *L) {
  const char *filename;
  const char *name = solL_checkstring(L, 1);
  filename = findfile(L, name, "path", SOL_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (solL_loadfile(L, filename) == SOL_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "solopen_X" and look for it. (For compatibility, if that
** fails, it also tries "solopen_Y".) If there is no ignore mark,
** look for a function named "solopen_modname".
*/
static int loadfunc (sol_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = solL_gsub(L, modname, ".", SOL_OFSEP);
  mark = strchr(modname, *SOL_IGMARK);
  if (mark) {
    int stat;
    openfunc = sol_pushlstring(L, modname, mark - modname);
    openfunc = sol_pushfstring(L, SOL_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else go ahead and try old-style name */
  }
  openfunc = sol_pushfstring(L, SOL_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (sol_State *L) {
  const char *name = solL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", SOL_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (sol_State *L) {
  const char *filename;
  const char *name = solL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  sol_pushlstring(L, name, p - name);
  filename = findfile(L, sol_tostring(L, -1), "cpath", SOL_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      sol_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  sol_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (sol_State *L) {
  const char *name = solL_checkstring(L, 1);
  sol_getfield(L, SOL_REGISTRYINDEX, SOL_PRELOAD_TABLE);
  if (sol_getfield(L, -1, name) == SOL_TNIL) {  /* not found? */
    sol_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    sol_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (sol_State *L, const char *name) {
  int i;
  solL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(sol_getfield(L, sol_upvalueindex(1), "searchers")
                 != SOL_TTABLE))
    solL_error(L, "'package.searchers' must be a table");
  solL_buffinit(L, &msg);
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    solL_addstring(&msg, "\n\t");  /* error-message prefix */
    if (l_unlikely(sol_rawgeti(L, 3, i) == SOL_TNIL)) {  /* no more searchers? */
      sol_pop(L, 1);  /* remove nil */
      solL_buffsub(&msg, 2);  /* remove prefix */
      solL_pushresult(&msg);  /* create error message */
      solL_error(L, "module '%s' not found:%s", name, sol_tostring(L, -1));
    }
    sol_pushstring(L, name);
    sol_call(L, 1, 2);  /* call it */
    if (sol_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (sol_isstring(L, -2)) {  /* searcher returned error message? */
      sol_pop(L, 1);  /* remove extra return */
      solL_addvalue(&msg);  /* concatenate error message */
    }
    else {  /* no error message */
      sol_pop(L, 2);  /* remove both returns */
      solL_buffsub(&msg, 2);  /* remove prefix */
    }
  }
}


static int ll_require (sol_State *L) {
  const char *name = solL_checkstring(L, 1);
  sol_settop(L, 1);  /* LOADED table will be at index 2 */
  sol_getfield(L, SOL_REGISTRYINDEX, SOL_LOADED_TABLE);
  sol_getfield(L, 2, name);  /* LOADED[name] */
  if (sol_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  sol_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  sol_rotate(L, -2, 1);  /* function <-> loader data */
  sol_pushvalue(L, 1);  /* name is 1st argument to module loader */
  sol_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  sol_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!sol_isnil(L, -1))  /* non-nil return? */
    sol_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    sol_pop(L, 1);  /* pop nil */
  if (sol_getfield(L, 2, name) == SOL_TNIL) {   /* module set no value? */
    sol_pushboolean(L, 1);  /* use true as result */
    sol_copy(L, -1, -2);  /* replace loader result */
    sol_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  sol_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const solL_Reg pk_funcs[] = {
  {"loadlib", ll_loadlib},
  {"searchpath", ll_searchpath},
  /* placeholders */
  {"preload", NULL},
  {"cpath", NULL},
  {"path", NULL},
  {"searchers", NULL},
  {"loaded", NULL},
  {NULL, NULL}
};


static const solL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (sol_State *L) {
  static const sol_CFunction searchers[] = {
    searcher_preload,
    searcher_Sol,
    searcher_C,
    searcher_Croot,
    NULL
  };
  int i;
  /* create 'searchers' table */
  sol_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    sol_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    sol_pushcclosure(L, searchers[i], 1);
    sol_rawseti(L, -2, i+1);
  }
  sol_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (sol_State *L) {
  solL_getsubtable(L, SOL_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  sol_createtable(L, 0, 1);  /* create metatable for CLIBS */
  sol_pushcfunction(L, gctm);
  sol_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  sol_setmetatable(L, -2);
}


SOLMOD_API int solopen_package (sol_State *L) {
  createclibstable(L);
  solL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", SOL_PATH_VAR, SOL_PATH_DEFAULT);
  setpath(L, "cpath", SOL_CPATH_VAR, SOL_CPATH_DEFAULT);
  /* store config information */
  sol_pushliteral(L, SOL_DIRSEP "\n" SOL_PATH_SEP "\n" SOL_PATH_MARK "\n"
                     SOL_EXEC_DIR "\n" SOL_IGMARK "\n");
  sol_setfield(L, -2, "config");
  /* set field 'loaded' */
  solL_getsubtable(L, SOL_REGISTRYINDEX, SOL_LOADED_TABLE);
  sol_setfield(L, -2, "loaded");
  /* set field 'preload' */
  solL_getsubtable(L, SOL_REGISTRYINDEX, SOL_PRELOAD_TABLE);
  sol_setfield(L, -2, "preload");
  sol_pushglobaltable(L);
  sol_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  solL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  sol_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

