/*
** $Id: lvm.h $
** Sol virtual machine
** See Copyright Notice in sol.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#if !defined(SOL_NOCVTN2S)
#define cvt2str(o)	ttisnumber(o)
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(SOL_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define SOL_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(SOL_FLOORN2I)
#define SOL_FLOORN2I		F2Ieq
#endif


/*
** Rounding modes for float->integer coercion
 */
typedef enum {
  F2Ieq,     /* no rounding; accepts only integral values */
  F2Ifloor,  /* takes the floor of the number */
  F2Iceil    /* takes the ceil of the number */
} F2Imod;


/* convert an object to a float (including string coercion) */
#define tonumber(o,n) \
	(ttisfloat(o) ? (*(n) = fltvalue(o), 1) : solV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
	(ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
	(ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : solV_tointeger(o,i,SOL_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : solV_tointegerns(o,i,SOL_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define solV_rawequalobj(t1,t2)		solV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define solV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */


/*
** Special case of 'solV_fastget' for integers, inlining the fast case
** of 'solH_getint'.
*/
#define solV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->alimit) \
              ? &hvalue(t)->array[k - 1] : solH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */


/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/
#define solV_finishfastset(L,t,slot,v) \
    { setobj2t(L, cast(TValue *,slot), v); \
      solC_barrierback(L, gcvalue(t), v); }


/*
** Shift right is the same as shift left with a negative 'y'
*/
#define solV_shiftr(x,y)	solV_shiftl(x,intop(-, 0, y))



SOLI_FUNC int solV_equalobj (sol_State *L, const TValue *t1, const TValue *t2);
SOLI_FUNC int solV_lessthan (sol_State *L, const TValue *l, const TValue *r);
SOLI_FUNC int solV_lessequal (sol_State *L, const TValue *l, const TValue *r);
SOLI_FUNC int solV_tonumber_ (const TValue *obj, sol_Number *n);
SOLI_FUNC int solV_tointeger (const TValue *obj, sol_Integer *p, F2Imod mode);
SOLI_FUNC int solV_tointegerns (const TValue *obj, sol_Integer *p,
                                F2Imod mode);
SOLI_FUNC int solV_flttointeger (sol_Number n, sol_Integer *p, F2Imod mode);
SOLI_FUNC void solV_finishget (sol_State *L, const TValue *t, TValue *key,
                               StkId val, const TValue *slot);
SOLI_FUNC void solV_finishset (sol_State *L, const TValue *t, TValue *key,
                               TValue *val, const TValue *slot);
SOLI_FUNC void solV_finishOp (sol_State *L);
SOLI_FUNC void solV_execute (sol_State *L, CallInfo *ci);
SOLI_FUNC void solV_concat (sol_State *L, int total);
SOLI_FUNC sol_Integer solV_idiv (sol_State *L, sol_Integer x, sol_Integer y);
SOLI_FUNC sol_Integer solV_mod (sol_State *L, sol_Integer x, sol_Integer y);
SOLI_FUNC sol_Number solV_modf (sol_State *L, sol_Number x, sol_Number y);
SOLI_FUNC sol_Integer solV_shiftl (sol_Integer x, sol_Integer y);
SOLI_FUNC void solV_objlen (sol_State *L, StkId ra, const TValue *rb);

#endif
