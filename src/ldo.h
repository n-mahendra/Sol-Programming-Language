/*
** $Id: ldo.h $
** Stack and Call structure of Sol
** See Copyright Notice in sol.h
*/

#ifndef ldo_h
#define ldo_h


#include "llimits.h"
#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


/*
** Macro to check stack size and grow stack if needed.  Parameters
** 'pre'/'pos' allow the macro to preserve a pointer into the
** stack across reallocations, doing the work only when needed.
** It also allows the running of one GC step when the stack is
** reallocated.
** 'condmovestack' is used in heavy tests to force a stack reallocation
** at every check.
*/
#define solD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last.p - L->top.p <= (n))) \
	  { pre; solD_growstack(L, n, 1); pos; } \
        else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define solD_checkstack(L,n)	solD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,pt)		(cast_charp(pt) - cast_charp(L->stack.p))
#define restorestack(L,n)	cast(StkId, cast_charp(L->stack.p) + (n))


/* macro to check stack size, preserving 'p' */
#define checkstackp(L,n,p)  \
  solD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p),  /* save 'p' */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC, preserving 'p' */
#define checkstackGCp(L,n,p)  \
  solD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    solC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC */
#define checkstackGC(L,fsize)  \
	solD_checkstackaux(L, (fsize), solC_checkGC(L), (void)0)


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (sol_State *L, void *ud);

SOLI_FUNC l_noret solD_errerr (sol_State *L);
SOLI_FUNC void solD_seterrorobj (sol_State *L, int errcode, StkId oldtop);
SOLI_FUNC int solD_protectedparser (sol_State *L, ZIO *z, const char *name,
                                                  const char *mode);
SOLI_FUNC void solD_hook (sol_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
SOLI_FUNC void solD_hookcall (sol_State *L, CallInfo *ci);
SOLI_FUNC int solD_pretailcall (sol_State *L, CallInfo *ci, StkId func,
                                              int narg1, int delta);
SOLI_FUNC CallInfo *solD_precall (sol_State *L, StkId func, int nResults);
SOLI_FUNC void solD_call (sol_State *L, StkId func, int nResults);
SOLI_FUNC void solD_callnoyield (sol_State *L, StkId func, int nResults);
SOLI_FUNC int solD_closeprotected (sol_State *L, ptrdiff_t level, int status);
SOLI_FUNC int solD_pcall (sol_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
SOLI_FUNC void solD_poscall (sol_State *L, CallInfo *ci, int nres);
SOLI_FUNC int solD_reallocstack (sol_State *L, int newsize, int raiseerror);
SOLI_FUNC int solD_growstack (sol_State *L, int n, int raiseerror);
SOLI_FUNC void solD_shrinkstack (sol_State *L);
SOLI_FUNC void solD_inctop (sol_State *L);

SOLI_FUNC l_noret solD_throw (sol_State *L, int errcode);
SOLI_FUNC int solD_rawrunprotected (sol_State *L, Pfunc f, void *ud);

#endif

