/*
** $Id: lapi.c $
** Sol API
** See Copyright Notice in sol.h
*/

#define lapi_c
#define SOL_CORE

#include "lprefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "sol.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"



const char sol_ident[] =
  "$SolVersion: " SOL_COPYRIGHT " $"
  "$SolAuthors: " SOL_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
** '!ttisnil(o)' implies 'o != &G(L)->nilvalue', so it is not needed.
** However, it covers the most common cases in a faster way.
*/
#define isvalid(L, o)	(!ttisnil(o) || o != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= SOL_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < SOL_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (sol_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func.p + idx;
    api_check(L, idx <= ci->top.p - (ci->func.p + 1), "unacceptable index");
    if (o >= L->top.p) return &G(L)->nilvalue;
    else return s2v(o);
  }
  else if (!ispseudo(idx)) {  /* negative index */
    api_check(L, idx != 0 && -idx <= L->top.p - (ci->func.p + 1),
                 "invalid index");
    return s2v(L->top.p + idx);
  }
  else if (idx == SOL_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = SOL_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func.p))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func.p));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C function or Sol function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func.p)), "caller not a C function");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
l_sinline StkId index2stack (sol_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func.p + idx;
    api_check(L, o < L->top.p, "invalid index");
    return o;
  }
  else {    /* non-positive index */
    api_check(L, idx != 0 && -idx <= L->top.p - (ci->func.p + 1),
                 "invalid index");
    api_check(L, !ispseudo(idx), "invalid index");
    return L->top.p + idx;
  }
}


SOL_API int sol_checkstack (sol_State *L, int n) {
  int res;
  CallInfo *ci;
  sol_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last.p - L->top.p > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else  /* need to grow stack */
    res = solD_growstack(L, n, 0);
  if (res && ci->top.p < L->top.p + n)
    ci->top.p = L->top.p + n;  /* adjust frame top */
  sol_unlock(L);
  return res;
}


SOL_API void sol_xmove (sol_State *from, sol_State *to, int n) {
  int i;
  if (from == to) return;
  sol_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top.p - to->top.p >= n, "stack overflow");
  from->top.p -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top.p, from->top.p + i);
    to->top.p++;  /* stack already checked by previous 'api_check' */
  }
  sol_unlock(to);
}


SOL_API sol_CFunction sol_atpanic (sol_State *L, sol_CFunction panicf) {
  sol_CFunction old;
  sol_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  sol_unlock(L);
  return old;
}


SOL_API sol_Number sol_version (sol_State *L) {
  UNUSED(L);
  return SOL_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
SOL_API int sol_absindex (sol_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top.p - L->ci->func.p) + idx;
}


SOL_API int sol_gettop (sol_State *L) {
  return cast_int(L->top.p - (L->ci->func.p + 1));
}


SOL_API void sol_settop (sol_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  sol_lock(L);
  ci = L->ci;
  func = ci->func.p;
  if (idx >= 0) {
    api_check(L, idx <= ci->top.p - (func + 1), "new top too large");
    diff = ((func + 1) + idx) - L->top.p;
    for (; diff > 0; diff--)
      setnilvalue(s2v(L->top.p++));  /* clear new slots */
  }
  else {
    api_check(L, -(idx+1) <= (L->top.p - (func + 1)), "invalid new top");
    diff = idx + 1;  /* will "subtract" index (as it is negative) */
  }
  api_check(L, L->tbclist.p < L->top.p, "previous pop of an unclosed slot");
  newtop = L->top.p + diff;
  if (diff < 0 && L->tbclist.p >= newtop) {
    sol_assert(hastocloseCfunc(ci->nresults));
    newtop = solF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top.p = newtop;  /* correct top only after closing any upvalue */
  sol_unlock(L);
}


SOL_API void sol_closeslot (sol_State *L, int idx) {
  StkId level;
  sol_lock(L);
  level = index2stack(L, idx);
  api_check(L, hastocloseCfunc(L->ci->nresults) && L->tbclist.p == level,
     "no variable to close at given level");
  level = solF_close(L, level, CLOSEKTOP, 0);
  setnilvalue(s2v(level));
  sol_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'sol_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
l_sinline void reverse (sol_State *L, StkId from, StkId to) {
  for (; from < to; from++, to--) {
    TValue temp;
    setobj(L, &temp, s2v(from));
    setobjs2s(L, from, to);
    setobj2s(L, to, &temp);
  }
}


/*
** Let x = AB, where A is a prefix of length 'n'. Then,
** rotate x n == BA. But BA == (A^r . B^r)^r.
*/
SOL_API void sol_rotate (sol_State *L, int idx, int n) {
  StkId p, t, m;
  sol_lock(L);
  t = L->top.p - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  sol_unlock(L);
}


SOL_API void sol_copy (sol_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  sol_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    solC_barrier(L, clCvalue(s2v(L->ci->func.p)), fr);
  /* SOL_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  sol_unlock(L);
}


SOL_API void sol_pushvalue (sol_State *L, int idx) {
  sol_lock(L);
  setobj2s(L, L->top.p, index2value(L, idx));
  api_incr_top(L);
  sol_unlock(L);
}



/*
** access functions (stack -> C)
*/


SOL_API int sol_type (sol_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : SOL_TNONE);
}


SOL_API const char *sol_typename (sol_State *L, int t) {
  UNUSED(L);
  api_check(L, SOL_TNONE <= t && t < SOL_NUMTYPES, "invalid type");
  return ttypename(t);
}


SOL_API int sol_iscfunction (sol_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


SOL_API int sol_isinteger (sol_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


SOL_API int sol_isnumber (sol_State *L, int idx) {
  sol_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


SOL_API int sol_isstring (sol_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


SOL_API int sol_isuserdata (sol_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


SOL_API int sol_rawequal (sol_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? solV_rawequalobj(o1, o2) : 0;
}


SOL_API void sol_arith (sol_State *L, int op) {
  sol_lock(L);
  if (op != SOL_OPUNM && op != SOL_OPBNOT)
    api_checknelems(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top.p, L->top.p - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result go to top - 2 */
  solO_arith(L, op, s2v(L->top.p - 2), s2v(L->top.p - 1), L->top.p - 2);
  L->top.p--;  /* remove second operand */
  sol_unlock(L);
}


SOL_API int sol_compare (sol_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  sol_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case SOL_OPEQ: i = solV_equalobj(L, o1, o2); break;
      case SOL_OPLT: i = solV_lessthan(L, o1, o2); break;
      case SOL_OPLE: i = solV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  sol_unlock(L);
  return i;
}


SOL_API size_t sol_stringtonumber (sol_State *L, const char *s) {
  size_t sz = solO_str2num(s, s2v(L->top.p));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


SOL_API sol_Number sol_tonumberx (sol_State *L, int idx, int *pisnum) {
  sol_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


SOL_API sol_Integer sol_tointegerx (sol_State *L, int idx, int *pisnum) {
  sol_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


SOL_API int sol_toboolean (sol_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


SOL_API const char *sol_tolstring (sol_State *L, int idx, size_t *len) {
  TValue *o;
  sol_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      sol_unlock(L);
      return NULL;
    }
    solO_tostring(L, o);
    solC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  if (len != NULL)
    *len = tsslen(tsvalue(o));
  sol_unlock(L);
  return getstr(tsvalue(o));
}


SOL_API sol_Unsigned sol_rawlen (sol_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case SOL_VSHRSTR: return tsvalue(o)->shrlen;
    case SOL_VLNGSTR: return tsvalue(o)->u.lnglen;
    case SOL_VUSERDATA: return uvalue(o)->len;
    case SOL_VTABLE: return solH_getn(hvalue(o));
    default: return 0;
  }
}


SOL_API sol_CFunction sol_tocfunction (sol_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case SOL_TUSERDATA: return getudatamem(uvalue(o));
    case SOL_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


SOL_API void *sol_touserdata (sol_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


SOL_API sol_State *sol_tothread (sol_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here goes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
SOL_API const void *sol_topointer (sol_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case SOL_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case SOL_VUSERDATA: case SOL_VLIGHTUSERDATA:
      return touserdata(o);
    default: {
      if (iscollectable(o))
        return gcvalue(o);
      else
        return NULL;
    }
  }
}



/*
** push functions (C -> stack)
*/


SOL_API void sol_pushnil (sol_State *L) {
  sol_lock(L);
  setnilvalue(s2v(L->top.p));
  api_incr_top(L);
  sol_unlock(L);
}


SOL_API void sol_pushnumber (sol_State *L, sol_Number n) {
  sol_lock(L);
  setfltvalue(s2v(L->top.p), n);
  api_incr_top(L);
  sol_unlock(L);
}


SOL_API void sol_pushinteger (sol_State *L, sol_Integer n) {
  sol_lock(L);
  setivalue(s2v(L->top.p), n);
  api_incr_top(L);
  sol_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
SOL_API const char *sol_pushlstring (sol_State *L, const char *s, size_t len) {
  TString *ts;
  sol_lock(L);
  ts = (len == 0) ? solS_new(L, "") : solS_newlstr(L, s, len);
  setsvalue2s(L, L->top.p, ts);
  api_incr_top(L);
  solC_checkGC(L);
  sol_unlock(L);
  return getstr(ts);
}


SOL_API const char *sol_pushstring (sol_State *L, const char *s) {
  sol_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top.p));
  else {
    TString *ts;
    ts = solS_new(L, s);
    setsvalue2s(L, L->top.p, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  solC_checkGC(L);
  sol_unlock(L);
  return s;
}


SOL_API const char *sol_pushvfstring (sol_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  sol_lock(L);
  ret = solO_pushvfstring(L, fmt, argp);
  solC_checkGC(L);
  sol_unlock(L);
  return ret;
}


SOL_API const char *sol_pushfstring (sol_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  sol_lock(L);
  va_start(argp, fmt);
  ret = solO_pushvfstring(L, fmt, argp);
  va_end(argp);
  solC_checkGC(L);
  sol_unlock(L);
  return ret;
}


SOL_API void sol_pushcclosure (sol_State *L, sol_CFunction fn, int n) {
  sol_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top.p), fn);
    api_incr_top(L);
  }
  else {
    CClosure *cl;
    api_checknelems(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = solF_newCclosure(L, n);
    cl->f = fn;
    L->top.p -= n;
    while (n--) {
      setobj2n(L, &cl->upvalue[n], s2v(L->top.p + n));
      /* does not need barrier because closure is white */
      sol_assert(iswhite(cl));
    }
    setclCvalue(L, s2v(L->top.p), cl);
    api_incr_top(L);
    solC_checkGC(L);
  }
  sol_unlock(L);
}


SOL_API void sol_pushboolean (sol_State *L, int b) {
  sol_lock(L);
  if (b)
    setbtvalue(s2v(L->top.p));
  else
    setbfvalue(s2v(L->top.p));
  api_incr_top(L);
  sol_unlock(L);
}


SOL_API void sol_pushlightuserdata (sol_State *L, void *p) {
  sol_lock(L);
  setpvalue(s2v(L->top.p), p);
  api_incr_top(L);
  sol_unlock(L);
}


SOL_API int sol_pushthread (sol_State *L) {
  sol_lock(L);
  setthvalue(L, s2v(L->top.p), L);
  api_incr_top(L);
  sol_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Sol -> stack)
*/


l_sinline int auxgetstr (sol_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = solS_new(L, k);
  if (solV_fastget(L, t, str, slot, solH_getstr)) {
    setobj2s(L, L->top.p, slot);
    api_incr_top(L);
  }
  else {
    setsvalue2s(L, L->top.p, str);
    api_incr_top(L);
    solV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, slot);
  }
  sol_unlock(L);
  return ttype(s2v(L->top.p - 1));
}


/*
** Get the global table in the registry. Since all predefined
** indices in the registry were inserted right when the registry
** was created and never removed, they must always be in the array
** part of the registry.
*/
#define getGtable(L)  \
	(&hvalue(&G(L)->l_registry)->array[SOL_RIDX_GLOBALS - 1])


SOL_API int sol_getglobal (sol_State *L, const char *name) {
  const TValue *G;
  sol_lock(L);
  G = getGtable(L);
  return auxgetstr(L, G, name);
}


SOL_API int sol_gettable (sol_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  sol_lock(L);
  t = index2value(L, idx);
  if (solV_fastget(L, t, s2v(L->top.p - 1), slot, solH_get)) {
    setobj2s(L, L->top.p - 1, slot);
  }
  else
    solV_finishget(L, t, s2v(L->top.p - 1), L->top.p - 1, slot);
  sol_unlock(L);
  return ttype(s2v(L->top.p - 1));
}


SOL_API int sol_getfield (sol_State *L, int idx, const char *k) {
  sol_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


SOL_API int sol_geti (sol_State *L, int idx, sol_Integer n) {
  TValue *t;
  const TValue *slot;
  sol_lock(L);
  t = index2value(L, idx);
  if (solV_fastgeti(L, t, n, slot)) {
    setobj2s(L, L->top.p, slot);
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    solV_finishget(L, t, &aux, L->top.p, slot);
  }
  api_incr_top(L);
  sol_unlock(L);
  return ttype(s2v(L->top.p - 1));
}


l_sinline int finishrawget (sol_State *L, const TValue *val) {
  if (isempty(val))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top.p));
  else
    setobj2s(L, L->top.p, val);
  api_incr_top(L);
  sol_unlock(L);
  return ttype(s2v(L->top.p - 1));
}


static Table *gettable (sol_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


SOL_API int sol_rawget (sol_State *L, int idx) {
  Table *t;
  const TValue *val;
  sol_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  val = solH_get(t, s2v(L->top.p - 1));
  L->top.p--;  /* remove key */
  return finishrawget(L, val);
}


SOL_API int sol_rawgeti (sol_State *L, int idx, sol_Integer n) {
  Table *t;
  sol_lock(L);
  t = gettable(L, idx);
  return finishrawget(L, solH_getint(t, n));
}


SOL_API int sol_rawgetp (sol_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  sol_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, solH_get(t, &k));
}


SOL_API void sol_createtable (sol_State *L, int narray, int nrec) {
  Table *t;
  sol_lock(L);
  t = solH_new(L);
  sethvalue2s(L, L->top.p, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    solH_resize(L, t, narray, nrec);
  solC_checkGC(L);
  sol_unlock(L);
}


SOL_API int sol_getmetatable (sol_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  sol_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case SOL_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case SOL_TUSERDATA:
      mt = uvalue(obj)->metatable;
      break;
    default:
      mt = G(L)->mt[ttype(obj)];
      break;
  }
  if (mt != NULL) {
    sethvalue2s(L, L->top.p, mt);
    api_incr_top(L);
    res = 1;
  }
  sol_unlock(L);
  return res;
}


SOL_API int sol_getiuservalue (sol_State *L, int idx, int n) {
  TValue *o;
  int t;
  sol_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top.p));
    t = SOL_TNONE;
  }
  else {
    setobj2s(L, L->top.p, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top.p));
  }
  api_incr_top(L);
  sol_unlock(L);
  return t;
}


/*
** set functions (stack -> Sol)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (sol_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = solS_new(L, k);
  api_checknelems(L, 1);
  if (solV_fastget(L, t, str, slot, solH_getstr)) {
    solV_finishfastset(L, t, slot, s2v(L->top.p - 1));
    L->top.p--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top.p, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    solV_finishset(L, t, s2v(L->top.p - 1), s2v(L->top.p - 2), slot);
    L->top.p -= 2;  /* pop value and key */
  }
  sol_unlock(L);  /* lock done by caller */
}


SOL_API void sol_setglobal (sol_State *L, const char *name) {
  const TValue *G;
  sol_lock(L);  /* unlock done in 'auxsetstr' */
  G = getGtable(L);
  auxsetstr(L, G, name);
}


SOL_API void sol_settable (sol_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  sol_lock(L);
  api_checknelems(L, 2);
  t = index2value(L, idx);
  if (solV_fastget(L, t, s2v(L->top.p - 2), slot, solH_get)) {
    solV_finishfastset(L, t, slot, s2v(L->top.p - 1));
  }
  else
    solV_finishset(L, t, s2v(L->top.p - 2), s2v(L->top.p - 1), slot);
  L->top.p -= 2;  /* pop index and value */
  sol_unlock(L);
}


SOL_API void sol_setfield (sol_State *L, int idx, const char *k) {
  sol_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


SOL_API void sol_seti (sol_State *L, int idx, sol_Integer n) {
  TValue *t;
  const TValue *slot;
  sol_lock(L);
  api_checknelems(L, 1);
  t = index2value(L, idx);
  if (solV_fastgeti(L, t, n, slot)) {
    solV_finishfastset(L, t, slot, s2v(L->top.p - 1));
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    solV_finishset(L, t, &aux, s2v(L->top.p - 1), slot);
  }
  L->top.p--;  /* pop value */
  sol_unlock(L);
}


static void aux_rawset (sol_State *L, int idx, TValue *key, int n) {
  Table *t;
  sol_lock(L);
  api_checknelems(L, n);
  t = gettable(L, idx);
  solH_set(L, t, key, s2v(L->top.p - 1));
  invalidateTMcache(t);
  solC_barrierback(L, obj2gco(t), s2v(L->top.p - 1));
  L->top.p -= n;
  sol_unlock(L);
}


SOL_API void sol_rawset (sol_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top.p - 2), 2);
}


SOL_API void sol_rawsetp (sol_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


SOL_API void sol_rawseti (sol_State *L, int idx, sol_Integer n) {
  Table *t;
  sol_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  solH_setint(L, t, n, s2v(L->top.p - 1));
  solC_barrierback(L, obj2gco(t), s2v(L->top.p - 1));
  L->top.p--;
  sol_unlock(L);
}


SOL_API int sol_setmetatable (sol_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  sol_lock(L);
  api_checknelems(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top.p - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top.p - 1)), "table expected");
    mt = hvalue(s2v(L->top.p - 1));
  }
  switch (ttype(obj)) {
    case SOL_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        solC_objbarrier(L, gcvalue(obj), mt);
        solC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case SOL_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        solC_objbarrier(L, uvalue(obj), mt);
        solC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top.p--;
  sol_unlock(L);
  return 1;
}


SOL_API int sol_setiuservalue (sol_State *L, int idx, int n) {
  TValue *o;
  int res;
  sol_lock(L);
  api_checknelems(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top.p - 1));
    solC_barrierback(L, gcvalue(o), s2v(L->top.p - 1));
    res = 1;
  }
  L->top.p--;
  sol_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Sol code)
*/


#define checkresults(L,na,nr) \
     api_check(L, (nr) == SOL_MULTRET \
               || (L->ci->top.p - L->top.p >= (nr) - (na)), \
	"results from function overflow current stack size")


SOL_API void sol_callk (sol_State *L, int nargs, int nresults,
                        sol_KContext ctx, sol_KFunction k) {
  StkId func;
  sol_lock(L);
  api_check(L, k == NULL || !isSol(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == SOL_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top.p - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    solD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    solD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  sol_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (sol_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  solD_callnoyield(L, c->func, c->nresults);
}



SOL_API int sol_pcallk (sol_State *L, int nargs, int nresults, int errfunc,
                        sol_KContext ctx, sol_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  sol_lock(L);
  api_check(L, k == NULL || !isSol(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == SOL_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  if (errfunc == 0)
    func = 0;
  else {
    StkId o = index2stack(L, errfunc);
    api_check(L, ttisfunction(s2v(o)), "error handler must be a function");
    func = savestack(L, o);
  }
  c.func = L->top.p - (nargs+1);  /* function to be called */
  if (k == NULL || !yieldable(L)) {  /* no continuation or no yieldable? */
    c.nresults = nresults;  /* do a 'conventional' protected call */
    status = solD_pcall(L, f_call, &c, savestack(L, c.func), func);
  }
  else {  /* prepare continuation (call is already protected by 'resume') */
    CallInfo *ci = L->ci;
    ci->u.c.k = k;  /* save continuation */
    ci->u.c.ctx = ctx;  /* save context */
    /* save information for error recovery */
    ci->u2.funcidx = cast_int(savestack(L, c.func));
    ci->u.c.old_errfunc = L->errfunc;
    L->errfunc = func;
    setoah(ci->callstatus, L->allowhook);  /* save value of 'allowhook' */
    ci->callstatus |= CIST_YPCALL;  /* function can do error recovery */
    solD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = SOL_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  sol_unlock(L);
  return status;
}


SOL_API int sol_load (sol_State *L, sol_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  sol_lock(L);
  if (!chunkname) chunkname = "?";
  solZ_init(L, &z, reader, data);
  status = solD_protectedparser(L, &z, chunkname, mode);
  if (status == SOL_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top.p - 1));  /* get new function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      const TValue *gt = getGtable(L);
      /* set global table as 1st upvalue of 'f' (may be SOL_ENV) */
      setobj(L, f->upvals[0]->v.p, gt);
      solC_barrier(L, f->upvals[0], gt);
    }
  }
  sol_unlock(L);
  return status;
}


SOL_API int sol_dump (sol_State *L, sol_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  sol_lock(L);
  api_checknelems(L, 1);
  o = s2v(L->top.p - 1);
  if (isLfunction(o))
    status = solU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  sol_unlock(L);
  return status;
}


SOL_API int sol_status (sol_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/
SOL_API int sol_gc (sol_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & GCSTPGC)  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  sol_lock(L);
  va_start(argp, what);
  switch (what) {
    case SOL_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case SOL_GCRESTART: {
      solE_setdebt(g, 0);
      g->gcstp = 0;  /* (GCSTPGC must be already zero here) */
      break;
    }
    case SOL_GCCOLLECT: {
      solC_fullgc(L, 0);
      break;
    }
    case SOL_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case SOL_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case SOL_GCSTEP: {
      int data = va_arg(argp, int);
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldstp = g->gcstp;
      g->gcstp = 0;  /* allow GC to run (GCSTPGC must be zero here) */
      if (data == 0) {
        solE_setdebt(g, 0);  /* do a basic step */
        solC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = cast(l_mem, data) * 1024 + g->GCdebt;
        solE_setdebt(g, debt);
        solC_checkGC(L);
      }
      g->gcstp = oldstp;  /* restore previous state */
      if (debt > 0 && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case SOL_GCSETPAUSE: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcpause);
      setgcparam(g->gcpause, data);
      break;
    }
    case SOL_GCSETSTEPMUL: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcstepmul);
      setgcparam(g->gcstepmul, data);
      break;
    }
    case SOL_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case SOL_GCGEN: {
      int minormul = va_arg(argp, int);
      int majormul = va_arg(argp, int);
      res = isdecGCmodegen(g) ? SOL_GCGEN : SOL_GCINC;
      if (minormul != 0)
        g->genminormul = minormul;
      if (majormul != 0)
        setgcparam(g->genmajormul, majormul);
      solC_changemode(L, KGC_GEN);
      break;
    }
    case SOL_GCINC: {
      int pause = va_arg(argp, int);
      int stepmul = va_arg(argp, int);
      int stepsize = va_arg(argp, int);
      res = isdecGCmodegen(g) ? SOL_GCGEN : SOL_GCINC;
      if (pause != 0)
        setgcparam(g->gcpause, pause);
      if (stepmul != 0)
        setgcparam(g->gcstepmul, stepmul);
      if (stepsize != 0)
        g->gcstepsize = stepsize;
      solC_changemode(L, KGC_INC);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  sol_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


SOL_API int sol_error (sol_State *L) {
  TValue *errobj;
  sol_lock(L);
  errobj = s2v(L->top.p - 1);
  api_checknelems(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    solM_error(L);  /* raise a memory error */
  else
    solG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


SOL_API int sol_next (sol_State *L, int idx) {
  Table *t;
  int more;
  sol_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  more = solH_next(L, t, L->top.p - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top.p -= 1;  /* remove key */
  sol_unlock(L);
  return more;
}


SOL_API void sol_toclose (sol_State *L, int idx) {
  int nresults;
  StkId o;
  sol_lock(L);
  o = index2stack(L, idx);
  nresults = L->ci->nresults;
  api_check(L, L->tbclist.p < o, "given index below or equal a marked one");
  solF_newtbcupval(L, o);  /* create new to-be-closed upvalue */
  if (!hastocloseCfunc(nresults))  /* function not marked yet? */
    L->ci->nresults = codeNresults(nresults);  /* mark it */
  sol_assert(hastocloseCfunc(L->ci->nresults));
  sol_unlock(L);
}


SOL_API void sol_concat (sol_State *L, int n) {
  sol_lock(L);
  api_checknelems(L, n);
  if (n > 0)
    solV_concat(L, n);
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top.p, solS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  solC_checkGC(L);
  sol_unlock(L);
}


SOL_API void sol_len (sol_State *L, int idx) {
  TValue *t;
  sol_lock(L);
  t = index2value(L, idx);
  solV_objlen(L, L->top.p, t);
  api_incr_top(L);
  sol_unlock(L);
}


SOL_API sol_Alloc sol_getallocf (sol_State *L, void **ud) {
  sol_Alloc f;
  sol_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  sol_unlock(L);
  return f;
}


SOL_API void sol_setallocf (sol_State *L, sol_Alloc f, void *ud) {
  sol_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  sol_unlock(L);
}


void sol_setwarnf (sol_State *L, sol_WarnFunction f, void *ud) {
  sol_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  sol_unlock(L);
}


void sol_warning (sol_State *L, const char *msg, int tocont) {
  sol_lock(L);
  solE_warning(L, msg, tocont);
  sol_unlock(L);
}



SOL_API void *sol_newuserdatauv (sol_State *L, size_t size, int nuvalue) {
  Udata *u;
  sol_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < SHRT_MAX, "invalid value");
  u = solS_newudata(L, size, nuvalue);
  setuvalue(L, s2v(L->top.p), u);
  api_incr_top(L);
  solC_checkGC(L);
  sol_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case SOL_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case SOL_VLCL: {  /* Sol closure */
      LClosure *f = clLvalue(fi);
      TString *name;
      Proto *p = f->p;
      if (!(cast_uint(n) - 1u  < cast_uint(p->sizeupvalues)))
        return NULL;  /* 'n' not in [1, p->sizeupvalues] */
      *val = f->upvals[n-1]->v.p;
      if (owner) *owner = obj2gco(f->upvals[n - 1]);
      name = p->upvalues[n-1].name;
      return (name == NULL) ? "(no name)" : getstr(name);
    }
    default: return NULL;  /* not a closure */
  }
}


SOL_API const char *sol_getupvalue (sol_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  sol_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top.p, val);
    api_incr_top(L);
  }
  sol_unlock(L);
  return name;
}


SOL_API const char *sol_setupvalue (sol_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  sol_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top.p--;
    setobj(L, val, s2v(L->top.p));
    solC_barrier(L, owner, val);
  }
  sol_unlock(L);
  return name;
}


static UpVal **getupvalref (sol_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Sol function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


SOL_API void *sol_upvalueid (sol_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case SOL_VLCL: {  /* sol closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case SOL_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case SOL_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


SOL_API void sol_upvaluejoin (sol_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  solC_objbarrier(L, f1, *up1);
}


