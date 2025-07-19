/*
** $Id: lfunc.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in sol.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"


#define sizeCclosure(n)	(cast_int(offsetof(CClosure, upvalue)) + \
                         cast_int(sizeof(TValue)) * (n))

#define sizeLclosure(n)	(cast_int(offsetof(LClosure, upvals)) + \
                         cast_int(sizeof(TValue *)) * (n))


/* test whether thread is in 'twups' list */
#define isintwups(L)	(L->twups != L)


/*
** maximum number of upvalues in a closure (both C and Sol). (Value
** must fit in a VM register.)
*/
#define MAXUPVAL	255


#define upisopen(up)	((up)->v.p != &(up)->u.value)


#define uplevel(up)	check_exp(upisopen(up), cast(StkId, (up)->v.p))


/*
** maximum number of misses before giving up the cache of closures
** in prototypes
*/
#define MAXMISS		10



/* special status to close upvalues preserving the top of the stack */
#define CLOSEKTOP	(-1)


SOLI_FUNC Proto *solF_newproto (sol_State *L);
SOLI_FUNC CClosure *solF_newCclosure (sol_State *L, int nupvals);
SOLI_FUNC LClosure *solF_newLclosure (sol_State *L, int nupvals);
SOLI_FUNC void solF_initupvals (sol_State *L, LClosure *cl);
SOLI_FUNC UpVal *solF_findupval (sol_State *L, StkId level);
SOLI_FUNC void solF_newtbcupval (sol_State *L, StkId level);
SOLI_FUNC void solF_closeupval (sol_State *L, StkId level);
SOLI_FUNC StkId solF_close (sol_State *L, StkId level, int status, int yy);
SOLI_FUNC void solF_unlinkupval (UpVal *uv);
SOLI_FUNC void solF_freeproto (sol_State *L, Proto *f);
SOLI_FUNC const char *solF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
