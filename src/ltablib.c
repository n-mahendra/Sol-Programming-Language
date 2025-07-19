/*
** $Id: ltablib.c $
** Library for Table Manipulation
** See Copyright Notice in sol.h
*/

#define ltablib_c
#define SOL_LIB

#include "lprefix.h"


#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "sol.h"

#include "lauxlib.h"
#include "sollib.h"


/*
** Operations that an object must define to mimic a table
** (some functions only need some of them)
*/
#define TAB_R	1			/* read */
#define TAB_W	2			/* write */
#define TAB_L	4			/* length */
#define TAB_RW	(TAB_R | TAB_W)		/* read/write */


#define aux_getn(L,n,w)	(checktab(L, n, (w) | TAB_L), solL_len(L, n))


static int checkfield (sol_State *L, const char *key, int n) {
  sol_pushstring(L, key);
  return (sol_rawget(L, -n) != SOL_TNIL);
}


/*
** Check that 'arg' either is a table or can behave like one (that is,
** has a metatable with the required metamethods)
*/
static void checktab (sol_State *L, int arg, int what) {
  if (sol_type(L, arg) != SOL_TTABLE) {  /* is it not a table? */
    int n = 1;  /* number of elements to pop */
    if (sol_getmetatable(L, arg) &&  /* must have metatable */
        (!(what & TAB_R) || checkfield(L, "__index", ++n)) &&
        (!(what & TAB_W) || checkfield(L, "__newindex", ++n)) &&
        (!(what & TAB_L) || checkfield(L, "__len", ++n))) {
      sol_pop(L, n);  /* pop metatable and tested metamethods */
    }
    else
      solL_checktype(L, arg, SOL_TTABLE);  /* force an error */
  }
}


static int tinsert (sol_State *L) {
  sol_Integer pos;  /* where to insert new element */
  sol_Integer e = aux_getn(L, 1, TAB_RW);
  e = solL_intop(+, e, 1);  /* first empty element */
  switch (sol_gettop(L)) {
    case 2: {  /* called with only 2 arguments */
      pos = e;  /* insert new element at the end */
      break;
    }
    case 3: {
      sol_Integer i;
      pos = solL_checkinteger(L, 2);  /* 2nd argument is the position */
      /* check whether 'pos' is in [1, e] */
      solL_argcheck(L, (sol_Unsigned)pos - 1u < (sol_Unsigned)e, 2,
                       "position out of bounds");
      for (i = e; i > pos; i--) {  /* move up elements */
        sol_geti(L, 1, i - 1);
        sol_seti(L, 1, i);  /* t[i] = t[i - 1] */
      }
      break;
    }
    default: {
      return solL_error(L, "wrong number of arguments to 'insert'");
    }
  }
  sol_seti(L, 1, pos);  /* t[pos] = v */
  return 0;
}


static int tremove (sol_State *L) {
  sol_Integer size = aux_getn(L, 1, TAB_RW);
  sol_Integer pos = solL_optinteger(L, 2, size);
  if (pos != size)  /* validate 'pos' if given */
    /* check whether 'pos' is in [1, size + 1] */
    solL_argcheck(L, (sol_Unsigned)pos - 1u <= (sol_Unsigned)size, 2,
                     "position out of bounds");
  sol_geti(L, 1, pos);  /* result = t[pos] */
  for ( ; pos < size; pos++) {
    sol_geti(L, 1, pos + 1);
    sol_seti(L, 1, pos);  /* t[pos] = t[pos + 1] */
  }
  sol_pushnil(L);
  sol_seti(L, 1, pos);  /* remove entry t[pos] */
  return 1;
}


/*
** Copy elements (1[f], ..., 1[e]) into (tt[t], tt[t+1], ...). Whenever
** possible, copy in increasing order, which is better for rehashing.
** "possible" means destination after original range, or smaller
** than origin, or copying to another table.
*/
static int tmove (sol_State *L) {
  sol_Integer f = solL_checkinteger(L, 2);
  sol_Integer e = solL_checkinteger(L, 3);
  sol_Integer t = solL_checkinteger(L, 4);
  int tt = !sol_isnoneornil(L, 5) ? 5 : 1;  /* destination table */
  checktab(L, 1, TAB_R);
  checktab(L, tt, TAB_W);
  if (e >= f) {  /* otherwise, nothing to move */
    sol_Integer n, i;
    solL_argcheck(L, f > 0 || e < SOL_MAXINTEGER + f, 3,
                  "too many elements to move");
    n = e - f + 1;  /* number of elements to move */
    solL_argcheck(L, t <= SOL_MAXINTEGER - n + 1, 4,
                  "destination wrap around");
    if (t > e || t <= f || (tt != 1 && !sol_compare(L, 1, tt, SOL_OPEQ))) {
      for (i = 0; i < n; i++) {
        sol_geti(L, 1, f + i);
        sol_seti(L, tt, t + i);
      }
    }
    else {
      for (i = n - 1; i >= 0; i--) {
        sol_geti(L, 1, f + i);
        sol_seti(L, tt, t + i);
      }
    }
  }
  sol_pushvalue(L, tt);  /* return destination table */
  return 1;
}


static void addfield (sol_State *L, solL_Buffer *b, sol_Integer i) {
  sol_geti(L, 1, i);
  if (l_unlikely(!sol_isstring(L, -1)))
    solL_error(L, "invalid value (%s) at index %I in table for 'concat'",
                  solL_typename(L, -1), (SOLI_UACINT)i);
  solL_addvalue(b);
}


static int tconcat (sol_State *L) {
  solL_Buffer b;
  sol_Integer last = aux_getn(L, 1, TAB_R);
  size_t lsep;
  const char *sep = solL_optlstring(L, 2, "", &lsep);
  sol_Integer i = solL_optinteger(L, 3, 1);
  last = solL_optinteger(L, 4, last);
  solL_buffinit(L, &b);
  for (; i < last; i++) {
    addfield(L, &b, i);
    solL_addlstring(&b, sep, lsep);
  }
  if (i == last)  /* add last value (if interval was not empty) */
    addfield(L, &b, i);
  solL_pushresult(&b);
  return 1;
}


/*
** {======================================================
** Pack/unpack
** =======================================================
*/

static int tpack (sol_State *L) {
  int i;
  int n = sol_gettop(L);  /* number of elements to pack */
  sol_createtable(L, n, 1);  /* create result table */
  sol_insert(L, 1);  /* put it at index 1 */
  for (i = n; i >= 1; i--)  /* assign elements */
    sol_seti(L, 1, i);
  sol_pushinteger(L, n);
  sol_setfield(L, 1, "n");  /* t.n = number of elements */
  return 1;  /* return table */
}


static int tunpack (sol_State *L) {
  sol_Unsigned n;
  sol_Integer i = solL_optinteger(L, 2, 1);
  sol_Integer e = solL_opt(L, solL_checkinteger, 3, solL_len(L, 1));
  if (i > e) return 0;  /* empty range */
  n = (sol_Unsigned)e - i;  /* number of elements minus 1 (avoid overflows) */
  if (l_unlikely(n >= (unsigned int)INT_MAX  ||
                 !sol_checkstack(L, (int)(++n))))
    return solL_error(L, "too many results to unpack");
  for (; i < e; i++) {  /* push arg[i..e - 1] (to avoid overflows) */
    sol_geti(L, 1, i);
  }
  sol_geti(L, 1, e);  /* push last element */
  return (int)n;
}

/* }====================================================== */



/*
** {======================================================
** Quicksort
** (based on 'Algorithms in MODULA-3', Robert Sedgewick;
**  Addison-Wesley, 1993.)
** =======================================================
*/


/* type for array indices */
typedef unsigned int IdxT;


/*
** Produce a "random" 'unsigned int' to randomize pivot choice. This
** macro is used only when 'sort' detects a big imbalance in the result
** of a partition. (If you don't want/need this "randomness", ~0 is a
** good choice.)
*/
#if !defined(l_randomizePivot)		/* { */

#include <time.h>

/* size of 'e' measured in number of 'unsigned int's */
#define sof(e)		(sizeof(e) / sizeof(unsigned int))

/*
** Use 'time' and 'clock' as sources of "randomness". Because we don't
** know the types 'clock_t' and 'time_t', we cannot cast them to
** anything without risking overflows. A safe way to use their values
** is to copy them to an array of a known type and use the array values.
*/
static unsigned int l_randomizePivot (void) {
  clock_t c = clock();
  time_t t = time(NULL);
  unsigned int buff[sof(c) + sof(t)];
  unsigned int i, rnd = 0;
  memcpy(buff, &c, sof(c) * sizeof(unsigned int));
  memcpy(buff + sof(c), &t, sof(t) * sizeof(unsigned int));
  for (i = 0; i < sof(buff); i++)
    rnd += buff[i];
  return rnd;
}

#endif					/* } */


/* arrays larger than 'RANLIMIT' may use randomized pivots */
#define RANLIMIT	100u


static void set2 (sol_State *L, IdxT i, IdxT j) {
  sol_seti(L, 1, i);
  sol_seti(L, 1, j);
}


/*
** Return true iff value at stack index 'a' is less than the value at
** index 'b' (according to the order of the sort).
*/
static int sort_comp (sol_State *L, int a, int b) {
  if (sol_isnil(L, 2))  /* no function? */
    return sol_compare(L, a, b, SOL_OPLT);  /* a < b */
  else {  /* function */
    int res;
    sol_pushvalue(L, 2);    /* push function */
    sol_pushvalue(L, a-1);  /* -1 to compensate function */
    sol_pushvalue(L, b-2);  /* -2 to compensate function and 'a' */
    sol_call(L, 2, 1);      /* call function */
    res = sol_toboolean(L, -1);  /* get result */
    sol_pop(L, 1);          /* pop result */
    return res;
  }
}


/*
** Does the partition: Pivot P is at the top of the stack.
** precondition: a[lo] <= P == a[up-1] <= a[up],
** so it only needs to do the partition from lo + 1 to up - 2.
** Pos-condition: a[lo .. i - 1] <= a[i] == P <= a[i + 1 .. up]
** returns 'i'.
*/
static IdxT partition (sol_State *L, IdxT lo, IdxT up) {
  IdxT i = lo;  /* will be incremented before first use */
  IdxT j = up - 1;  /* will be decremented before first use */
  /* loop invariant: a[lo .. i] <= P <= a[j .. up] */
  for (;;) {
    /* next loop: repeat ++i while a[i] < P */
    while ((void)sol_geti(L, 1, ++i), sort_comp(L, -1, -2)) {
      if (l_unlikely(i == up - 1))  /* a[i] < P  but a[up - 1] == P  ?? */
        solL_error(L, "invalid order function for sorting");
      sol_pop(L, 1);  /* remove a[i] */
    }
    /* after the loop, a[i] >= P and a[lo .. i - 1] < P */
    /* next loop: repeat --j while P < a[j] */
    while ((void)sol_geti(L, 1, --j), sort_comp(L, -3, -1)) {
      if (l_unlikely(j < i))  /* j < i  but  a[j] > P ?? */
        solL_error(L, "invalid order function for sorting");
      sol_pop(L, 1);  /* remove a[j] */
    }
    /* after the loop, a[j] <= P and a[j + 1 .. up] >= P */
    if (j < i) {  /* no elements out of place? */
      /* a[lo .. i - 1] <= P <= a[j + 1 .. i .. up] */
      sol_pop(L, 1);  /* pop a[j] */
      /* swap pivot (a[up - 1]) with a[i] to satisfy pos-condition */
      set2(L, up - 1, i);
      return i;
    }
    /* otherwise, swap a[i] - a[j] to restore invariant and repeat */
    set2(L, i, j);
  }
}


/*
** Choose an element in the middle (2nd-3th quarters) of [lo,up]
** "randomized" by 'rnd'
*/
static IdxT choosePivot (IdxT lo, IdxT up, unsigned int rnd) {
  IdxT r4 = (up - lo) / 4;  /* range/4 */
  IdxT p = rnd % (r4 * 2) + (lo + r4);
  sol_assert(lo + r4 <= p && p <= up - r4);
  return p;
}


/*
** Quicksort algorithm (recursive function)
*/
static void auxsort (sol_State *L, IdxT lo, IdxT up,
                                   unsigned int rnd) {
  while (lo < up) {  /* loop for tail recursion */
    IdxT p;  /* Pivot index */
    IdxT n;  /* to be used later */
    /* sort elements 'lo', 'p', and 'up' */
    sol_geti(L, 1, lo);
    sol_geti(L, 1, up);
    if (sort_comp(L, -1, -2))  /* a[up] < a[lo]? */
      set2(L, lo, up);  /* swap a[lo] - a[up] */
    else
      sol_pop(L, 2);  /* remove both values */
    if (up - lo == 1)  /* only 2 elements? */
      return;  /* already sorted */
    if (up - lo < RANLIMIT || rnd == 0)  /* small interval or no randomize? */
      p = (lo + up)/2;  /* middle element is a good pivot */
    else  /* for larger intervals, it is worth a random pivot */
      p = choosePivot(lo, up, rnd);
    sol_geti(L, 1, p);
    sol_geti(L, 1, lo);
    if (sort_comp(L, -2, -1))  /* a[p] < a[lo]? */
      set2(L, p, lo);  /* swap a[p] - a[lo] */
    else {
      sol_pop(L, 1);  /* remove a[lo] */
      sol_geti(L, 1, up);
      if (sort_comp(L, -1, -2))  /* a[up] < a[p]? */
        set2(L, p, up);  /* swap a[up] - a[p] */
      else
        sol_pop(L, 2);
    }
    if (up - lo == 2)  /* only 3 elements? */
      return;  /* already sorted */
    sol_geti(L, 1, p);  /* get middle element (Pivot) */
    sol_pushvalue(L, -1);  /* push Pivot */
    sol_geti(L, 1, up - 1);  /* push a[up - 1] */
    set2(L, p, up - 1);  /* swap Pivot (a[p]) with a[up - 1] */
    p = partition(L, lo, up);
    /* a[lo .. p - 1] <= a[p] == P <= a[p + 1 .. up] */
    if (p - lo < up - p) {  /* lower interval is smaller? */
      auxsort(L, lo, p - 1, rnd);  /* call recursively for lower interval */
      n = p - lo;  /* size of smaller interval */
      lo = p + 1;  /* tail call for [p + 1 .. up] (upper interval) */
    }
    else {
      auxsort(L, p + 1, up, rnd);  /* call recursively for upper interval */
      n = up - p;  /* size of smaller interval */
      up = p - 1;  /* tail call for [lo .. p - 1]  (lower interval) */
    }
    if ((up - lo) / 128 > n) /* partition too imbalanced? */
      rnd = l_randomizePivot();  /* try a new randomization */
  }  /* tail call auxsort(L, lo, up, rnd) */
}


static int sort (sol_State *L) {
  sol_Integer n = aux_getn(L, 1, TAB_RW);
  if (n > 1) {  /* non-trivial interval? */
    solL_argcheck(L, n < INT_MAX, 1, "array too big");
    if (!sol_isnoneornil(L, 2))  /* is there a 2nd argument? */
      solL_checktype(L, 2, SOL_TFUNCTION);  /* must be a function */
    sol_settop(L, 2);  /* make sure there are two arguments */
    auxsort(L, 1, (IdxT)n, 0);
  }
  return 0;
}

/* }====================================================== */


static const solL_Reg tab_funcs[] = {
  {"concat", tconcat},
  {"insert", tinsert},
  {"pack", tpack},
  {"unpack", tunpack},
  {"remove", tremove},
  {"move", tmove},
  {"sort", sort},
  {NULL, NULL}
};


SOLMOD_API int solopen_table (sol_State *L) {
  solL_newlib(L, tab_funcs);
  return 1;
}

