/*
** $Id: lstate.c $
** Global State
** See Copyright Notice in sol.h
*/

#define lstate_c
#define SOL_CORE

#include "lprefix.h"


#include <stddef.h>
#include <string.h>

#include "sol.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"



/*
** thread state + extra space
*/
typedef struct LX {
  lu_byte extra_[SOL_EXTRASPACE];
  sol_State l;
} LX;


/*
** Main thread combines a thread state and the global state
*/
typedef struct LG {
  LX l;
  global_State g;
} LG;



#define fromstate(L)	(cast(LX *, cast(lu_byte *, (L)) - offsetof(LX, l)))


/*
** A macro to create a "random" seed when a state is created;
** the seed is used to randomize string hashes.
*/
#if !defined(soli_makeseed)

#include <time.h>

/*
** Compute an initial seed with some level of randomness.
** Rely on Address Space Layout Randomization (if present) and
** current time.
*/
#define addbuff(b,p,e) \
  { size_t t = cast_sizet(e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int soli_makeseed (sol_State *L) {
  char buff[3 * sizeof(size_t)];
  unsigned int h = cast_uint(time(NULL));
  int p = 0;
  addbuff(buff, p, L);  /* heap variable */
  addbuff(buff, p, &h);  /* local variable */
  addbuff(buff, p, &sol_newstate);  /* public function */
  sol_assert(p == sizeof(buff));
  return solS_hash(buff, p, h);
}

#endif


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void solE_setdebt (global_State *g, l_mem debt) {
  l_mem tb = gettotalbytes(g);
  sol_assert(tb > 0);
  if (debt < tb - MAX_LMEM)
    debt = tb - MAX_LMEM;  /* will make 'totalbytes == MAX_LMEM' */
  g->totalbytes = tb - debt;
  g->GCdebt = debt;
}


SOL_API int sol_setcstacklimit (sol_State *L, unsigned int limit) {
  UNUSED(L); UNUSED(limit);
  return SOLI_MAXCCALLS;  /* warning?? */
}


CallInfo *solE_extendCI (sol_State *L) {
  CallInfo *ci;
  sol_assert(L->ci->next == NULL);
  ci = solM_new(L, CallInfo);
  sol_assert(L->ci->next == NULL);
  L->ci->next = ci;
  ci->previous = L->ci;
  ci->next = NULL;
  ci->u.l.trap = 0;
  L->nci++;
  return ci;
}


/*
** free all CallInfo structures not in use by a thread
*/
static void freeCI (sol_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    solM_free(L, ci);
    L->nci--;
  }
}


/*
** free half of the CallInfo structures not in use by a thread,
** keeping the first one.
*/
void solE_shrinkCI (sol_State *L) {
  CallInfo *ci = L->ci->next;  /* first free CallInfo */
  CallInfo *next;
  if (ci == NULL)
    return;  /* no extra elements */
  while ((next = ci->next) != NULL) {  /* two extra elements? */
    CallInfo *next2 = next->next;  /* next's next */
    ci->next = next2;  /* remove next from the list */
    L->nci--;
    solM_free(L, next);  /* free next */
    if (next2 == NULL)
      break;  /* no more elements */
    else {
      next2->previous = ci;
      ci = next2;  /* continue */
    }
  }
}


/*
** Called when 'getCcalls(L)' larger or equal to SOLI_MAXCCALLS.
** If equal, raises an overflow error. If value is larger than
** SOLI_MAXCCALLS (which means it is handling an overflow) but
** not much larger, does not report an error (to allow overflow
** handling to work).
*/
void solE_checkcstack (sol_State *L) {
  if (getCcalls(L) == SOLI_MAXCCALLS)
    solG_runerror(L, "C stack overflow");
  else if (getCcalls(L) >= (SOLI_MAXCCALLS / 10 * 11))
    solD_errerr(L);  /* error while handling stack error */
}


SOLI_FUNC void solE_incCstack (sol_State *L) {
  L->nCcalls++;
  if (l_unlikely(getCcalls(L) >= SOLI_MAXCCALLS))
    solE_checkcstack(L);
}


static void stack_init (sol_State *L1, sol_State *L) {
  int i; CallInfo *ci;
  /* initialize stack array */
  L1->stack.p = solM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
  L1->tbclist.p = L1->stack.p;
  for (i = 0; i < BASIC_STACK_SIZE + EXTRA_STACK; i++)
    setnilvalue(s2v(L1->stack.p + i));  /* erase new stack */
  L1->top.p = L1->stack.p;
  L1->stack_last.p = L1->stack.p + BASIC_STACK_SIZE;
  /* initialize first ci */
  ci = &L1->base_ci;
  ci->next = ci->previous = NULL;
  ci->callstatus = CIST_C;
  ci->func.p = L1->top.p;
  ci->u.c.k = NULL;
  ci->nresults = 0;
  setnilvalue(s2v(L1->top.p));  /* 'function' entry for this 'ci' */
  L1->top.p++;
  ci->top.p = L1->top.p + SOL_MINSTACK;
  L1->ci = ci;
}


static void freestack (sol_State *L) {
  if (L->stack.p == NULL)
    return;  /* stack not completely built yet */
  L->ci = &L->base_ci;  /* free the entire 'ci' list */
  freeCI(L);
  sol_assert(L->nci == 0);
  solM_freearray(L, L->stack.p, stacksize(L) + EXTRA_STACK);  /* free stack */
}


/*
** Create registry table and its predefined values
*/
static void init_registry (sol_State *L, global_State *g) {
  /* create registry */
  Table *registry = solH_new(L);
  sethvalue(L, &g->l_registry, registry);
  solH_resize(L, registry, SOL_RIDX_LAST, 0);
  /* registry[SOL_RIDX_MAINTHREAD] = L */
  setthvalue(L, &registry->array[SOL_RIDX_MAINTHREAD - 1], L);
  /* registry[SOL_RIDX_GLOBALS] = new table (table of globals) */
  sethvalue(L, &registry->array[SOL_RIDX_GLOBALS - 1], solH_new(L));
}


/*
** open parts of the state that may cause memory-allocation errors.
*/
static void f_solopen (sol_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack */
  init_registry(L, g);
  solS_init(L);
  solT_init(L);
  solX_init(L);
  g->gcstp = 0;  /* allow gc */
  setnilvalue(&g->nilvalue);  /* now state is complete */
  soli_userstateopen(L);
}


/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread (sol_State *L, global_State *g) {
  G(L) = g;
  L->stack.p = NULL;
  L->ci = NULL;
  L->nci = 0;
  L->twups = L;  /* thread has no upvalues */
  L->nCcalls = 0;
  L->errorJmp = NULL;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->status = SOL_OK;
  L->errfunc = 0;
  L->oldpc = 0;
}


static void close_state (sol_State *L) {
  global_State *g = G(L);
  if (!completestate(g))  /* closing a partially built state? */
    solC_freeallobjects(L);  /* just collect its objects */
  else {  /* closing a fully built state */
    L->ci = &L->base_ci;  /* unwind CallInfo list */
    L->errfunc = 0;   /* stack unwind can "throw away" the error function */
    solD_closeprotected(L, 1, SOL_OK);  /* close all upvalues */
    L->top.p = L->stack.p + 1;  /* empty the stack to run finalizers */
    solC_freeallobjects(L);  /* collect all objects */
    soli_userstateclose(L);
  }
  solM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  freestack(L);
  sol_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}


SOL_API sol_State *sol_newthread (sol_State *L) {
  global_State *g = G(L);
  GCObject *o;
  sol_State *L1;
  sol_lock(L);
  solC_checkGC(L);
  /* create new thread */
  o = solC_newobjdt(L, SOL_TTHREAD, sizeof(LX), offsetof(LX, l));
  L1 = gco2th(o);
  /* anchor it on L stack */
  setthvalue2s(L, L->top.p, L1);
  api_incr_top(L);
  preinit_thread(L1, g);
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  /* initialize L1 extra space */
  memcpy(sol_getextraspace(L1), sol_getextraspace(g->mainthread),
         SOL_EXTRASPACE);
  soli_userstatethread(L, L1);
  stack_init(L1, L);  /* init stack */
  sol_unlock(L);
  return L1;
}


void solE_freethread (sol_State *L, sol_State *L1) {
  LX *l = fromstate(L1);
  solF_closeupval(L1, L1->stack.p);  /* close all upvalues */
  sol_assert(L1->openupval == NULL);
  soli_userstatefree(L, L1);
  freestack(L1);
  solM_free(L, l);
}


int solE_resetthread (sol_State *L, int status) {
  CallInfo *ci = L->ci = &L->base_ci;  /* unwind CallInfo list */
  setnilvalue(s2v(L->stack.p));  /* 'function' entry for basic 'ci' */
  ci->func.p = L->stack.p;
  ci->callstatus = CIST_C;
  if (status == SOL_YIELD)
    status = SOL_OK;
  L->status = SOL_OK;  /* so it can run __close metamethods */
  L->errfunc = 0;   /* stack unwind can "throw away" the error function */
  status = solD_closeprotected(L, 1, status);
  if (status != SOL_OK)  /* errors? */
    solD_seterrorobj(L, status, L->stack.p + 1);
  else
    L->top.p = L->stack.p + 1;
  ci->top.p = L->top.p + SOL_MINSTACK;
  solD_reallocstack(L, cast_int(ci->top.p - L->stack.p), 0);
  return status;
}


SOL_API int sol_closethread (sol_State *L, sol_State *from) {
  int status;
  sol_lock(L);
  L->nCcalls = (from) ? getCcalls(from) : 0;
  status = solE_resetthread(L, L->status);
  sol_unlock(L);
  return status;
}


/*
** Deprecated! Use 'sol_closethread' instead.
*/
SOL_API int sol_resetthread (sol_State *L) {
  return sol_closethread(L, NULL);
}


SOL_API sol_State *sol_newstate (sol_Alloc f, void *ud) {
  int i;
  sol_State *L;
  global_State *g;
  LG *l = cast(LG *, (*f)(ud, NULL, SOL_TTHREAD, sizeof(LG)));
  if (l == NULL) return NULL;
  L = &l->l.l;
  g = &l->g;
  L->tt = SOL_VTHREAD;
  g->currentwhite = bitmask(WHITE0BIT);
  L->marked = solC_white(g);
  preinit_thread(L, g);
  g->allgc = obj2gco(L);  /* by now, only object is the main thread */
  L->next = NULL;
  incnny(L);  /* main thread is always non yieldable */
  g->frealloc = f;
  g->ud = ud;
  g->warnf = NULL;
  g->ud_warn = NULL;
  g->mainthread = L;
  g->seed = soli_makeseed(L);
  g->gcstp = GCSTPGC;  /* no GC while building state */
  g->strt.size = g->strt.nuse = 0;
  g->strt.hash = NULL;
  setnilvalue(&g->l_registry);
  g->panic = NULL;
  g->gcstate = GCSpause;
  g->gckind = KGC_INC;
  g->gcstopem = 0;
  g->gcemergency = 0;
  g->finobj = g->tobefnz = g->fixedgc = NULL;
  g->firstold1 = g->survival = g->old1 = g->reallyold = NULL;
  g->finobjsur = g->finobjold1 = g->finobjrold = NULL;
  g->sweepgc = NULL;
  g->gray = g->grayagain = NULL;
  g->weak = g->ephemeron = g->allweak = NULL;
  g->twups = NULL;
  g->totalbytes = sizeof(LG);
  g->GCdebt = 0;
  g->lastatomic = 0;
  setivalue(&g->nilvalue, 0);  /* to signal that state is not yet built */
  setgcparam(g->gcpause, SOLI_GCPAUSE);
  setgcparam(g->gcstepmul, SOLI_GCMUL);
  g->gcstepsize = SOLI_GCSTEPSIZE;
  setgcparam(g->genmajormul, SOLI_GENMAJORMUL);
  g->genminormul = SOLI_GENMINORMUL;
  for (i=0; i < SOL_NUMTAGS; i++) g->mt[i] = NULL;
  if (solD_rawrunprotected(L, f_solopen, NULL) != SOL_OK) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  return L;
}


SOL_API void sol_close (sol_State *L) {
  sol_lock(L);
  L = G(L)->mainthread;  /* only the main thread can be closed */
  close_state(L);
}


void solE_warning (sol_State *L, const char *msg, int tocont) {
  sol_WarnFunction wf = G(L)->warnf;
  if (wf != NULL)
    wf(G(L)->ud_warn, msg, tocont);
}


/*
** Generate a warning from an error message
*/
void solE_warnerror (sol_State *L, const char *where) {
  TValue *errobj = s2v(L->top.p - 1);  /* error object */
  const char *msg = (ttisstring(errobj))
                  ? getstr(tsvalue(errobj))
                  : "error object is not a string";
  /* produce warning "error in %s (%s)" (where, msg) */
  solE_warning(L, "error in ", 1);
  solE_warning(L, where, 1);
  solE_warning(L, " (", 1);
  solE_warning(L, msg, 1);
  solE_warning(L, ")", 0);
}

