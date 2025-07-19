/*
** $Id: sol.h $
** Sol - A Scripting Language
** Sol.org, PUC-Rio, Brazil (http://www.sol.org)
** See Copyright Notice at the end of this file
*/


#ifndef sol_h
#define sol_h

#include <stdarg.h>
#include <stddef.h>


#include "solconf.h"


#define SOL_VERSION_MAJOR	"25"
#define SOL_VERSION_MINOR	"7"
#define SOL_VERSION_RELEASE	"1"

#define SOL_VERSION_NUM			504
#define SOL_VERSION_RELEASE_NUM		(SOL_VERSION_NUM * 100 + 8)

#define SOL_VERSION	"Sol " SOL_VERSION_MAJOR "." SOL_VERSION_MINOR
#define SOL_RELEASE	SOL_VERSION "." SOL_VERSION_RELEASE
#define SOL_COPYRIGHT	SOL_RELEASE "  Copyright (C) 2025 mahendra.uk"
#define SOL_AUTHORS	"me@mahendra.uk"


/* mark for precompiled code ('<esc>Sol') */
#define SOL_SIGNATURE	"\x1bSol"

/* option for multiple returns in 'sol_pcall' and 'sol_call' */
#define SOL_MULTRET	(-1)


/*
** Pseudo-indices
** (-SOLI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define SOL_REGISTRYINDEX	(-SOLI_MAXSTACK - 1000)
#define sol_upvalueindex(i)	(SOL_REGISTRYINDEX - (i))


/* thread status */
#define SOL_OK		0
#define SOL_YIELD	1
#define SOL_ERRRUN	2
#define SOL_ERRSYNTAX	3
#define SOL_ERRMEM	4
#define SOL_ERRERR	5


typedef struct sol_State sol_State;


/*
** basic types
*/
#define SOL_TNONE		(-1)

#define SOL_TNIL		0
#define SOL_TBOOLEAN		1
#define SOL_TLIGHTUSERDATA	2
#define SOL_TNUMBER		3
#define SOL_TSTRING		4
#define SOL_TTABLE		5
#define SOL_TFUNCTION		6
#define SOL_TUSERDATA		7
#define SOL_TTHREAD		8

#define SOL_NUMTYPES		9



/* minimum Sol stack available to a C function */
#define SOL_MINSTACK	20


/* predefined values in the registry */
#define SOL_RIDX_MAINTHREAD	1
#define SOL_RIDX_GLOBALS	2
#define SOL_RIDX_LAST		SOL_RIDX_GLOBALS


/* type of numbers in Sol */
typedef SOL_NUMBER sol_Number;


/* type for integer functions */
typedef SOL_INTEGER sol_Integer;

/* unsigned integer type */
typedef SOL_UNSIGNED sol_Unsigned;

/* type for continuation-function contexts */
typedef SOL_KCONTEXT sol_KContext;


/*
** Type for C functions registered with Sol
*/
typedef int (*sol_CFunction) (sol_State *L);

/*
** Type for continuation functions
*/
typedef int (*sol_KFunction) (sol_State *L, int status, sol_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Sol chunks
*/
typedef const char * (*sol_Reader) (sol_State *L, void *ud, size_t *sz);

typedef int (*sol_Writer) (sol_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*sol_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*sol_WarnFunction) (void *ud, const char *msg, int tocont);


/*
** Type used by the debug API to collect debug information
*/
typedef struct sol_Debug sol_Debug;


/*
** Functions to be called by the debugger in specific events
*/
typedef void (*sol_Hook) (sol_State *L, sol_Debug *ar);


/*
** generic extra include file
*/
#if defined(SOL_USER_H)
#include SOL_USER_H
#endif


/*
** RCS ident string
*/
extern const char sol_ident[];


/*
** state manipulation
*/
SOL_API sol_State *(sol_newstate) (sol_Alloc f, void *ud);
SOL_API void       (sol_close) (sol_State *L);
SOL_API sol_State *(sol_newthread) (sol_State *L);
SOL_API int        (sol_closethread) (sol_State *L, sol_State *from);
SOL_API int        (sol_resetthread) (sol_State *L);  /* Deprecated! */

SOL_API sol_CFunction (sol_atpanic) (sol_State *L, sol_CFunction panicf);


SOL_API sol_Number (sol_version) (sol_State *L);


/*
** basic stack manipulation
*/
SOL_API int   (sol_absindex) (sol_State *L, int idx);
SOL_API int   (sol_gettop) (sol_State *L);
SOL_API void  (sol_settop) (sol_State *L, int idx);
SOL_API void  (sol_pushvalue) (sol_State *L, int idx);
SOL_API void  (sol_rotate) (sol_State *L, int idx, int n);
SOL_API void  (sol_copy) (sol_State *L, int fromidx, int toidx);
SOL_API int   (sol_checkstack) (sol_State *L, int n);

SOL_API void  (sol_xmove) (sol_State *from, sol_State *to, int n);


/*
** access functions (stack -> C)
*/

SOL_API int             (sol_isnumber) (sol_State *L, int idx);
SOL_API int             (sol_isstring) (sol_State *L, int idx);
SOL_API int             (sol_iscfunction) (sol_State *L, int idx);
SOL_API int             (sol_isinteger) (sol_State *L, int idx);
SOL_API int             (sol_isuserdata) (sol_State *L, int idx);
SOL_API int             (sol_type) (sol_State *L, int idx);
SOL_API const char     *(sol_typename) (sol_State *L, int tp);

SOL_API sol_Number      (sol_tonumberx) (sol_State *L, int idx, int *isnum);
SOL_API sol_Integer     (sol_tointegerx) (sol_State *L, int idx, int *isnum);
SOL_API int             (sol_toboolean) (sol_State *L, int idx);
SOL_API const char     *(sol_tolstring) (sol_State *L, int idx, size_t *len);
SOL_API sol_Unsigned    (sol_rawlen) (sol_State *L, int idx);
SOL_API sol_CFunction   (sol_tocfunction) (sol_State *L, int idx);
SOL_API void	       *(sol_touserdata) (sol_State *L, int idx);
SOL_API sol_State      *(sol_tothread) (sol_State *L, int idx);
SOL_API const void     *(sol_topointer) (sol_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define SOL_OPADD	0	/* ORDER TM, ORDER OP */
#define SOL_OPSUB	1
#define SOL_OPMUL	2
#define SOL_OPMOD	3
#define SOL_OPPOW	4
#define SOL_OPDIV	5
#define SOL_OPIDIV	6
#define SOL_OPBAND	7
#define SOL_OPBOR	8
#define SOL_OPBXOR	9
#define SOL_OPSHL	10
#define SOL_OPSHR	11
#define SOL_OPUNM	12
#define SOL_OPBNOT	13

SOL_API void  (sol_arith) (sol_State *L, int op);

#define SOL_OPEQ	0
#define SOL_OPLT	1
#define SOL_OPLE	2

SOL_API int   (sol_rawequal) (sol_State *L, int idx1, int idx2);
SOL_API int   (sol_compare) (sol_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
SOL_API void        (sol_pushnil) (sol_State *L);
SOL_API void        (sol_pushnumber) (sol_State *L, sol_Number n);
SOL_API void        (sol_pushinteger) (sol_State *L, sol_Integer n);
SOL_API const char *(sol_pushlstring) (sol_State *L, const char *s, size_t len);
SOL_API const char *(sol_pushstring) (sol_State *L, const char *s);
SOL_API const char *(sol_pushvfstring) (sol_State *L, const char *fmt,
                                                      va_list argp);
SOL_API const char *(sol_pushfstring) (sol_State *L, const char *fmt, ...);
SOL_API void  (sol_pushcclosure) (sol_State *L, sol_CFunction fn, int n);
SOL_API void  (sol_pushboolean) (sol_State *L, int b);
SOL_API void  (sol_pushlightuserdata) (sol_State *L, void *p);
SOL_API int   (sol_pushthread) (sol_State *L);


/*
** get functions (Sol -> stack)
*/
SOL_API int (sol_getglobal) (sol_State *L, const char *name);
SOL_API int (sol_gettable) (sol_State *L, int idx);
SOL_API int (sol_getfield) (sol_State *L, int idx, const char *k);
SOL_API int (sol_geti) (sol_State *L, int idx, sol_Integer n);
SOL_API int (sol_rawget) (sol_State *L, int idx);
SOL_API int (sol_rawgeti) (sol_State *L, int idx, sol_Integer n);
SOL_API int (sol_rawgetp) (sol_State *L, int idx, const void *p);

SOL_API void  (sol_createtable) (sol_State *L, int narr, int nrec);
SOL_API void *(sol_newuserdatauv) (sol_State *L, size_t sz, int nuvalue);
SOL_API int   (sol_getmetatable) (sol_State *L, int objindex);
SOL_API int  (sol_getiuservalue) (sol_State *L, int idx, int n);


/*
** set functions (stack -> Sol)
*/
SOL_API void  (sol_setglobal) (sol_State *L, const char *name);
SOL_API void  (sol_settable) (sol_State *L, int idx);
SOL_API void  (sol_setfield) (sol_State *L, int idx, const char *k);
SOL_API void  (sol_seti) (sol_State *L, int idx, sol_Integer n);
SOL_API void  (sol_rawset) (sol_State *L, int idx);
SOL_API void  (sol_rawseti) (sol_State *L, int idx, sol_Integer n);
SOL_API void  (sol_rawsetp) (sol_State *L, int idx, const void *p);
SOL_API int   (sol_setmetatable) (sol_State *L, int objindex);
SOL_API int   (sol_setiuservalue) (sol_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Sol code)
*/
SOL_API void  (sol_callk) (sol_State *L, int nargs, int nresults,
                           sol_KContext ctx, sol_KFunction k);
#define sol_call(L,n,r)		sol_callk(L, (n), (r), 0, NULL)

SOL_API int   (sol_pcallk) (sol_State *L, int nargs, int nresults, int errfunc,
                            sol_KContext ctx, sol_KFunction k);
#define sol_pcall(L,n,r,f)	sol_pcallk(L, (n), (r), (f), 0, NULL)

SOL_API int   (sol_load) (sol_State *L, sol_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

SOL_API int (sol_dump) (sol_State *L, sol_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
SOL_API int  (sol_yieldk)     (sol_State *L, int nresults, sol_KContext ctx,
                               sol_KFunction k);
SOL_API int  (sol_resume)     (sol_State *L, sol_State *from, int narg,
                               int *nres);
SOL_API int  (sol_status)     (sol_State *L);
SOL_API int (sol_isyieldable) (sol_State *L);

#define sol_yield(L,n)		sol_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
SOL_API void (sol_setwarnf) (sol_State *L, sol_WarnFunction f, void *ud);
SOL_API void (sol_warning)  (sol_State *L, const char *msg, int tocont);


/*
** garbage-collection function and options
*/

#define SOL_GCSTOP		0
#define SOL_GCRESTART		1
#define SOL_GCCOLLECT		2
#define SOL_GCCOUNT		3
#define SOL_GCCOUNTB		4
#define SOL_GCSTEP		5
#define SOL_GCSETPAUSE		6
#define SOL_GCSETSTEPMUL	7
#define SOL_GCISRUNNING		9
#define SOL_GCGEN		10
#define SOL_GCINC		11

SOL_API int (sol_gc) (sol_State *L, int what, ...);


/*
** miscellaneous functions
*/

SOL_API int   (sol_error) (sol_State *L);

SOL_API int   (sol_next) (sol_State *L, int idx);

SOL_API void  (sol_concat) (sol_State *L, int n);
SOL_API void  (sol_len)    (sol_State *L, int idx);

SOL_API size_t   (sol_stringtonumber) (sol_State *L, const char *s);

SOL_API sol_Alloc (sol_getallocf) (sol_State *L, void **ud);
SOL_API void      (sol_setallocf) (sol_State *L, sol_Alloc f, void *ud);

SOL_API void (sol_toclose) (sol_State *L, int idx);
SOL_API void (sol_closeslot) (sol_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define sol_getextraspace(L)	((void *)((char *)(L) - SOL_EXTRASPACE))

#define sol_tonumber(L,i)	sol_tonumberx(L,(i),NULL)
#define sol_tointeger(L,i)	sol_tointegerx(L,(i),NULL)

#define sol_pop(L,n)		sol_settop(L, -(n)-1)

#define sol_newtable(L)		sol_createtable(L, 0, 0)

#define sol_register(L,n,f) (sol_pushcfunction(L, (f)), sol_setglobal(L, (n)))

#define sol_pushcfunction(L,f)	sol_pushcclosure(L, (f), 0)

#define sol_isfunction(L,n)	(sol_type(L, (n)) == SOL_TFUNCTION)
#define sol_istable(L,n)	(sol_type(L, (n)) == SOL_TTABLE)
#define sol_islightuserdata(L,n)	(sol_type(L, (n)) == SOL_TLIGHTUSERDATA)
#define sol_isnil(L,n)		(sol_type(L, (n)) == SOL_TNIL)
#define sol_isboolean(L,n)	(sol_type(L, (n)) == SOL_TBOOLEAN)
#define sol_isthread(L,n)	(sol_type(L, (n)) == SOL_TTHREAD)
#define sol_isnone(L,n)		(sol_type(L, (n)) == SOL_TNONE)
#define sol_isnoneornil(L, n)	(sol_type(L, (n)) <= 0)

#define sol_pushliteral(L, s)	sol_pushstring(L, "" s)

#define sol_pushglobaltable(L)  \
	((void)sol_rawgeti(L, SOL_REGISTRYINDEX, SOL_RIDX_GLOBALS))

#define sol_tostring(L,i)	sol_tolstring(L, (i), NULL)


#define sol_insert(L,idx)	sol_rotate(L, (idx), 1)

#define sol_remove(L,idx)	(sol_rotate(L, (idx), -1), sol_pop(L, 1))

#define sol_replace(L,idx)	(sol_copy(L, -1, (idx)), sol_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(SOL_COMPAT_APIINTCASTS)

#define sol_pushunsigned(L,n)	sol_pushinteger(L, (sol_Integer)(n))
#define sol_tounsignedx(L,i,is)	((sol_Unsigned)sol_tointegerx(L,i,is))
#define sol_tounsigned(L,i)	sol_tounsignedx(L,(i),NULL)

#endif

#define sol_newuserdata(L,s)	sol_newuserdatauv(L,s,1)
#define sol_getuservalue(L,idx)	sol_getiuservalue(L,idx,1)
#define sol_setuservalue(L,idx)	sol_setiuservalue(L,idx,1)

#define SOL_NUMTAGS		SOL_NUMTYPES

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define SOL_HOOKCALL	0
#define SOL_HOOKRET	1
#define SOL_HOOKLINE	2
#define SOL_HOOKCOUNT	3
#define SOL_HOOKTAILCALL 4


/*
** Event masks
*/
#define SOL_MASKCALL	(1 << SOL_HOOKCALL)
#define SOL_MASKRET	(1 << SOL_HOOKRET)
#define SOL_MASKLINE	(1 << SOL_HOOKLINE)
#define SOL_MASKCOUNT	(1 << SOL_HOOKCOUNT)


SOL_API int (sol_getstack) (sol_State *L, int level, sol_Debug *ar);
SOL_API int (sol_getinfo) (sol_State *L, const char *what, sol_Debug *ar);
SOL_API const char *(sol_getlocal) (sol_State *L, const sol_Debug *ar, int n);
SOL_API const char *(sol_setlocal) (sol_State *L, const sol_Debug *ar, int n);
SOL_API const char *(sol_getupvalue) (sol_State *L, int funcindex, int n);
SOL_API const char *(sol_setupvalue) (sol_State *L, int funcindex, int n);

SOL_API void *(sol_upvalueid) (sol_State *L, int fidx, int n);
SOL_API void  (sol_upvaluejoin) (sol_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

SOL_API void (sol_sethook) (sol_State *L, sol_Hook func, int mask, int count);
SOL_API sol_Hook (sol_gethook) (sol_State *L);
SOL_API int (sol_gethookmask) (sol_State *L);
SOL_API int (sol_gethookcount) (sol_State *L);

SOL_API int (sol_setcstacklimit) (sol_State *L, unsigned int limit);

struct sol_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Sol', 'C', 'main', 'tail' */
  const char *source;	/* (S) */
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  char istailcall;	/* (t) */
  unsigned short ftransfer;   /* (r) index of first value transferred */
  unsigned short ntransfer;   /* (r) number of transferred values */
  char short_src[SOL_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};

/* }====================================================================== */


/******************************************************************************
* Copyright (C) 1994-2025 Sol.org, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/


#endif
