/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in sol.h
*/

#define lbaselib_c
#define SOL_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sol.h"

#include "lauxlib.h"
#include "sollib.h"


static int solB_print (sol_State *L) {
  int n = sol_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = solL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      sol_writestring("\t", 1);  /* add a tab before it */
    sol_writestring(s, l);  /* print it */
    sol_pop(L, 1);  /* pop result */
  }
  sol_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int solB_warn (sol_State *L) {
  int n = sol_gettop(L);  /* number of arguments */
  int i;
  solL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    solL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    sol_warning(L, sol_tostring(L, i), 1);
  sol_warning(L, sol_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, int base, sol_Integer *pn) {
  sol_Unsigned n = 0;
  int neg = 0;
  s += strspn(s, SPACECHARS);  /* skip initial spaces */
  if (*s == '-') { s++; neg = 1; }  /* handle sign */
  else if (*s == '+') s++;
  if (!isalnum((unsigned char)*s))  /* no digit? */
    return NULL;
  do {
    int digit = (isdigit((unsigned char)*s)) ? *s - '0'
                   : (toupper((unsigned char)*s) - 'A') + 10;
    if (digit >= base) return NULL;  /* invalid numeral */
    n = n * base + digit;
    s++;
  } while (isalnum((unsigned char)*s));
  s += strspn(s, SPACECHARS);  /* skip trailing spaces */
  *pn = (sol_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int solB_tonumber (sol_State *L) {
  if (sol_isnoneornil(L, 2)) {  /* standard conversion? */
    if (sol_type(L, 1) == SOL_TNUMBER) {  /* already a number? */
      sol_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = sol_tolstring(L, 1, &l);
      if (s != NULL && sol_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      solL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    sol_Integer n = 0;  /* to avoid warnings */
    sol_Integer base = solL_checkinteger(L, 2);
    solL_checktype(L, 1, SOL_TSTRING);  /* no numbers as strings */
    s = sol_tolstring(L, 1, &l);
    solL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      sol_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  solL_pushfail(L);  /* not a number */
  return 1;
}


static int solB_error (sol_State *L) {
  int level = (int)solL_optinteger(L, 2, 1);
  sol_settop(L, 1);
  if (sol_type(L, 1) == SOL_TSTRING && level > 0) {
    solL_where(L, level);   /* add extra information */
    sol_pushvalue(L, 1);
    sol_concat(L, 2);
  }
  return sol_error(L);
}


static int solB_getmetatable (sol_State *L) {
  solL_checkany(L, 1);
  if (!sol_getmetatable(L, 1)) {
    sol_pushnil(L);
    return 1;  /* no metatable */
  }
  solL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int solB_setmetatable (sol_State *L) {
  int t = sol_type(L, 2);
  solL_checktype(L, 1, SOL_TTABLE);
  solL_argexpected(L, t == SOL_TNIL || t == SOL_TTABLE, 2, "nil or table");
  if (l_unlikely(solL_getmetafield(L, 1, "__metatable") != SOL_TNIL))
    return solL_error(L, "cannot change a protected metatable");
  sol_settop(L, 2);
  sol_setmetatable(L, 1);
  return 1;
}


static int solB_rawequal (sol_State *L) {
  solL_checkany(L, 1);
  solL_checkany(L, 2);
  sol_pushboolean(L, sol_rawequal(L, 1, 2));
  return 1;
}


static int solB_rawlen (sol_State *L) {
  int t = sol_type(L, 1);
  solL_argexpected(L, t == SOL_TTABLE || t == SOL_TSTRING, 1,
                      "table or string");
  sol_pushinteger(L, sol_rawlen(L, 1));
  return 1;
}


static int solB_rawget (sol_State *L) {
  solL_checktype(L, 1, SOL_TTABLE);
  solL_checkany(L, 2);
  sol_settop(L, 2);
  sol_rawget(L, 1);
  return 1;
}

static int solB_rawset (sol_State *L) {
  solL_checktype(L, 1, SOL_TTABLE);
  solL_checkany(L, 2);
  solL_checkany(L, 3);
  sol_settop(L, 3);
  sol_rawset(L, 1);
  return 1;
}


static int pushmode (sol_State *L, int oldmode) {
  if (oldmode == -1)
    solL_pushfail(L);  /* invalid call to 'sol_gc' */
  else
    sol_pushstring(L, (oldmode == SOL_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'sol_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int solB_collectgarbage (sol_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", "generational", "incremental", NULL};
  static const int optsnum[] = {SOL_GCSTOP, SOL_GCRESTART, SOL_GCCOLLECT,
    SOL_GCCOUNT, SOL_GCSTEP, SOL_GCSETPAUSE, SOL_GCSETSTEPMUL,
    SOL_GCISRUNNING, SOL_GCGEN, SOL_GCINC};
  int o = optsnum[solL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case SOL_GCCOUNT: {
      int k = sol_gc(L, o);
      int b = sol_gc(L, SOL_GCCOUNTB);
      checkvalres(k);
      sol_pushnumber(L, (sol_Number)k + ((sol_Number)b/1024));
      return 1;
    }
    case SOL_GCSTEP: {
      int step = (int)solL_optinteger(L, 2, 0);
      int res = sol_gc(L, o, step);
      checkvalres(res);
      sol_pushboolean(L, res);
      return 1;
    }
    case SOL_GCSETPAUSE:
    case SOL_GCSETSTEPMUL: {
      int p = (int)solL_optinteger(L, 2, 0);
      int previous = sol_gc(L, o, p);
      checkvalres(previous);
      sol_pushinteger(L, previous);
      return 1;
    }
    case SOL_GCISRUNNING: {
      int res = sol_gc(L, o);
      checkvalres(res);
      sol_pushboolean(L, res);
      return 1;
    }
    case SOL_GCGEN: {
      int minormul = (int)solL_optinteger(L, 2, 0);
      int majormul = (int)solL_optinteger(L, 3, 0);
      return pushmode(L, sol_gc(L, o, minormul, majormul));
    }
    case SOL_GCINC: {
      int pause = (int)solL_optinteger(L, 2, 0);
      int stepmul = (int)solL_optinteger(L, 3, 0);
      int stepsize = (int)solL_optinteger(L, 4, 0);
      return pushmode(L, sol_gc(L, o, pause, stepmul, stepsize));
    }
    default: {
      int res = sol_gc(L, o);
      checkvalres(res);
      sol_pushinteger(L, res);
      return 1;
    }
  }
  solL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int solB_type (sol_State *L) {
  int t = sol_type(L, 1);
  solL_argcheck(L, t != SOL_TNONE, 1, "value expected");
  sol_pushstring(L, sol_typename(L, t));
  return 1;
}


static int solB_next (sol_State *L) {
  solL_checktype(L, 1, SOL_TTABLE);
  sol_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (sol_next(L, 1))
    return 2;
  else {
    sol_pushnil(L);
    return 1;
  }
}


static int pairscont (sol_State *L, int status, sol_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int solB_pairs (sol_State *L) {
  solL_checkany(L, 1);
  if (solL_getmetafield(L, 1, "__pairs") == SOL_TNIL) {  /* no metamethod? */
    sol_pushcfunction(L, solB_next);  /* will return generator, */
    sol_pushvalue(L, 1);  /* state, */
    sol_pushnil(L);  /* and initial value */
  }
  else {
    sol_pushvalue(L, 1);  /* argument 'self' to metamethod */
    sol_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (sol_State *L) {
  sol_Integer i = solL_checkinteger(L, 2);
  i = solL_intop(+, i, 1);
  sol_pushinteger(L, i);
  return (sol_geti(L, 1, i) == SOL_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int solB_ipairs (sol_State *L) {
  solL_checkany(L, 1);
  sol_pushcfunction(L, ipairsaux);  /* iteration function */
  sol_pushvalue(L, 1);  /* state */
  sol_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (sol_State *L, int status, int envidx) {
  if (l_likely(status == SOL_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      sol_pushvalue(L, envidx);  /* environment for loaded function */
      if (!sol_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        sol_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    solL_pushfail(L);
    sol_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static int solB_loadfile (sol_State *L) {
  const char *fname = solL_optstring(L, 1, NULL);
  const char *mode = solL_optstring(L, 2, NULL);
  int env = (!sol_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = solL_loadfilex(L, fname, mode);
  return load_aux(L, status, env);
}


/*
** {======================================================
** Generic Read function
** =======================================================
*/


/*
** reserved slot, above all arguments, to hold a copy of the returned
** string to avoid it being collected while parsed. 'load' has four
** optional arguments (chunk, source name, mode, and environment).
*/
#define RESERVEDSLOT	5


/*
** Reader for generic 'load' function: 'sol_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (sol_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  solL_checkstack(L, 2, "too many nested functions");
  sol_pushvalue(L, 1);  /* get function */
  sol_call(L, 0, 1);  /* call it */
  if (sol_isnil(L, -1)) {
    sol_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!sol_isstring(L, -1)))
    solL_error(L, "reader function must return a string");
  sol_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return sol_tolstring(L, RESERVEDSLOT, size);
}


static int solB_load (sol_State *L) {
  int status;
  size_t l;
  const char *s = sol_tolstring(L, 1, &l);
  const char *mode = solL_optstring(L, 3, "bt");
  int env = (!sol_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = solL_optstring(L, 2, s);
    status = solL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = solL_optstring(L, 2, "=(load)");
    solL_checktype(L, 1, SOL_TFUNCTION);
    sol_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = sol_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (sol_State *L, int d1, sol_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'sol_Kfunction' prototype */
  return sol_gettop(L) - 1;
}


static int solB_dofile (sol_State *L) {
  const char *fname = solL_optstring(L, 1, NULL);
  sol_settop(L, 1);
  if (l_unlikely(solL_loadfile(L, fname) != SOL_OK))
    return sol_error(L);
  sol_callk(L, 0, SOL_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int solB_assert (sol_State *L) {
  if (l_likely(sol_toboolean(L, 1)))  /* condition is true? */
    return sol_gettop(L);  /* return all arguments */
  else {  /* error */
    solL_checkany(L, 1);  /* there must be a condition */
    sol_remove(L, 1);  /* remove it */
    sol_pushliteral(L, "assertion failed!");  /* default message */
    sol_settop(L, 1);  /* leave only message (default if no other one) */
    return solB_error(L);  /* call 'error' */
  }
}


static int solB_select (sol_State *L) {
  int n = sol_gettop(L);
  if (sol_type(L, 1) == SOL_TSTRING && *sol_tostring(L, 1) == '#') {
    sol_pushinteger(L, n-1);
    return 1;
  }
  else {
    sol_Integer i = solL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    solL_argcheck(L, 1 <= i, 1, "index out of range");
    return n - (int)i;
  }
}


/*
** Continuation function for 'pcall' and 'xpcall'. Both functions
** already pushed a 'true' before doing the call, so in case of success
** 'finishpcall' only has to return everything in the stack minus
** 'extra' values (where 'extra' is exactly the number of items to be
** ignored).
*/
static int finishpcall (sol_State *L, int status, sol_KContext extra) {
  if (l_unlikely(status != SOL_OK && status != SOL_YIELD)) {  /* error? */
    sol_pushboolean(L, 0);  /* first result (false) */
    sol_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return sol_gettop(L) - (int)extra;  /* return all results */
}


static int solB_pcall (sol_State *L) {
  int status;
  solL_checkany(L, 1);
  sol_pushboolean(L, 1);  /* first result if no errors */
  sol_insert(L, 1);  /* put it in place */
  status = sol_pcallk(L, sol_gettop(L) - 2, SOL_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'sol_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int solB_xpcall (sol_State *L) {
  int status;
  int n = sol_gettop(L);
  solL_checktype(L, 2, SOL_TFUNCTION);  /* check error function */
  sol_pushboolean(L, 1);  /* first result */
  sol_pushvalue(L, 1);  /* function */
  sol_rotate(L, 3, 2);  /* move them below function's arguments */
  status = sol_pcallk(L, n - 2, SOL_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int solB_tostring (sol_State *L) {
  solL_checkany(L, 1);
  solL_tolstring(L, 1, NULL);
  return 1;
}


static const solL_Reg base_funcs[] = {
  {"assert", solB_assert},
  {"collectgarbage", solB_collectgarbage},
  {"dofile", solB_dofile},
  {"error", solB_error},
  {"getmetatable", solB_getmetatable},
  {"ipairs", solB_ipairs},
  {"loadfile", solB_loadfile},
  {"load", solB_load},
  {"next", solB_next},
  {"pairs", solB_pairs},
  {"pcall", solB_pcall},
  {"print", solB_print},
  {"warn", solB_warn},
  {"rawequal", solB_rawequal},
  {"rawlen", solB_rawlen},
  {"rawget", solB_rawget},
  {"rawset", solB_rawset},
  {"select", solB_select},
  {"setmetatable", solB_setmetatable},
  {"tonumber", solB_tonumber},
  {"tostring", solB_tostring},
  {"type", solB_type},
  {"xpcall", solB_xpcall},
  /* placeholders */
  {SOL_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


SOLMOD_API int solopen_base (sol_State *L) {
  /* open lib into global table */
  sol_pushglobaltable(L);
  solL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  sol_pushvalue(L, -1);
  sol_setfield(L, -2, SOL_GNAME);
  /* set global _VERSION */
  sol_pushliteral(L, SOL_VERSION);
  sol_setfield(L, -2, "_VERSION");
  return 1;
}

