/*
** $Id: ldblib.c $
** Interface from Sol to its debug API
** See Copyright Notice in sol.h
*/

#define ldblib_c
#define SOL_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sol.h"

#include "lauxlib.h"
#include "sollib.h"


/*
** The hook table at registry[HOOKKEY] maps threads to their current
** hook function.
*/
static const char *const HOOKKEY = "_HOOKKEY";


/*
** If L1 != L, L1 can be in any state, and therefore there are no
** guarantees about its stack space; any push in L1 must be
** checked.
*/
static void checkstack (sol_State *L, sol_State *L1, int n) {
  if (l_unlikely(L != L1 && !sol_checkstack(L1, n)))
    solL_error(L, "stack overflow");
}


static int db_getregistry (sol_State *L) {
  sol_pushvalue(L, SOL_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (sol_State *L) {
  solL_checkany(L, 1);
  if (!sol_getmetatable(L, 1)) {
    sol_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (sol_State *L) {
  int t = sol_type(L, 2);
  solL_argexpected(L, t == SOL_TNIL || t == SOL_TTABLE, 2, "nil or table");
  sol_settop(L, 2);
  sol_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (sol_State *L) {
  int n = (int)solL_optinteger(L, 2, 1);
  if (sol_type(L, 1) != SOL_TUSERDATA)
    solL_pushfail(L);
  else if (sol_getiuservalue(L, 1, n) != SOL_TNONE) {
    sol_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (sol_State *L) {
  int n = (int)solL_optinteger(L, 3, 1);
  solL_checktype(L, 1, SOL_TUSERDATA);
  solL_checkany(L, 2);
  sol_settop(L, 2);
  if (!sol_setiuservalue(L, 1, n))
    solL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static sol_State *getthread (sol_State *L, int *arg) {
  if (sol_isthread(L, 1)) {
    *arg = 1;
    return sol_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'sol_settable', used by 'db_getinfo' to put results
** from 'sol_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (sol_State *L, const char *k, const char *v) {
  sol_pushstring(L, v);
  sol_setfield(L, -2, k);
}

static void settabsi (sol_State *L, const char *k, int v) {
  sol_pushinteger(L, v);
  sol_setfield(L, -2, k);
}

static void settabsb (sol_State *L, const char *k, int v) {
  sol_pushboolean(L, v);
  sol_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'sol_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'sol_getinfo' on top of the result table so that it can call
** 'sol_setfield'.
*/
static void treatstackoption (sol_State *L, sol_State *L1, const char *fname) {
  if (L == L1)
    sol_rotate(L, -2, 1);  /* exchange object and table */
  else
    sol_xmove(L1, L, 1);  /* move object to the "main" stack */
  sol_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'sol_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'sol_getinfo'.
*/
static int db_getinfo (sol_State *L) {
  sol_Debug ar;
  int arg;
  sol_State *L1 = getthread(L, &arg);
  const char *options = solL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  solL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (sol_isfunction(L, arg + 1)) {  /* info about a function? */
    options = sol_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    sol_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    sol_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!sol_getstack(L1, (int)solL_checkinteger(L, arg + 1), &ar)) {
      solL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!sol_getinfo(L1, options, &ar))
    return solL_argerror(L, arg+2, "invalid option");
  sol_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    sol_pushlstring(L, ar.source, ar.srclen);
    sol_setfield(L, -2, "source");
    settabss(L, "short_src", ar.short_src);
    settabsi(L, "linedefined", ar.linedefined);
    settabsi(L, "lastlinedefined", ar.lastlinedefined);
    settabss(L, "what", ar.what);
  }
  if (strchr(options, 'l'))
    settabsi(L, "currentline", ar.currentline);
  if (strchr(options, 'u')) {
    settabsi(L, "nups", ar.nups);
    settabsi(L, "nparams", ar.nparams);
    settabsb(L, "isvararg", ar.isvararg);
  }
  if (strchr(options, 'n')) {
    settabss(L, "name", ar.name);
    settabss(L, "namewhat", ar.namewhat);
  }
  if (strchr(options, 'r')) {
    settabsi(L, "ftransfer", ar.ftransfer);
    settabsi(L, "ntransfer", ar.ntransfer);
  }
  if (strchr(options, 't'))
    settabsb(L, "istailcall", ar.istailcall);
  if (strchr(options, 'L'))
    treatstackoption(L, L1, "activelines");
  if (strchr(options, 'f'))
    treatstackoption(L, L1, "func");
  return 1;  /* return table */
}


static int db_getlocal (sol_State *L) {
  int arg;
  sol_State *L1 = getthread(L, &arg);
  int nvar = (int)solL_checkinteger(L, arg + 2);  /* local-variable index */
  if (sol_isfunction(L, arg + 1)) {  /* function argument? */
    sol_pushvalue(L, arg + 1);  /* push function */
    sol_pushstring(L, sol_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    sol_Debug ar;
    const char *name;
    int level = (int)solL_checkinteger(L, arg + 1);
    if (l_unlikely(!sol_getstack(L1, level, &ar)))  /* out of range? */
      return solL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = sol_getlocal(L1, &ar, nvar);
    if (name) {
      sol_xmove(L1, L, 1);  /* move local value */
      sol_pushstring(L, name);  /* push name */
      sol_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      solL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (sol_State *L) {
  int arg;
  const char *name;
  sol_State *L1 = getthread(L, &arg);
  sol_Debug ar;
  int level = (int)solL_checkinteger(L, arg + 1);
  int nvar = (int)solL_checkinteger(L, arg + 2);
  if (l_unlikely(!sol_getstack(L1, level, &ar)))  /* out of range? */
    return solL_argerror(L, arg+1, "level out of range");
  solL_checkany(L, arg+3);
  sol_settop(L, arg+3);
  checkstack(L, L1, 1);
  sol_xmove(L, L1, 1);
  name = sol_setlocal(L1, &ar, nvar);
  if (name == NULL)
    sol_pop(L1, 1);  /* pop value (if not popped by 'sol_setlocal') */
  sol_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (sol_State *L, int get) {
  const char *name;
  int n = (int)solL_checkinteger(L, 2);  /* upvalue index */
  solL_checktype(L, 1, SOL_TFUNCTION);  /* closure */
  name = get ? sol_getupvalue(L, 1, n) : sol_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  sol_pushstring(L, name);
  sol_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (sol_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (sol_State *L) {
  solL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (sol_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)solL_checkinteger(L, argnup);  /* upvalue index */
  solL_checktype(L, argf, SOL_TFUNCTION);  /* closure */
  id = sol_upvalueid(L, argf, nup);
  if (pnup) {
    solL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (sol_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    sol_pushlightuserdata(L, id);
  else
    solL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (sol_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  solL_argcheck(L, !sol_iscfunction(L, 1), 1, "Sol function expected");
  solL_argcheck(L, !sol_iscfunction(L, 3), 3, "Sol function expected");
  sol_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (sol_State *L, sol_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  sol_getfield(L, SOL_REGISTRYINDEX, HOOKKEY);
  sol_pushthread(L);
  if (sol_rawget(L, -2) == SOL_TFUNCTION) {  /* is there a hook function? */
    sol_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      sol_pushinteger(L, ar->currentline);  /* push current line */
    else sol_pushnil(L);
    sol_assert(sol_getinfo(L, "lS", ar));
    sol_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= SOL_MASKCALL;
  if (strchr(smask, 'r')) mask |= SOL_MASKRET;
  if (strchr(smask, 'l')) mask |= SOL_MASKLINE;
  if (count > 0) mask |= SOL_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & SOL_MASKCALL) smask[i++] = 'c';
  if (mask & SOL_MASKRET) smask[i++] = 'r';
  if (mask & SOL_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (sol_State *L) {
  int arg, mask, count;
  sol_Hook func;
  sol_State *L1 = getthread(L, &arg);
  if (sol_isnoneornil(L, arg+1)) {  /* no hook? */
    sol_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = solL_checkstring(L, arg+2);
    solL_checktype(L, arg+1, SOL_TFUNCTION);
    count = (int)solL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!solL_getsubtable(L, SOL_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    sol_pushliteral(L, "k");
    sol_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    sol_pushvalue(L, -1);
    sol_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  sol_pushthread(L1); sol_xmove(L1, L, 1);  /* key (thread) */
  sol_pushvalue(L, arg + 1);  /* value (hook function) */
  sol_rawset(L, -3);  /* hooktable[L1] = new Sol hook */
  sol_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (sol_State *L) {
  int arg;
  sol_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = sol_gethookmask(L1);
  sol_Hook hook = sol_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    solL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    sol_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    sol_getfield(L, SOL_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    sol_pushthread(L1); sol_xmove(L1, L, 1);
    sol_rawget(L, -2);   /* 1st result = hooktable[L1] */
    sol_remove(L, -2);  /* remove hook table */
  }
  sol_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  sol_pushinteger(L, sol_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (sol_State *L) {
  for (;;) {
    char buffer[250];
    sol_writestringerror("%s", "sol_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (solL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        sol_pcall(L, 0, 0, 0))
      sol_writestringerror("%s\n", solL_tolstring(L, -1, NULL));
    sol_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (sol_State *L) {
  int arg;
  sol_State *L1 = getthread(L, &arg);
  const char *msg = sol_tostring(L, arg + 1);
  if (msg == NULL && !sol_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    sol_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)solL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    solL_traceback(L, L1, msg, level);
  }
  return 1;
}


static int db_setcstacklimit (sol_State *L) {
  int limit = (int)solL_checkinteger(L, 1);
  int res = sol_setcstacklimit(L, limit);
  sol_pushinteger(L, res);
  return 1;
}


static const solL_Reg dblib[] = {
  {"debug", db_debug},
  {"getuservalue", db_getuservalue},
  {"gethook", db_gethook},
  {"getinfo", db_getinfo},
  {"getlocal", db_getlocal},
  {"getregistry", db_getregistry},
  {"getmetatable", db_getmetatable},
  {"getupvalue", db_getupvalue},
  {"upvaluejoin", db_upvaluejoin},
  {"upvalueid", db_upvalueid},
  {"setuservalue", db_setuservalue},
  {"sethook", db_sethook},
  {"setlocal", db_setlocal},
  {"setmetatable", db_setmetatable},
  {"setupvalue", db_setupvalue},
  {"traceback", db_traceback},
  {"setcstacklimit", db_setcstacklimit},
  {NULL, NULL}
};


SOLMOD_API int solopen_debug (sol_State *L) {
  solL_newlib(L, dblib);
  return 1;
}

