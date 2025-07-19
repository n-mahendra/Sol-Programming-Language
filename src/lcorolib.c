/*
** $Id: lcorolib.c $
** Coroutine Library
** See Copyright Notice in sol.h
*/

#define lcorolib_c
#define SOL_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "sol.h"

#include "lauxlib.h"
#include "sollib.h"


static sol_State *getco (sol_State *L) {
  sol_State *co = sol_tothread(L, 1);
  solL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (sol_State *L, sol_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!sol_checkstack(co, narg))) {
    sol_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  sol_xmove(L, co, narg);
  status = sol_resume(co, L, narg, &nres);
  if (l_likely(status == SOL_OK || status == SOL_YIELD)) {
    if (l_unlikely(!sol_checkstack(L, nres + 1))) {
      sol_pop(co, nres);  /* remove results anyway */
      sol_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    sol_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    sol_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int solB_coresume (sol_State *L) {
  sol_State *co = getco(L);
  int r;
  r = auxresume(L, co, sol_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    sol_pushboolean(L, 0);
    sol_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    sol_pushboolean(L, 1);
    sol_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int solB_auxwrap (sol_State *L) {
  sol_State *co = sol_tothread(L, sol_upvalueindex(1));
  int r = auxresume(L, co, sol_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = sol_status(co);
    if (stat != SOL_OK && stat != SOL_YIELD) {  /* error in the coroutine? */
      stat = sol_closethread(co, L);  /* close its tbc variables */
      sol_assert(stat != SOL_OK);
      sol_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != SOL_ERRMEM &&  /* not a memory error and ... */
        sol_type(L, -1) == SOL_TSTRING) {  /* ... error object is a string? */
      solL_where(L, 1);  /* add extra info, if available */
      sol_insert(L, -2);
      sol_concat(L, 2);
    }
    return sol_error(L);  /* propagate error */
  }
  return r;
}


static int solB_cocreate (sol_State *L) {
  sol_State *NL;
  solL_checktype(L, 1, SOL_TFUNCTION);
  NL = sol_newthread(L);
  sol_pushvalue(L, 1);  /* move function to top */
  sol_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int solB_cowrap (sol_State *L) {
  solB_cocreate(L);
  sol_pushcclosure(L, solB_auxwrap, 1);
  return 1;
}


static int solB_yield (sol_State *L) {
  return sol_yield(L, sol_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (sol_State *L, sol_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (sol_status(co)) {
      case SOL_YIELD:
        return COS_YIELD;
      case SOL_OK: {
        sol_Debug ar;
        if (sol_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (sol_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int solB_costatus (sol_State *L) {
  sol_State *co = getco(L);
  sol_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int solB_yieldable (sol_State *L) {
  sol_State *co = sol_isnone(L, 1) ? L : getco(L);
  sol_pushboolean(L, sol_isyieldable(co));
  return 1;
}


static int solB_corunning (sol_State *L) {
  int ismain = sol_pushthread(L);
  sol_pushboolean(L, ismain);
  return 2;
}


static int solB_close (sol_State *L) {
  sol_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = sol_closethread(co, L);
      if (status == SOL_OK) {
        sol_pushboolean(L, 1);
        return 1;
      }
      else {
        sol_pushboolean(L, 0);
        sol_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      return solL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const solL_Reg co_funcs[] = {
  {"create", solB_cocreate},
  {"resume", solB_coresume},
  {"running", solB_corunning},
  {"status", solB_costatus},
  {"wrap", solB_cowrap},
  {"yield", solB_yield},
  {"isyieldable", solB_yieldable},
  {"close", solB_close},
  {NULL, NULL}
};



SOLMOD_API int solopen_coroutine (sol_State *L) {
  solL_newlib(L, co_funcs);
  return 1;
}

