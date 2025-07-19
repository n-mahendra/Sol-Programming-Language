/*
** $Id: ldo.c $
** Stack and Call structure of Sol
** See Copyright Notice in sol.h
*/

#define ldo_c
#define SOL_CORE

#include "lprefix.h"


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "sol.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"



#define errorstatus(s)	((s) > SOL_YIELD)


/*
** {======================================================
** Error-recovery functions
** =======================================================
*/

/*
** SOLI_THROW/SOLI_TRY define how Sol does exception handling. By
** default, Sol handles errors with exceptions when compiling as
** C++ code, with _longjmp/_setjmp when asked to use them, and with
** longjmp/setjmp otherwise.
*/
#if !defined(SOLI_THROW)				/* { */

#if defined(__cplusplus) && !defined(SOL_USE_LONGJMP)	/* { */

/* C++ exceptions */
#define SOLI_THROW(L,c)		throw(c)
#define SOLI_TRY(L,c,a) \
	try { a } catch(...) { if ((c)->status == 0) (c)->status = -1; }
#define soli_jmpbuf		int  /* dummy variable */

#elif defined(SOL_USE_POSIX)				/* }{ */

/* in POSIX, try _longjmp/_setjmp (more efficient) */
#define SOLI_THROW(L,c)		_longjmp((c)->b, 1)
#define SOLI_TRY(L,c,a)		if (_setjmp((c)->b) == 0) { a }
#define soli_jmpbuf		jmp_buf

#else							/* }{ */

/* ISO C handling with long jumps */
#define SOLI_THROW(L,c)		longjmp((c)->b, 1)
#define SOLI_TRY(L,c,a)		if (setjmp((c)->b) == 0) { a }
#define soli_jmpbuf		jmp_buf

#endif							/* } */

#endif							/* } */



/* chain list of long jump buffers */
struct sol_longjmp {
  struct sol_longjmp *previous;
  soli_jmpbuf b;
  volatile int status;  /* error code */
};


void solD_seterrorobj (sol_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case SOL_ERRMEM: {  /* memory error? */
      setsvalue2s(L, oldtop, G(L)->memerrmsg); /* reuse preregistered msg. */
      break;
    }
    case SOL_OK: {  /* special case only for closing upvalues */
      setnilvalue(s2v(oldtop));  /* no error message */
      break;
    }
    default: {
      sol_assert(errorstatus(errcode));  /* real error */
      setobjs2s(L, oldtop, L->top.p - 1);  /* error message on current top */
      break;
    }
  }
  L->top.p = oldtop + 1;
}


l_noret solD_throw (sol_State *L, int errcode) {
  if (L->errorJmp) {  /* thread has an error handler? */
    L->errorJmp->status = errcode;  /* set status */
    SOLI_THROW(L, L->errorJmp);  /* jump to it */
  }
  else {  /* thread has no error handler */
    global_State *g = G(L);
    errcode = solE_resetthread(L, errcode);  /* close all upvalues */
    L->status = errcode;
    if (g->mainthread->errorJmp) {  /* main thread has a handler? */
      setobjs2s(L, g->mainthread->top.p++, L->top.p - 1);  /* copy error obj. */
      solD_throw(g->mainthread, errcode);  /* re-throw in main thread */
    }
    else {  /* no handler at all; abort */
      if (g->panic) {  /* panic function? */
        sol_unlock(L);
        g->panic(L);  /* call panic function (last chance to jump out) */
      }
      abort();
    }
  }
}


int solD_rawrunprotected (sol_State *L, Pfunc f, void *ud) {
  l_uint32 oldnCcalls = L->nCcalls;
  struct sol_longjmp lj;
  lj.status = SOL_OK;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  SOLI_TRY(L, &lj,
    (*f)(L, ud);
  );
  L->errorJmp = lj.previous;  /* restore old error handler */
  L->nCcalls = oldnCcalls;
  return lj.status;
}

/* }====================================================== */


/*
** {==================================================================
** Stack reallocation
** ===================================================================
*/


/*
** Change all pointers to the stack into offsets.
*/
static void relstack (sol_State *L) {
  CallInfo *ci;
  UpVal *up;
  L->top.offset = savestack(L, L->top.p);
  L->tbclist.offset = savestack(L, L->tbclist.p);
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v.offset = savestack(L, uplevel(up));
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top.offset = savestack(L, ci->top.p);
    ci->func.offset = savestack(L, ci->func.p);
  }
}


/*
** Change back all offsets into pointers.
*/
static void correctstack (sol_State *L) {
  CallInfo *ci;
  UpVal *up;
  L->top.p = restorestack(L, L->top.offset);
  L->tbclist.p = restorestack(L, L->tbclist.offset);
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v.p = s2v(restorestack(L, up->v.offset));
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top.p = restorestack(L, ci->top.offset);
    ci->func.p = restorestack(L, ci->func.offset);
    if (isSol(ci))
      ci->u.l.trap = 1;  /* signal to update 'trap' in 'solV_execute' */
  }
}


/* some space for error handling */
#define ERRORSTACKSIZE	(SOLI_MAXSTACK + 200)


/* raise an error while running the message handler */
l_noret solD_errerr (sol_State *L) {
  TString *msg = solS_newliteral(L, "error in error handling");
  setsvalue2s(L, L->top.p, msg);
  L->top.p++;  /* assume EXTRA_STACK */
  solD_throw(L, SOL_ERRERR);
}


/*
** Reallocate the stack to a new size, correcting all pointers into it.
** In ISO C, any pointer use after the pointer has been deallocated is
** undefined behavior. So, before the reallocation, all pointers are
** changed to offsets, and after the reallocation they are changed back
** to pointers. As during the reallocation the pointers are invalid, the
** reallocation cannot run emergency collections.
**
** In case of allocation error, raise an error or return false according
** to 'raiseerror'.
*/
int solD_reallocstack (sol_State *L, int newsize, int raiseerror) {
  int oldsize = stacksize(L);
  int i;
  StkId newstack;
  int oldgcstop = G(L)->gcstopem;
  sol_assert(newsize <= SOLI_MAXSTACK || newsize == ERRORSTACKSIZE);
  relstack(L);  /* change pointers to offsets */
  G(L)->gcstopem = 1;  /* stop emergency collection */
  newstack = solM_reallocvector(L, L->stack.p, oldsize + EXTRA_STACK,
                                   newsize + EXTRA_STACK, StackValue);
  G(L)->gcstopem = oldgcstop;  /* restore emergency collection */
  if (l_unlikely(newstack == NULL)) {  /* reallocation failed? */
    correctstack(L);  /* change offsets back to pointers */
    if (raiseerror)
      solM_error(L);
    else return 0;  /* do not raise an error */
  }
  L->stack.p = newstack;
  correctstack(L);  /* change offsets back to pointers */
  L->stack_last.p = L->stack.p + newsize;
  for (i = oldsize + EXTRA_STACK; i < newsize + EXTRA_STACK; i++)
    setnilvalue(s2v(newstack + i)); /* erase new segment */
  return 1;
}


/*
** Try to grow the stack by at least 'n' elements. When 'raiseerror'
** is true, raises any error; otherwise, return 0 in case of errors.
*/
int solD_growstack (sol_State *L, int n, int raiseerror) {
  int size = stacksize(L);
  if (l_unlikely(size > SOLI_MAXSTACK)) {
    /* if stack is larger than maximum, thread is already using the
       extra space reserved for errors, that is, thread is handling
       a stack error; cannot grow further than that. */
    sol_assert(stacksize(L) == ERRORSTACKSIZE);
    if (raiseerror)
      solD_errerr(L);  /* error inside message handler */
    return 0;  /* if not 'raiseerror', just signal it */
  }
  else if (n < SOLI_MAXSTACK) {  /* avoids arithmetic overflows */
    int newsize = 2 * size;  /* tentative new size */
    int needed = cast_int(L->top.p - L->stack.p) + n;
    if (newsize > SOLI_MAXSTACK)  /* cannot cross the limit */
      newsize = SOLI_MAXSTACK;
    if (newsize < needed)  /* but must respect what was asked for */
      newsize = needed;
    if (l_likely(newsize <= SOLI_MAXSTACK))
      return solD_reallocstack(L, newsize, raiseerror);
  }
  /* else stack overflow */
  /* add extra size to be able to handle the error message */
  solD_reallocstack(L, ERRORSTACKSIZE, raiseerror);
  if (raiseerror)
    solG_runerror(L, "stack overflow");
  return 0;
}


/*
** Compute how much of the stack is being used, by computing the
** maximum top of all call frames in the stack and the current top.
*/
static int stackinuse (sol_State *L) {
  CallInfo *ci;
  int res;
  StkId lim = L->top.p;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    if (lim < ci->top.p) lim = ci->top.p;
  }
  sol_assert(lim <= L->stack_last.p + EXTRA_STACK);
  res = cast_int(lim - L->stack.p) + 1;  /* part of stack in use */
  if (res < SOL_MINSTACK)
    res = SOL_MINSTACK;  /* ensure a minimum size */
  return res;
}


/*
** If stack size is more than 3 times the current use, reduce that size
** to twice the current use. (So, the final stack size is at most 2/3 the
** previous size, and half of its entries are empty.)
** As a particular case, if stack was handling a stack overflow and now
** it is not, 'max' (limited by SOLI_MAXSTACK) will be smaller than
** stacksize (equal to ERRORSTACKSIZE in this case), and so the stack
** will be reduced to a "regular" size.
*/
void solD_shrinkstack (sol_State *L) {
  int inuse = stackinuse(L);
  int max = (inuse > SOLI_MAXSTACK / 3) ? SOLI_MAXSTACK : inuse * 3;
  /* if thread is currently not handling a stack overflow and its
     size is larger than maximum "reasonable" size, shrink it */
  if (inuse <= SOLI_MAXSTACK && stacksize(L) > max) {
    int nsize = (inuse > SOLI_MAXSTACK / 2) ? SOLI_MAXSTACK : inuse * 2;
    solD_reallocstack(L, nsize, 0);  /* ok if that fails */
  }
  else  /* don't change stack */
    condmovestack(L,{},{});  /* (change only for debugging) */
  solE_shrinkCI(L);  /* shrink CI list */
}


void solD_inctop (sol_State *L) {
  solD_checkstack(L, 1);
  L->top.p++;
}

/* }================================================================== */


/*
** Call a hook for the given event. Make sure there is a hook to be
** called. (Both 'L->hook' and 'L->hookmask', which trigger this
** function, can be changed asynchronously by signals.)
*/
void solD_hook (sol_State *L, int event, int line,
                              int ftransfer, int ntransfer) {
  sol_Hook hook = L->hook;
  if (hook && L->allowhook) {  /* make sure there is a hook */
    int mask = CIST_HOOKED;
    CallInfo *ci = L->ci;
    ptrdiff_t top = savestack(L, L->top.p);  /* preserve original 'top' */
    ptrdiff_t ci_top = savestack(L, ci->top.p);  /* idem for 'ci->top' */
    sol_Debug ar;
    ar.event = event;
    ar.currentline = line;
    ar.i_ci = ci;
    if (ntransfer != 0) {
      mask |= CIST_TRAN;  /* 'ci' has transfer information */
      ci->u2.transferinfo.ftransfer = ftransfer;
      ci->u2.transferinfo.ntransfer = ntransfer;
    }
    if (isSol(ci) && L->top.p < ci->top.p)
      L->top.p = ci->top.p;  /* protect entire activation register */
    solD_checkstack(L, SOL_MINSTACK);  /* ensure minimum stack size */
    if (ci->top.p < L->top.p + SOL_MINSTACK)
      ci->top.p = L->top.p + SOL_MINSTACK;
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    ci->callstatus |= mask;
    sol_unlock(L);
    (*hook)(L, &ar);
    sol_lock(L);
    sol_assert(!L->allowhook);
    L->allowhook = 1;
    ci->top.p = restorestack(L, ci_top);
    L->top.p = restorestack(L, top);
    ci->callstatus &= ~mask;
  }
}


/*
** Executes a call hook for Sol functions. This function is called
** whenever 'hookmask' is not zero, so it checks whether call hooks are
** active.
*/
void solD_hookcall (sol_State *L, CallInfo *ci) {
  L->oldpc = 0;  /* set 'oldpc' for new function */
  if (L->hookmask & SOL_MASKCALL) {  /* is call hook on? */
    int event = (ci->callstatus & CIST_TAIL) ? SOL_HOOKTAILCALL
                                             : SOL_HOOKCALL;
    Proto *p = ci_func(ci)->p;
    ci->u.l.savedpc++;  /* hooks assume 'pc' is already incremented */
    solD_hook(L, event, -1, 1, p->numparams);
    ci->u.l.savedpc--;  /* correct 'pc' */
  }
}


/*
** Executes a return hook for Sol and C functions and sets/corrects
** 'oldpc'. (Note that this correction is needed by the line hook, so it
** is done even when return hooks are off.)
*/
static void rethook (sol_State *L, CallInfo *ci, int nres) {
  if (L->hookmask & SOL_MASKRET) {  /* is return hook on? */
    StkId firstres = L->top.p - nres;  /* index of first result */
    int delta = 0;  /* correction for vararg functions */
    int ftransfer;
    if (isSol(ci)) {
      Proto *p = ci_func(ci)->p;
      if (p->is_vararg)
        delta = ci->u.l.nextraargs + p->numparams + 1;
    }
    ci->func.p += delta;  /* if vararg, back to virtual 'func' */
    ftransfer = cast(unsigned short, firstres - ci->func.p);
    solD_hook(L, SOL_HOOKRET, -1, ftransfer, nres);  /* call it */
    ci->func.p -= delta;
  }
  if (isSol(ci = ci->previous))
    L->oldpc = pcRel(ci->u.l.savedpc, ci_func(ci)->p);  /* set 'oldpc' */
}


/*
** Check whether 'func' has a '__call' metafield. If so, put it in the
** stack, below original 'func', so that 'solD_precall' can call it. Raise
** an error if there is no '__call' metafield.
*/
static StkId tryfuncTM (sol_State *L, StkId func) {
  const TValue *tm;
  StkId p;
  checkstackGCp(L, 1, func);  /* space for metamethod */
  tm = solT_gettmbyobj(L, s2v(func), TM_CALL);  /* (after previous GC) */
  if (l_unlikely(ttisnil(tm)))
    solG_callerror(L, s2v(func));  /* nothing to call */
  for (p = L->top.p; p > func; p--)  /* open space for metamethod */
    setobjs2s(L, p, p-1);
  L->top.p++;  /* stack space pre-allocated by the caller */
  setobj2s(L, func, tm);  /* metamethod is the new function to be called */
  return func;
}


/*
** Given 'nres' results at 'firstResult', move 'wanted' of them to 'res'.
** Handle most typical cases (zero results for commands, one result for
** expressions, multiple results for tail calls/single parameters)
** separated.
*/
l_sinline void moveresults (sol_State *L, StkId res, int nres, int wanted) {
  StkId firstresult;
  int i;
  switch (wanted) {  /* handle typical cases separately */
    case 0:  /* no values needed */
      L->top.p = res;
      return;
    case 1:  /* one value needed */
      if (nres == 0)   /* no results? */
        setnilvalue(s2v(res));  /* adjust with nil */
      else  /* at least one result */
        setobjs2s(L, res, L->top.p - nres);  /* move it to proper place */
      L->top.p = res + 1;
      return;
    case SOL_MULTRET:
      wanted = nres;  /* we want all results */
      break;
    default:  /* two/more results and/or to-be-closed variables */
      if (hastocloseCfunc(wanted)) {  /* to-be-closed variables? */
        L->ci->callstatus |= CIST_CLSRET;  /* in case of yields */
        L->ci->u2.nres = nres;
        res = solF_close(L, res, CLOSEKTOP, 1);
        L->ci->callstatus &= ~CIST_CLSRET;
        if (L->hookmask) {  /* if needed, call hook after '__close's */
          ptrdiff_t savedres = savestack(L, res);
          rethook(L, L->ci, nres);
          res = restorestack(L, savedres);  /* hook can move stack */
        }
        wanted = decodeNresults(wanted);
        if (wanted == SOL_MULTRET)
          wanted = nres;  /* we want all results */
      }
      break;
  }
  /* generic case */
  firstresult = L->top.p - nres;  /* index of first result */
  if (nres > wanted)  /* extra results? */
    nres = wanted;  /* don't need them */
  for (i = 0; i < nres; i++)  /* move all results to correct place */
    setobjs2s(L, res + i, firstresult + i);
  for (; i < wanted; i++)  /* complete wanted number of results */
    setnilvalue(s2v(res + i));
  L->top.p = res + wanted;  /* top points after the last result */
}


/*
** Finishes a function call: calls hook if necessary, moves current
** number of results to proper place, and returns to previous call
** info. If function has to close variables, hook must be called after
** that.
*/
void solD_poscall (sol_State *L, CallInfo *ci, int nres) {
  int wanted = ci->nresults;
  if (l_unlikely(L->hookmask && !hastocloseCfunc(wanted)))
    rethook(L, ci, nres);
  /* move results to proper place */
  moveresults(L, ci->func.p, nres, wanted);
  /* function cannot be in any of these cases when returning */
  sol_assert(!(ci->callstatus &
        (CIST_HOOKED | CIST_YPCALL | CIST_FIN | CIST_TRAN | CIST_CLSRET)));
  L->ci = ci->previous;  /* back to caller (after closing variables) */
}



#define next_ci(L)  (L->ci->next ? L->ci->next : solE_extendCI(L))


l_sinline CallInfo *prepCallInfo (sol_State *L, StkId func, int nret,
                                                int mask, StkId top) {
  CallInfo *ci = L->ci = next_ci(L);  /* new frame */
  ci->func.p = func;
  ci->nresults = nret;
  ci->callstatus = mask;
  ci->top.p = top;
  return ci;
}


/*
** precall for C functions
*/
l_sinline int precallC (sol_State *L, StkId func, int nresults,
                                            sol_CFunction f) {
  int n;  /* number of returns */
  CallInfo *ci;
  checkstackGCp(L, SOL_MINSTACK, func);  /* ensure minimum stack size */
  L->ci = ci = prepCallInfo(L, func, nresults, CIST_C,
                               L->top.p + SOL_MINSTACK);
  sol_assert(ci->top.p <= L->stack_last.p);
  if (l_unlikely(L->hookmask & SOL_MASKCALL)) {
    int narg = cast_int(L->top.p - func) - 1;
    solD_hook(L, SOL_HOOKCALL, -1, 1, narg);
  }
  sol_unlock(L);
  n = (*f)(L);  /* do the actual call */
  sol_lock(L);
  api_checknelems(L, n);
  solD_poscall(L, ci, n);
  return n;
}


/*
** Prepare a function for a tail call, building its call info on top
** of the current call info. 'narg1' is the number of arguments plus 1
** (so that it includes the function itself). Return the number of
** results, if it was a C function, or -1 for a Sol function.
*/
int solD_pretailcall (sol_State *L, CallInfo *ci, StkId func,
                                    int narg1, int delta) {
 retry:
  switch (ttypetag(s2v(func))) {
    case SOL_VCCL:  /* C closure */
      return precallC(L, func, SOL_MULTRET, clCvalue(s2v(func))->f);
    case SOL_VLCF:  /* light C function */
      return precallC(L, func, SOL_MULTRET, fvalue(s2v(func)));
    case SOL_VLCL: {  /* Sol function */
      Proto *p = clLvalue(s2v(func))->p;
      int fsize = p->maxstacksize;  /* frame size */
      int nfixparams = p->numparams;
      int i;
      checkstackGCp(L, fsize - delta, func);
      ci->func.p -= delta;  /* restore 'func' (if vararg) */
      for (i = 0; i < narg1; i++)  /* move down function and arguments */
        setobjs2s(L, ci->func.p + i, func + i);
      func = ci->func.p;  /* moved-down function */
      for (; narg1 <= nfixparams; narg1++)
        setnilvalue(s2v(func + narg1));  /* complete missing arguments */
      ci->top.p = func + 1 + fsize;  /* top for new function */
      sol_assert(ci->top.p <= L->stack_last.p);
      ci->u.l.savedpc = p->code;  /* starting point */
      ci->callstatus |= CIST_TAIL;
      L->top.p = func + narg1;  /* set top */
      return -1;
    }
    default: {  /* not a function */
      func = tryfuncTM(L, func);  /* try to get '__call' metamethod */
      /* return solD_pretailcall(L, ci, func, narg1 + 1, delta); */
      narg1++;
      goto retry;  /* try again */
    }
  }
}


/*
** Prepares the call to a function (C or Sol). For C functions, also do
** the call. The function to be called is at '*func'.  The arguments
** are on the stack, right after the function.  Returns the CallInfo
** to be executed, if it was a Sol function. Otherwise (a C function)
** returns NULL, with all the results on the stack, starting at the
** original function position.
*/
CallInfo *solD_precall (sol_State *L, StkId func, int nresults) {
 retry:
  switch (ttypetag(s2v(func))) {
    case SOL_VCCL:  /* C closure */
      precallC(L, func, nresults, clCvalue(s2v(func))->f);
      return NULL;
    case SOL_VLCF:  /* light C function */
      precallC(L, func, nresults, fvalue(s2v(func)));
      return NULL;
    case SOL_VLCL: {  /* Sol function */
      CallInfo *ci;
      Proto *p = clLvalue(s2v(func))->p;
      int narg = cast_int(L->top.p - func) - 1;  /* number of real arguments */
      int nfixparams = p->numparams;
      int fsize = p->maxstacksize;  /* frame size */
      checkstackGCp(L, fsize, func);
      L->ci = ci = prepCallInfo(L, func, nresults, 0, func + 1 + fsize);
      ci->u.l.savedpc = p->code;  /* starting point */
      for (; narg < nfixparams; narg++)
        setnilvalue(s2v(L->top.p++));  /* complete missing arguments */
      sol_assert(ci->top.p <= L->stack_last.p);
      return ci;
    }
    default: {  /* not a function */
      func = tryfuncTM(L, func);  /* try to get '__call' metamethod */
      /* return solD_precall(L, func, nresults); */
      goto retry;  /* try again with metamethod */
    }
  }
}


/*
** Call a function (C or Sol) through C. 'inc' can be 1 (increment
** number of recursive invocations in the C stack) or nyci (the same
** plus increment number of non-yieldable calls).
** This function can be called with some use of EXTRA_STACK, so it should
** check the stack before doing anything else. 'solD_precall' already
** does that.
*/
l_sinline void ccall (sol_State *L, StkId func, int nResults, l_uint32 inc) {
  CallInfo *ci;
  L->nCcalls += inc;
  if (l_unlikely(getCcalls(L) >= SOLI_MAXCCALLS)) {
    checkstackp(L, 0, func);  /* free any use of EXTRA_STACK */
    solE_checkcstack(L);
  }
  if ((ci = solD_precall(L, func, nResults)) != NULL) {  /* Sol function? */
    ci->callstatus = CIST_FRESH;  /* mark that it is a "fresh" execute */
    solV_execute(L, ci);  /* call it */
  }
  L->nCcalls -= inc;
}


/*
** External interface for 'ccall'
*/
void solD_call (sol_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, 1);
}


/*
** Similar to 'solD_call', but does not allow yields during the call.
*/
void solD_callnoyield (sol_State *L, StkId func, int nResults) {
  ccall(L, func, nResults, nyci);
}


/*
** Finish the job of 'sol_pcallk' after it was interrupted by an yield.
** (The caller, 'finishCcall', does the final call to 'adjustresults'.)
** The main job is to complete the 'solD_pcall' called by 'sol_pcallk'.
** If a '__close' method yields here, eventually control will be back
** to 'finishCcall' (when that '__close' method finally returns) and
** 'finishpcallk' will run again and close any still pending '__close'
** methods. Similarly, if a '__close' method errs, 'precover' calls
** 'unroll' which calls ''finishCcall' and we are back here again, to
** close any pending '__close' methods.
** Note that, up to the call to 'solF_close', the corresponding
** 'CallInfo' is not modified, so that this repeated run works like the
** first one (except that it has at least one less '__close' to do). In
** particular, field CIST_RECST preserves the error status across these
** multiple runs, changing only if there is a new error.
*/
static int finishpcallk (sol_State *L,  CallInfo *ci) {
  int status = getcistrecst(ci);  /* get original status */
  if (l_likely(status == SOL_OK))  /* no error? */
    status = SOL_YIELD;  /* was interrupted by an yield */
  else {  /* error */
    StkId func = restorestack(L, ci->u2.funcidx);
    L->allowhook = getoah(ci->callstatus);  /* restore 'allowhook' */
    func = solF_close(L, func, status, 1);  /* can yield or raise an error */
    solD_seterrorobj(L, status, func);
    solD_shrinkstack(L);   /* restore stack size in case of overflow */
    setcistrecst(ci, SOL_OK);  /* clear original status */
  }
  ci->callstatus &= ~CIST_YPCALL;
  L->errfunc = ci->u.c.old_errfunc;
  /* if it is here, there were errors or yields; unlike 'sol_pcallk',
     do not change status */
  return status;
}


/*
** Completes the execution of a C function interrupted by an yield.
** The interruption must have happened while the function was either
** closing its tbc variables in 'moveresults' or executing
** 'sol_callk'/'sol_pcallk'. In the first case, it just redoes
** 'solD_poscall'. In the second case, the call to 'finishpcallk'
** finishes the interrupted execution of 'sol_pcallk'.  After that, it
** calls the continuation of the interrupted function and finally it
** completes the job of the 'solD_call' that called the function.  In
** the call to 'adjustresults', we do not know the number of results
** of the function called by 'sol_callk'/'sol_pcallk', so we are
** conservative and use SOL_MULTRET (always adjust).
*/
static void finishCcall (sol_State *L, CallInfo *ci) {
  int n;  /* actual number of results from C function */
  if (ci->callstatus & CIST_CLSRET) {  /* was returning? */
    sol_assert(hastocloseCfunc(ci->nresults));
    n = ci->u2.nres;  /* just redo 'solD_poscall' */
    /* don't need to reset CIST_CLSRET, as it will be set again anyway */
  }
  else {
    int status = SOL_YIELD;  /* default if there were no errors */
    /* must have a continuation and must be able to call it */
    sol_assert(ci->u.c.k != NULL && yieldable(L));
    if (ci->callstatus & CIST_YPCALL)   /* was inside a 'sol_pcallk'? */
      status = finishpcallk(L, ci);  /* finish it */
    adjustresults(L, SOL_MULTRET);  /* finish 'sol_callk' */
    sol_unlock(L);
    n = (*ci->u.c.k)(L, status, ci->u.c.ctx);  /* call continuation */
    sol_lock(L);
    api_checknelems(L, n);
  }
  solD_poscall(L, ci, n);  /* finish 'solD_call' */
}


/*
** Executes "full continuation" (everything in the stack) of a
** previously interrupted coroutine until the stack is empty (or another
** interruption long-jumps out of the loop).
*/
static void unroll (sol_State *L, void *ud) {
  CallInfo *ci;
  UNUSED(ud);
  while ((ci = L->ci) != &L->base_ci) {  /* something in the stack */
    if (!isSol(ci))  /* C function? */
      finishCcall(L, ci);  /* complete its execution */
    else {  /* Sol function */
      solV_finishOp(L);  /* finish interrupted instruction */
      solV_execute(L, ci);  /* execute down to higher C 'boundary' */
    }
  }
}


/*
** Try to find a suspended protected call (a "recover point") for the
** given thread.
*/
static CallInfo *findpcall (sol_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {  /* search for a pcall */
    if (ci->callstatus & CIST_YPCALL)
      return ci;
  }
  return NULL;  /* no pending pcall */
}


/*
** Signal an error in the call to 'sol_resume', not in the execution
** of the coroutine itself. (Such errors should not be handled by any
** coroutine error handler and should not kill the coroutine.)
*/
static int resume_error (sol_State *L, const char *msg, int narg) {
  L->top.p -= narg;  /* remove args from the stack */
  setsvalue2s(L, L->top.p, solS_new(L, msg));  /* push error message */
  api_incr_top(L);
  sol_unlock(L);
  return SOL_ERRRUN;
}


/*
** Do the work for 'sol_resume' in protected mode. Most of the work
** depends on the status of the coroutine: initial state, suspended
** inside a hook, or regularly suspended (optionally with a continuation
** function), plus erroneous cases: non-suspended coroutine or dead
** coroutine.
*/
static void resume (sol_State *L, void *ud) {
  int n = *(cast(int*, ud));  /* number of arguments */
  StkId firstArg = L->top.p - n;  /* first argument */
  CallInfo *ci = L->ci;
  if (L->status == SOL_OK)  /* starting a coroutine? */
    ccall(L, firstArg - 1, SOL_MULTRET, 0);  /* just call its body */
  else {  /* resuming from previous yield */
    sol_assert(L->status == SOL_YIELD);
    L->status = SOL_OK;  /* mark that it is running (again) */
    if (isSol(ci)) {  /* yielded inside a hook? */
      /* undo increment made by 'solG_traceexec': instruction was not
         executed yet */
      sol_assert(ci->callstatus & CIST_HOOKYIELD);
      ci->u.l.savedpc--;
      L->top.p = firstArg;  /* discard arguments */
      solV_execute(L, ci);  /* just continue running Sol code */
    }
    else {  /* 'common' yield */
      if (ci->u.c.k != NULL) {  /* does it have a continuation function? */
        sol_unlock(L);
        n = (*ci->u.c.k)(L, SOL_YIELD, ci->u.c.ctx); /* call continuation */
        sol_lock(L);
        api_checknelems(L, n);
      }
      solD_poscall(L, ci, n);  /* finish 'solD_call' */
    }
    unroll(L, NULL);  /* run continuation */
  }
}


/*
** Unrolls a coroutine in protected mode while there are recoverable
** errors, that is, errors inside a protected call. (Any error
** interrupts 'unroll', and this loop protects it again so it can
** continue.) Stops with a normal end (status == SOL_OK), an yield
** (status == SOL_YIELD), or an unprotected error ('findpcall' doesn't
** find a recover point).
*/
static int precover (sol_State *L, int status) {
  CallInfo *ci;
  while (errorstatus(status) && (ci = findpcall(L)) != NULL) {
    L->ci = ci;  /* go down to recovery functions */
    setcistrecst(ci, status);  /* status to finish 'pcall' */
    status = solD_rawrunprotected(L, unroll, NULL);
  }
  return status;
}


SOL_API int sol_resume (sol_State *L, sol_State *from, int nargs,
                                      int *nresults) {
  int status;
  sol_lock(L);
  if (L->status == SOL_OK) {  /* may be starting a coroutine */
    if (L->ci != &L->base_ci)  /* not in base level? */
      return resume_error(L, "cannot resume non-suspended coroutine", nargs);
    else if (L->top.p - (L->ci->func.p + 1) == nargs)  /* no function? */
      return resume_error(L, "cannot resume dead coroutine", nargs);
  }
  else if (L->status != SOL_YIELD)  /* ended with errors? */
    return resume_error(L, "cannot resume dead coroutine", nargs);
  L->nCcalls = (from) ? getCcalls(from) : 0;
  if (getCcalls(L) >= SOLI_MAXCCALLS)
    return resume_error(L, "C stack overflow", nargs);
  L->nCcalls++;
  soli_userstateresume(L, nargs);
  api_checknelems(L, (L->status == SOL_OK) ? nargs + 1 : nargs);
  status = solD_rawrunprotected(L, resume, &nargs);
   /* continue running after recoverable errors */
  status = precover(L, status);
  if (l_likely(!errorstatus(status)))
    sol_assert(status == L->status);  /* normal end or yield */
  else {  /* unrecoverable error */
    L->status = cast_byte(status);  /* mark thread as 'dead' */
    solD_seterrorobj(L, status, L->top.p);  /* push error message */
    L->ci->top.p = L->top.p;
  }
  *nresults = (status == SOL_YIELD) ? L->ci->u2.nyield
                                    : cast_int(L->top.p - (L->ci->func.p + 1));
  sol_unlock(L);
  return status;
}


SOL_API int sol_isyieldable (sol_State *L) {
  return yieldable(L);
}


SOL_API int sol_yieldk (sol_State *L, int nresults, sol_KContext ctx,
                        sol_KFunction k) {
  CallInfo *ci;
  soli_userstateyield(L, nresults);
  sol_lock(L);
  ci = L->ci;
  api_checknelems(L, nresults);
  if (l_unlikely(!yieldable(L))) {
    if (L != G(L)->mainthread)
      solG_runerror(L, "attempt to yield across a C-call boundary");
    else
      solG_runerror(L, "attempt to yield from outside a coroutine");
  }
  L->status = SOL_YIELD;
  ci->u2.nyield = nresults;  /* save number of results */
  if (isSol(ci)) {  /* inside a hook? */
    sol_assert(!isSolcode(ci));
    api_check(L, nresults == 0, "hooks cannot yield values");
    api_check(L, k == NULL, "hooks cannot continue after yielding");
  }
  else {
    if ((ci->u.c.k = k) != NULL)  /* is there a continuation? */
      ci->u.c.ctx = ctx;  /* save context */
    solD_throw(L, SOL_YIELD);
  }
  sol_assert(ci->callstatus & CIST_HOOKED);  /* must be inside a hook */
  sol_unlock(L);
  return 0;  /* return to 'solD_hook' */
}


/*
** Auxiliary structure to call 'solF_close' in protected mode.
*/
struct CloseP {
  StkId level;
  int status;
};


/*
** Auxiliary function to call 'solF_close' in protected mode.
*/
static void closepaux (sol_State *L, void *ud) {
  struct CloseP *pcl = cast(struct CloseP *, ud);
  solF_close(L, pcl->level, pcl->status, 0);
}


/*
** Calls 'solF_close' in protected mode. Return the original status
** or, in case of errors, the new status.
*/
int solD_closeprotected (sol_State *L, ptrdiff_t level, int status) {
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  for (;;) {  /* keep closing upvalues until no more errors */
    struct CloseP pcl;
    pcl.level = restorestack(L, level); pcl.status = status;
    status = solD_rawrunprotected(L, &closepaux, &pcl);
    if (l_likely(status == SOL_OK))  /* no more errors? */
      return pcl.status;
    else {  /* an error occurred; restore saved state and repeat */
      L->ci = old_ci;
      L->allowhook = old_allowhooks;
    }
  }
}


/*
** Call the C function 'func' in protected mode, restoring basic
** thread information ('allowhook', etc.) and in particular
** its stack level in case of errors.
*/
int solD_pcall (sol_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = solD_rawrunprotected(L, func, u);
  if (l_unlikely(status != SOL_OK)) {  /* an error occurred? */
    L->ci = old_ci;
    L->allowhook = old_allowhooks;
    status = solD_closeprotected(L, old_top, status);
    solD_seterrorobj(L, status, restorestack(L, old_top));
    solD_shrinkstack(L);   /* restore stack size in case of overflow */
  }
  L->errfunc = old_errfunc;
  return status;
}



/*
** Execute a protected parser.
*/
struct SParser {  /* data to 'f_parser' */
  ZIO *z;
  Mbuffer buff;  /* dynamic structure used by the scanner */
  Dyndata dyd;  /* dynamic structures used by the parser */
  const char *mode;
  const char *name;
};


static void checkmode (sol_State *L, const char *mode, const char *x) {
  if (mode && strchr(mode, x[0]) == NULL) {
    solO_pushfstring(L,
       "attempt to load a %s chunk (mode is '%s')", x, mode);
    solD_throw(L, SOL_ERRSYNTAX);
  }
}


static void f_parser (sol_State *L, void *ud) {
  LClosure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = zgetc(p->z);  /* read first character */
  if (c == SOL_SIGNATURE[0]) {
    checkmode(L, p->mode, "binary");
    cl = solU_undump(L, p->z, p->name);
  }
  else {
    checkmode(L, p->mode, "text");
    cl = solY_parser(L, p->z, &p->buff, &p->dyd, p->name, c);
  }
  sol_assert(cl->nupvalues == cl->p->sizeupvalues);
  solF_initupvals(L, cl);
}


int solD_protectedparser (sol_State *L, ZIO *z, const char *name,
                                        const char *mode) {
  struct SParser p;
  int status;
  incnny(L);  /* cannot yield during parsing */
  p.z = z; p.name = name; p.mode = mode;
  p.dyd.actvar.arr = NULL; p.dyd.actvar.size = 0;
  p.dyd.gt.arr = NULL; p.dyd.gt.size = 0;
  p.dyd.label.arr = NULL; p.dyd.label.size = 0;
  solZ_initbuffer(L, &p.buff);
  status = solD_pcall(L, f_parser, &p, savestack(L, L->top.p), L->errfunc);
  solZ_freebuffer(L, &p.buff);
  solM_freearray(L, p.dyd.actvar.arr, p.dyd.actvar.size);
  solM_freearray(L, p.dyd.gt.arr, p.dyd.gt.size);
  solM_freearray(L, p.dyd.label.arr, p.dyd.label.size);
  decnny(L);
  return status;
}


