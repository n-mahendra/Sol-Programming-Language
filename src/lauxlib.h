/*
** $Id: lauxlib.h $
** Auxiliary functions for building Sol libraries
** See Copyright Notice in sol.h
*/


#ifndef lauxlib_h
#define lauxlib_h


#include <stddef.h>
#include <stdio.h>

#include "solconf.h"
#include "sol.h"


/* global table */
#define SOL_GNAME	"_G"


typedef struct solL_Buffer solL_Buffer;


/* extra error code for 'solL_loadfilex' */
#define SOL_ERRFILE     (SOL_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define SOL_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define SOL_PRELOAD_TABLE	"_PRELOAD"


typedef struct solL_Reg {
  const char *name;
  sol_CFunction func;
} solL_Reg;


#define SOLL_NUMSIZES	(sizeof(sol_Integer)*16 + sizeof(sol_Number))

SOLLIB_API void (solL_checkversion_) (sol_State *L, sol_Number ver, size_t sz);
#define solL_checkversion(L)  \
	  solL_checkversion_(L, SOL_VERSION_NUM, SOLL_NUMSIZES)

SOLLIB_API int (solL_getmetafield) (sol_State *L, int obj, const char *e);
SOLLIB_API int (solL_callmeta) (sol_State *L, int obj, const char *e);
SOLLIB_API const char *(solL_tolstring) (sol_State *L, int idx, size_t *len);
SOLLIB_API int (solL_argerror) (sol_State *L, int arg, const char *extramsg);
SOLLIB_API int (solL_typeerror) (sol_State *L, int arg, const char *tname);
SOLLIB_API const char *(solL_checklstring) (sol_State *L, int arg,
                                                          size_t *l);
SOLLIB_API const char *(solL_optlstring) (sol_State *L, int arg,
                                          const char *def, size_t *l);
SOLLIB_API sol_Number (solL_checknumber) (sol_State *L, int arg);
SOLLIB_API sol_Number (solL_optnumber) (sol_State *L, int arg, sol_Number def);

SOLLIB_API sol_Integer (solL_checkinteger) (sol_State *L, int arg);
SOLLIB_API sol_Integer (solL_optinteger) (sol_State *L, int arg,
                                          sol_Integer def);

SOLLIB_API void (solL_checkstack) (sol_State *L, int sz, const char *msg);
SOLLIB_API void (solL_checktype) (sol_State *L, int arg, int t);
SOLLIB_API void (solL_checkany) (sol_State *L, int arg);

SOLLIB_API int   (solL_newmetatable) (sol_State *L, const char *tname);
SOLLIB_API void  (solL_setmetatable) (sol_State *L, const char *tname);
SOLLIB_API void *(solL_testudata) (sol_State *L, int ud, const char *tname);
SOLLIB_API void *(solL_checkudata) (sol_State *L, int ud, const char *tname);

SOLLIB_API void (solL_where) (sol_State *L, int lvl);
SOLLIB_API int (solL_error) (sol_State *L, const char *fmt, ...);

SOLLIB_API int (solL_checkoption) (sol_State *L, int arg, const char *def,
                                   const char *const lst[]);

SOLLIB_API int (solL_fileresult) (sol_State *L, int stat, const char *fname);
SOLLIB_API int (solL_execresult) (sol_State *L, int stat);


/* predefined references */
#define SOL_NOREF       (-2)
#define SOL_REFNIL      (-1)

SOLLIB_API int (solL_ref) (sol_State *L, int t);
SOLLIB_API void (solL_unref) (sol_State *L, int t, int ref);

SOLLIB_API int (solL_loadfilex) (sol_State *L, const char *filename,
                                               const char *mode);

#define solL_loadfile(L,f)	solL_loadfilex(L,f,NULL)

SOLLIB_API int (solL_loadbufferx) (sol_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
SOLLIB_API int (solL_loadstring) (sol_State *L, const char *s);

SOLLIB_API sol_State *(solL_newstate) (void);

SOLLIB_API sol_Integer (solL_len) (sol_State *L, int idx);

SOLLIB_API void (solL_addgsub) (solL_Buffer *b, const char *s,
                                     const char *p, const char *r);
SOLLIB_API const char *(solL_gsub) (sol_State *L, const char *s,
                                    const char *p, const char *r);

SOLLIB_API void (solL_setfuncs) (sol_State *L, const solL_Reg *l, int nup);

SOLLIB_API int (solL_getsubtable) (sol_State *L, int idx, const char *fname);

SOLLIB_API void (solL_traceback) (sol_State *L, sol_State *L1,
                                  const char *msg, int level);

SOLLIB_API void (solL_requiref) (sol_State *L, const char *modname,
                                 sol_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define solL_newlibtable(L,l)	\
  sol_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define solL_newlib(L,l)  \
  (solL_checkversion(L), solL_newlibtable(L,l), solL_setfuncs(L,l,0))

#define solL_argcheck(L, cond,arg,extramsg)	\
	((void)(soli_likely(cond) || solL_argerror(L, (arg), (extramsg))))

#define solL_argexpected(L,cond,arg,tname)	\
	((void)(soli_likely(cond) || solL_typeerror(L, (arg), (tname))))

#define solL_checkstring(L,n)	(solL_checklstring(L, (n), NULL))
#define solL_optstring(L,n,d)	(solL_optlstring(L, (n), (d), NULL))

#define solL_typename(L,i)	sol_typename(L, sol_type(L,(i)))

#define solL_dofile(L, fn) \
	(solL_loadfile(L, fn) || sol_pcall(L, 0, SOL_MULTRET, 0))

#define solL_dostring(L, s) \
	(solL_loadstring(L, s) || sol_pcall(L, 0, SOL_MULTRET, 0))

#define solL_getmetatable(L,n)	(sol_getfield(L, SOL_REGISTRYINDEX, (n)))

#define solL_opt(L,f,n,d)	(sol_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define solL_loadbuffer(L,s,sz,n)	solL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on sol_Integer values with wrap-around
** semantics, as the Sol core does.
*/
#define solL_intop(op,v1,v2)  \
	((sol_Integer)((sol_Unsigned)(v1) op (sol_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define solL_pushfail(L)	sol_pushnil(L)


/*
** Internal assertions for in-house debugging
*/
#if !defined(sol_assert)

#if defined SOLI_ASSERT
  #include <assert.h>
  #define sol_assert(c)		assert(c)
#else
  #define sol_assert(c)		((void)0)
#endif

#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct solL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  sol_State *L;
  union {
    SOLI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[SOLL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define solL_bufflen(bf)	((bf)->n)
#define solL_buffaddr(bf)	((bf)->b)


#define solL_addchar(B,c) \
  ((void)((B)->n < (B)->size || solL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define solL_addsize(B,s)	((B)->n += (s))

#define solL_buffsub(B,s)	((B)->n -= (s))

SOLLIB_API void (solL_buffinit) (sol_State *L, solL_Buffer *B);
SOLLIB_API char *(solL_prepbuffsize) (solL_Buffer *B, size_t sz);
SOLLIB_API void (solL_addlstring) (solL_Buffer *B, const char *s, size_t l);
SOLLIB_API void (solL_addstring) (solL_Buffer *B, const char *s);
SOLLIB_API void (solL_addvalue) (solL_Buffer *B);
SOLLIB_API void (solL_pushresult) (solL_Buffer *B);
SOLLIB_API void (solL_pushresultsize) (solL_Buffer *B, size_t sz);
SOLLIB_API char *(solL_buffinitsize) (sol_State *L, solL_Buffer *B, size_t sz);

#define solL_prepbuffer(B)	solL_prepbuffsize(B, SOLL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'SOL_FILEHANDLE' and
** initial structure 'solL_Stream' (it may contain other fields
** after that initial structure).
*/

#define SOL_FILEHANDLE          "FILE*"


typedef struct solL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  sol_CFunction closef;  /* to close stream (NULL for closed streams) */
} solL_Stream;

/* }====================================================== */

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(sol_writestring)
#define sol_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(sol_writeline)
#define sol_writeline()        (sol_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(sol_writestringerror)
#define sol_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(SOL_COMPAT_APIINTCASTS)

#define solL_checkunsigned(L,a)	((sol_Unsigned)solL_checkinteger(L,a))
#define solL_optunsigned(L,a,d)	\
	((sol_Unsigned)solL_optinteger(L,a,(sol_Integer)(d)))

#define solL_checkint(L,n)	((int)solL_checkinteger(L, (n)))
#define solL_optint(L,n,d)	((int)solL_optinteger(L, (n), (d)))

#define solL_checklong(L,n)	((long)solL_checkinteger(L, (n)))
#define solL_optlong(L,n,d)	((long)solL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */



#endif


