/*
** $Id: lauxlib.c $
** Auxiliary functions for building Sol libraries
** See Copyright Notice in sol.h
*/

#define lauxlib_c
#define SOL_LIB

#include "lprefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Sol.
** Any function declared here could be written as an application function.
*/

#include "sol.h"

#include "lauxlib.h"


#if !defined(MAX_SIZET)
/* maximum value for size_t */
#define MAX_SIZET	((size_t)(~(size_t)0))
#endif


/*
** {======================================================
** Traceback
** =======================================================
*/


#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */



/*
** Search for 'objidx' in table at index -1. ('objidx' must be an
** absolute index.) Return 1 + string at top if it found a good name.
*/
static int findfield (sol_State *L, int objidx, int level) {
  if (level == 0 || !sol_istable(L, -1))
    return 0;  /* not found */
  sol_pushnil(L);  /* start 'next' loop */
  while (sol_next(L, -2)) {  /* for each pair in table */
    if (sol_type(L, -2) == SOL_TSTRING) {  /* ignore non-string keys */
      if (sol_rawequal(L, objidx, -1)) {  /* found object? */
        sol_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        sol_pushliteral(L, ".");  /* place '.' between the two names */
        sol_replace(L, -3);  /* (in the slot occupied by table) */
        sol_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    sol_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (sol_State *L, sol_Debug *ar) {
  int top = sol_gettop(L);
  sol_getinfo(L, "f", ar);  /* push function */
  sol_getfield(L, SOL_REGISTRYINDEX, SOL_LOADED_TABLE);
  solL_checkstack(L, 6, "not enough stack");  /* slots for 'findfield' */
  if (findfield(L, top + 1, 2)) {
    const char *name = sol_tostring(L, -1);
    if (strncmp(name, SOL_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      sol_pushstring(L, name + 3);  /* push name without prefix */
      sol_remove(L, -2);  /* remove original name */
    }
    sol_copy(L, -1, top + 1);  /* copy name to proper place */
    sol_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    sol_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (sol_State *L, sol_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    sol_pushfstring(L, "function '%s'", sol_tostring(L, -1));
    sol_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    sol_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      sol_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Sol functions, use <file:line> */
    sol_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    sol_pushliteral(L, "?");
}


static int lastlevel (sol_State *L) {
  sol_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (sol_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (sol_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


SOLLIB_API void solL_traceback (sol_State *L, sol_State *L1,
                                const char *msg, int level) {
  solL_Buffer b;
  sol_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  solL_buffinit(L, &b);
  if (msg) {
    solL_addstring(&b, msg);
    solL_addchar(&b, '\n');
  }
  solL_addstring(&b, "stack traceback:");
  while (sol_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      sol_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      solL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      sol_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        sol_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        sol_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      solL_addvalue(&b);
      pushfuncname(L, &ar);
      solL_addvalue(&b);
      if (ar.istailcall)
        solL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  solL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

SOLLIB_API int solL_argerror (sol_State *L, int arg, const char *extramsg) {
  sol_Debug ar;
  if (!sol_getstack(L, 0, &ar))  /* no stack frame? */
    return solL_error(L, "bad argument #%d (%s)", arg, extramsg);
  sol_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    arg--;  /* do not count 'self' */
    if (arg == 0)  /* error is in the self argument itself? */
      return solL_error(L, "calling '%s' on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? sol_tostring(L, -1) : "?";
  return solL_error(L, "bad argument #%d to '%s' (%s)",
                        arg, ar.name, extramsg);
}


SOLLIB_API int solL_typeerror (sol_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (solL_getmetafield(L, arg, "__name") == SOL_TSTRING)
    typearg = sol_tostring(L, -1);  /* use the given type name */
  else if (sol_type(L, arg) == SOL_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = solL_typename(L, arg);  /* standard name */
  msg = sol_pushfstring(L, "%s expected, got %s", tname, typearg);
  return solL_argerror(L, arg, msg);
}


static void tag_error (sol_State *L, int arg, int tag) {
  solL_typeerror(L, arg, sol_typename(L, tag));
}


/*
** The use of 'sol_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
SOLLIB_API void solL_where (sol_State *L, int level) {
  sol_Debug ar;
  if (sol_getstack(L, level, &ar)) {  /* check function at level */
    sol_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      sol_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  sol_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'sol_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
SOLLIB_API int solL_error (sol_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  solL_where(L, 1);
  sol_pushvfstring(L, fmt, argp);
  va_end(argp);
  sol_concat(L, 2);
  return sol_error(L);
}


SOLLIB_API int solL_fileresult (sol_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Sol API may change this value */
  if (stat) {
    sol_pushboolean(L, 1);
    return 1;
  }
  else {
    const char *msg;
    solL_pushfail(L);
    msg = (en != 0) ? strerror(en) : "(no extra info)";
    if (fname)
      sol_pushfstring(L, "%s: %s", fname, msg);
    else
      sol_pushstring(L, msg);
    sol_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(SOL_USE_POSIX)

#include <sys/wait.h>

/*
** use appropriate macros to interpret 'pclose' return status
*/
#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#else

#define l_inspectstat(stat,what)  /* no op */

#endif

#endif				/* } */


SOLLIB_API int solL_execresult (sol_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return solL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      sol_pushboolean(L, 1);
    else
      solL_pushfail(L);
    sol_pushstring(L, what);
    sol_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

SOLLIB_API int solL_newmetatable (sol_State *L, const char *tname) {
  if (solL_getmetatable(L, tname) != SOL_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  sol_pop(L, 1);
  sol_createtable(L, 0, 2);  /* create metatable */
  sol_pushstring(L, tname);
  sol_setfield(L, -2, "__name");  /* metatable.__name = tname */
  sol_pushvalue(L, -1);
  sol_setfield(L, SOL_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


SOLLIB_API void solL_setmetatable (sol_State *L, const char *tname) {
  solL_getmetatable(L, tname);
  sol_setmetatable(L, -2);
}


SOLLIB_API void *solL_testudata (sol_State *L, int ud, const char *tname) {
  void *p = sol_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (sol_getmetatable(L, ud)) {  /* does it have a metatable? */
      solL_getmetatable(L, tname);  /* get correct metatable */
      if (!sol_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      sol_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


SOLLIB_API void *solL_checkudata (sol_State *L, int ud, const char *tname) {
  void *p = solL_testudata(L, ud, tname);
  solL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

SOLLIB_API int solL_checkoption (sol_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? solL_optstring(L, arg, def) :
                             solL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return solL_argerror(L, arg,
                       sol_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Sol will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
SOLLIB_API void solL_checkstack (sol_State *L, int space, const char *msg) {
  if (l_unlikely(!sol_checkstack(L, space))) {
    if (msg)
      solL_error(L, "stack overflow (%s)", msg);
    else
      solL_error(L, "stack overflow");
  }
}


SOLLIB_API void solL_checktype (sol_State *L, int arg, int t) {
  if (l_unlikely(sol_type(L, arg) != t))
    tag_error(L, arg, t);
}


SOLLIB_API void solL_checkany (sol_State *L, int arg) {
  if (l_unlikely(sol_type(L, arg) == SOL_TNONE))
    solL_argerror(L, arg, "value expected");
}


SOLLIB_API const char *solL_checklstring (sol_State *L, int arg, size_t *len) {
  const char *s = sol_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, SOL_TSTRING);
  return s;
}


SOLLIB_API const char *solL_optlstring (sol_State *L, int arg,
                                        const char *def, size_t *len) {
  if (sol_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return solL_checklstring(L, arg, len);
}


SOLLIB_API sol_Number solL_checknumber (sol_State *L, int arg) {
  int isnum;
  sol_Number d = sol_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, SOL_TNUMBER);
  return d;
}


SOLLIB_API sol_Number solL_optnumber (sol_State *L, int arg, sol_Number def) {
  return solL_opt(L, solL_checknumber, arg, def);
}


static void interror (sol_State *L, int arg) {
  if (sol_isnumber(L, arg))
    solL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, SOL_TNUMBER);
}


SOLLIB_API sol_Integer solL_checkinteger (sol_State *L, int arg) {
  int isnum;
  sol_Integer d = sol_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


SOLLIB_API sol_Integer solL_optinteger (sol_State *L, int arg,
                                                      sol_Integer def) {
  return solL_opt(L, solL_checkinteger, arg, def);
}

/* }====================================================== */


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/* userdata to box arbitrary data */
typedef struct UBox {
  void *box;
  size_t bsize;
} UBox;


static void *resizebox (sol_State *L, int idx, size_t newsize) {
  void *ud;
  sol_Alloc allocf = sol_getallocf(L, &ud);
  UBox *box = (UBox *)sol_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
    sol_pushliteral(L, "not enough memory");
    sol_error(L);  /* raise a memory error */
  }
  box->box = temp;
  box->bsize = newsize;
  return temp;
}


static int boxgc (sol_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const solL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (sol_State *L) {
  UBox *box = (UBox *)sol_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (solL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    solL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  sol_setmetatable(L, -2);
}


/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->init.b)


/*
** Whenever buffer is accessed, slot 'idx' must either be a box (which
** cannot be NULL) or it is a placeholder for the buffer.
*/
#define checkbufferlevel(B,idx)  \
  sol_assert(buffonstack(B) ? sol_touserdata(B->L, idx) != NULL  \
                            : sol_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes. (The test for "not big enough" also gets the case when the
** computation of 'newsize' overflows.)
*/
static size_t newbuffsize (solL_Buffer *B, size_t sz) {
  size_t newsize = (B->size / 2) * 3;  /* buffer size * 1.5 */
  if (l_unlikely(MAX_SIZET - sz < B->n))  /* overflow in (B->n + sz)? */
    return solL_error(B->L, "buffer too large");
  if (newsize < B->n + sz)  /* not big enough? */
    newsize = B->n + sz;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (solL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    sol_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      sol_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      sol_insert(L, boxidx);  /* move box to its intended position */
      sol_toclose(L, boxidx);
      newbuff = (char *)resizebox(L, boxidx, newsize);
      memcpy(newbuff, B->b, B->n * sizeof(char));  /* copy original content */
    }
    B->b = newbuff;
    B->size = newsize;
    return newbuff + B->n;
  }
}

/*
** returns a pointer to a free area with at least 'sz' bytes
*/
SOLLIB_API char *solL_prepbuffsize (solL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


SOLLIB_API void solL_addlstring (solL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    solL_addsize(B, l);
  }
}


SOLLIB_API void solL_addstring (solL_Buffer *B, const char *s) {
  solL_addlstring(B, s, strlen(s));
}


SOLLIB_API void solL_pushresult (solL_Buffer *B) {
  sol_State *L = B->L;
  checkbufferlevel(B, -1);
  sol_pushlstring(L, B->b, B->n);
  if (buffonstack(B))
    sol_closeslot(L, -2);  /* close the box */
  sol_remove(L, -2);  /* remove box or placeholder from the stack */
}


SOLLIB_API void solL_pushresultsize (solL_Buffer *B, size_t sz) {
  solL_addsize(B, sz);
  solL_pushresult(B);
}


/*
** 'solL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'solL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) below the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
SOLLIB_API void solL_addvalue (solL_Buffer *B) {
  sol_State *L = B->L;
  size_t len;
  const char *s = sol_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  solL_addsize(B, len);
  sol_pop(L, 1);  /* pop string */
}


SOLLIB_API void solL_buffinit (sol_State *L, solL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = SOLL_BUFFERSIZE;
  sol_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


SOLLIB_API char *solL_buffinitsize (sol_State *L, solL_Buffer *B, size_t sz) {
  solL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header (after the predefined values) */
#define freelist	(SOL_RIDX_LAST + 1)

/*
** The previously freed references form a linked list:
** t[freelist] is the index of a first free index, or zero if list is
** empty; t[t[freelist]] is the index of the second element; etc.
*/
SOLLIB_API int solL_ref (sol_State *L, int t) {
  int ref;
  if (sol_isnil(L, -1)) {
    sol_pop(L, 1);  /* remove from stack */
    return SOL_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = sol_absindex(L, t);
  if (sol_rawgeti(L, t, freelist) == SOL_TNIL) {  /* first access? */
    ref = 0;  /* list is empty */
    sol_pushinteger(L, 0);  /* initialize as an empty list */
    sol_rawseti(L, t, freelist);  /* ref = t[freelist] = 0 */
  }
  else {  /* already initialized */
    sol_assert(sol_isinteger(L, -1));
    ref = (int)sol_tointeger(L, -1);  /* ref = t[freelist] */
  }
  sol_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    sol_rawgeti(L, t, ref);  /* remove it from list */
    sol_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)sol_rawlen(L, t) + 1;  /* get a new reference */
  sol_rawseti(L, t, ref);
  return ref;
}


SOLLIB_API void solL_unref (sol_State *L, int t, int ref) {
  if (ref >= 0) {
    t = sol_absindex(L, t);
    sol_rawgeti(L, t, freelist);
    sol_assert(sol_isinteger(L, -1));
    sol_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
    sol_pushinteger(L, ref);
    sol_rawseti(L, t, freelist);  /* t[freelist] = ref */
  }
}

/* }====================================================== */


/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
  int n;  /* number of pre-read characters */
  FILE *f;  /* file being read */
  char buff[BUFSIZ];  /* area for reading file */
} LoadF;


static const char *getF (sol_State *L, void *ud, size_t *size) {
  LoadF *lf = (LoadF *)ud;
  (void)L;  /* not used */
  if (lf->n > 0) {  /* are there pre-read characters to be read? */
    *size = lf->n;  /* return them (chars already in buffer) */
    lf->n = 0;  /* no more pre-read characters */
  }
  else {  /* read a block from file */
    /* 'fread' can return > 0 *and* set the EOF flag. If next call to
       'getF' called 'fread', it might still wait for user input.
       The next check avoids this problem. */
    if (feof(lf->f)) return NULL;
    *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);  /* read block */
  }
  return lf->buff;
}


static int errfile (sol_State *L, const char *what, int fnameindex) {
  int err = errno;
  const char *filename = sol_tostring(L, fnameindex) + 1;
  if (err != 0)
    sol_pushfstring(L, "cannot %s %s: %s", what, filename, strerror(err));
  else
    sol_pushfstring(L, "cannot %s %s", what, filename);
  sol_remove(L, fnameindex);
  return SOL_ERRFILE;
}


/*
** Skip an optional BOM at the start of a stream. If there is an
** incomplete BOM (the first character is correct but the rest is
** not), returns the first character anyway to force an error
** (as no chunk can start with 0xEF).
*/
static int skipBOM (FILE *f) {
  int c = getc(f);  /* read first character */
  if (c == 0xEF && getc(f) == 0xBB && getc(f) == 0xBF)  /* correct BOM? */
    return getc(f);  /* ignore BOM and return next char */
  else  /* no (valid) BOM */
    return c;  /* return first character */
}


/*
** reads the first character of file 'f' and skips an optional BOM mark
** in its beginning plus its first line if it starts with '#'. Returns
** true if it skipped the first line.  In any case, '*cp' has the
** first "valid" character of the file (after the optional BOM and
** a first-line comment).
*/
static int skipcomment (FILE *f, int *cp) {
  int c = *cp = skipBOM(f);
  if (c == '#') {  /* first line is a comment (Unix exec. file)? */
    do {  /* skip first line */
      c = getc(f);
    } while (c != EOF && c != '\n');
    *cp = getc(f);  /* next character after comment, if present */
    return 1;  /* there was a comment */
  }
  else return 0;  /* no comment */
}


SOLLIB_API int solL_loadfilex (sol_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = sol_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    sol_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    sol_pushfstring(L, "@%s", filename);
    errno = 0;
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  lf.n = 0;
  if (skipcomment(lf.f, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add newline to correct line numbers */
  if (c == SOL_SIGNATURE[0]) {  /* binary file? */
    lf.n = 0;  /* remove possible newline */
    if (filename) {  /* "real" file? */
      errno = 0;
      lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
      if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
      skipcomment(lf.f, &c);  /* re-read initial portion */
    }
  }
  if (c != EOF)
    lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
  errno = 0;
  status = sol_load(L, getF, &lf, sol_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    sol_settop(L, fnameindex);  /* ignore results from 'sol_load' */
    return errfile(L, "read", fnameindex);
  }
  sol_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (sol_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


SOLLIB_API int solL_loadbufferx (sol_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return sol_load(L, getS, &ls, name, mode);
}


SOLLIB_API int solL_loadstring (sol_State *L, const char *s) {
  return solL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



SOLLIB_API int solL_getmetafield (sol_State *L, int obj, const char *event) {
  if (!sol_getmetatable(L, obj))  /* no metatable? */
    return SOL_TNIL;
  else {
    int tt;
    sol_pushstring(L, event);
    tt = sol_rawget(L, -2);
    if (tt == SOL_TNIL)  /* is metafield nil? */
      sol_pop(L, 2);  /* remove metatable and metafield */
    else
      sol_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


SOLLIB_API int solL_callmeta (sol_State *L, int obj, const char *event) {
  obj = sol_absindex(L, obj);
  if (solL_getmetafield(L, obj, event) == SOL_TNIL)  /* no metafield? */
    return 0;
  sol_pushvalue(L, obj);
  sol_call(L, 1, 1);
  return 1;
}


SOLLIB_API sol_Integer solL_len (sol_State *L, int idx) {
  sol_Integer l;
  int isnum;
  sol_len(L, idx);
  l = sol_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    solL_error(L, "object length is not an integer");
  sol_pop(L, 1);  /* remove object */
  return l;
}


SOLLIB_API const char *solL_tolstring (sol_State *L, int idx, size_t *len) {
  idx = sol_absindex(L,idx);
  if (solL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!sol_isstring(L, -1))
      solL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (sol_type(L, idx)) {
      case SOL_TNUMBER: {
        if (sol_isinteger(L, idx))
          sol_pushfstring(L, "%I", (SOLI_UACINT)sol_tointeger(L, idx));
        else
          sol_pushfstring(L, "%f", (SOLI_UACNUMBER)sol_tonumber(L, idx));
        break;
      }
      case SOL_TSTRING:
        sol_pushvalue(L, idx);
        break;
      case SOL_TBOOLEAN:
        sol_pushstring(L, (sol_toboolean(L, idx) ? "true" : "false"));
        break;
      case SOL_TNIL:
        sol_pushliteral(L, "nil");
        break;
      default: {
        int tt = solL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == SOL_TSTRING) ? sol_tostring(L, -1) :
                                                 solL_typename(L, idx);
        sol_pushfstring(L, "%s: %p", kind, sol_topointer(L, idx));
        if (tt != SOL_TNIL)
          sol_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return sol_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
SOLLIB_API void solL_setfuncs (sol_State *L, const solL_Reg *l, int nup) {
  solL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* placeholder? */
      sol_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        sol_pushvalue(L, -nup);
      sol_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    sol_setfield(L, -(nup + 2), l->name);
  }
  sol_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
SOLLIB_API int solL_getsubtable (sol_State *L, int idx, const char *fname) {
  if (sol_getfield(L, idx, fname) == SOL_TTABLE)
    return 1;  /* table already there */
  else {
    sol_pop(L, 1);  /* remove previous result */
    idx = sol_absindex(L, idx);
    sol_newtable(L);
    sol_pushvalue(L, -1);  /* copy to be left at top */
    sol_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
SOLLIB_API void solL_requiref (sol_State *L, const char *modname,
                               sol_CFunction openf, int glb) {
  solL_getsubtable(L, SOL_REGISTRYINDEX, SOL_LOADED_TABLE);
  sol_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!sol_toboolean(L, -1)) {  /* package not already loaded? */
    sol_pop(L, 1);  /* remove field */
    sol_pushcfunction(L, openf);
    sol_pushstring(L, modname);  /* argument to open function */
    sol_call(L, 1, 1);  /* call 'openf' to open module */
    sol_pushvalue(L, -1);  /* make copy of module (call result) */
    sol_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  sol_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    sol_pushvalue(L, -1);  /* copy of module */
    sol_setglobal(L, modname);  /* _G[modname] = module */
  }
}


SOLLIB_API void solL_addgsub (solL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    solL_addlstring(b, s, wild - s);  /* push prefix */
    solL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  solL_addstring(b, s);  /* push last suffix */
}


SOLLIB_API const char *solL_gsub (sol_State *L, const char *s,
                                  const char *p, const char *r) {
  solL_Buffer b;
  solL_buffinit(L, &b);
  solL_addgsub(&b, s, p, r);
  solL_pushresult(&b);
  return sol_tostring(L, -1);
}


static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}


/*
** Standard panic funcion just prints an error message. The test
** with 'sol_type' avoids possible memory errors in 'sol_tostring'.
*/
static int panic (sol_State *L) {
  const char *msg = (sol_type(L, -1) == SOL_TSTRING)
                  ? sol_tostring(L, -1)
                  : "error object is not a string";
  sol_writestringerror("PANIC: unprotected error in call to Sol API (%s)\n",
                        msg);
  return 0;  /* return to Sol to abort */
}


/*
** Warning functions:
** warnfoff: warning system is off
** warnfon: ready to start a new message
** warnfcont: previous message is to be continued
*/
static void warnfoff (void *ud, const char *message, int tocont);
static void warnfon (void *ud, const char *message, int tocont);
static void warnfcont (void *ud, const char *message, int tocont);


/*
** Check whether message is a control message. If so, execute the
** control or ignore it if unknown.
*/
static int checkcontrol (sol_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      sol_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      sol_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((sol_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  sol_State *L = (sol_State *)ud;
  sol_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    sol_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    sol_writestringerror("%s", "\n");  /* finish message with end-of-line */
    sol_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((sol_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  sol_writestringerror("%s", "Sol warning: ");  /* start a new warning */
  warnfcont(ud, message, tocont);  /* finish processing */
}


SOLLIB_API sol_State *solL_newstate (void) {
  sol_State *L = sol_newstate(l_alloc, NULL);
  if (l_likely(L)) {
    sol_atpanic(L, &panic);
    sol_setwarnf(L, warnfoff, L);  /* default is warnings off */
  }
  return L;
}


SOLLIB_API void solL_checkversion_ (sol_State *L, sol_Number ver, size_t sz) {
  sol_Number v = sol_version(L);
  if (sz != SOLL_NUMSIZES)  /* check numeric types */
    solL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    solL_error(L, "version mismatch: app. needs %f, Sol core provides %f",
                  (SOLI_UACNUMBER)ver, (SOLI_UACNUMBER)v);
}

