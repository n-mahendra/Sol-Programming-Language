/*
** $Id: lutf8lib.c $
** Standard library for UTF-8 manipulation
** See Copyright Notice in sol.h
*/

#define lutf8lib_c
#define SOL_LIB

#include "lprefix.h"


#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "sol.h"

#include "lauxlib.h"
#include "sollib.h"


#define MAXUNICODE	0x10FFFFu

#define MAXUTF		0x7FFFFFFFu


#define MSGInvalid	"invalid UTF-8 code"

/*
** Integer type for decoded UTF-8 values; MAXUTF needs 31 bits.
*/
#if (UINT_MAX >> 30) >= 1
typedef unsigned int utfint;
#else
typedef unsigned long utfint;
#endif


#define iscont(c)	(((c) & 0xC0) == 0x80)
#define iscontp(p)	iscont(*(p))


/* from strlib */
/* translate a relative string position: negative means back from end */
static sol_Integer u_posrelat (sol_Integer pos, size_t len) {
  if (pos >= 0) return pos;
  else if (0u - (size_t)pos > len) return 0;
  else return (sol_Integer)len + pos + 1;
}


/*
** Decode one UTF-8 sequence, returning NULL if byte sequence is
** invalid.  The array 'limits' stores the minimum value for each
** sequence length, to check for overlong representations. Its first
** entry forces an error for non-ascii bytes with no continuation
** bytes (count == 0).
*/
static const char *utf8_decode (const char *s, utfint *val, int strict) {
  static const utfint limits[] =
        {~(utfint)0, 0x80, 0x800, 0x10000u, 0x200000u, 0x4000000u};
  unsigned int c = (unsigned char)s[0];
  utfint res = 0;  /* final result */
  if (c < 0x80)  /* ascii? */
    res = c;
  else {
    int count = 0;  /* to count number of continuation bytes */
    for (; c & 0x40; c <<= 1) {  /* while it needs continuation bytes... */
      unsigned int cc = (unsigned char)s[++count];  /* read next byte */
      if (!iscont(cc))  /* not a continuation byte? */
        return NULL;  /* invalid byte sequence */
      res = (res << 6) | (cc & 0x3F);  /* add lower 6 bits from cont. byte */
    }
    res |= ((utfint)(c & 0x7F) << (count * 5));  /* add first byte */
    if (count > 5 || res > MAXUTF || res < limits[count])
      return NULL;  /* invalid byte sequence */
    s += count;  /* skip continuation bytes read */
  }
  if (strict) {
    /* check for invalid code points; too large or surrogates */
    if (res > MAXUNICODE || (0xD800u <= res && res <= 0xDFFFu))
      return NULL;
  }
  if (val) *val = res;
  return s + 1;  /* +1 to include first byte */
}


/*
** utf8len(s [, i [, j [, lax]]]) --> number of characters that
** start in the range [i,j], or nil + current position if 's' is not
** well formed in that interval
*/
static int utflen (sol_State *L) {
  sol_Integer n = 0;  /* counter for the number of characters */
  size_t len;  /* string length in bytes */
  const char *s = solL_checklstring(L, 1, &len);
  sol_Integer posi = u_posrelat(solL_optinteger(L, 2, 1), len);
  sol_Integer posj = u_posrelat(solL_optinteger(L, 3, -1), len);
  int lax = sol_toboolean(L, 4);
  solL_argcheck(L, 1 <= posi && --posi <= (sol_Integer)len, 2,
                   "initial position out of bounds");
  solL_argcheck(L, --posj < (sol_Integer)len, 3,
                   "final position out of bounds");
  while (posi <= posj) {
    const char *s1 = utf8_decode(s + posi, NULL, !lax);
    if (s1 == NULL) {  /* conversion error? */
      solL_pushfail(L);  /* return fail ... */
      sol_pushinteger(L, posi + 1);  /* ... and current position */
      return 2;
    }
    posi = s1 - s;
    n++;
  }
  sol_pushinteger(L, n);
  return 1;
}


/*
** codepoint(s, [i, [j [, lax]]]) -> returns codepoints for all
** characters that start in the range [i,j]
*/
static int codepoint (sol_State *L) {
  size_t len;
  const char *s = solL_checklstring(L, 1, &len);
  sol_Integer posi = u_posrelat(solL_optinteger(L, 2, 1), len);
  sol_Integer pose = u_posrelat(solL_optinteger(L, 3, posi), len);
  int lax = sol_toboolean(L, 4);
  int n;
  const char *se;
  solL_argcheck(L, posi >= 1, 2, "out of bounds");
  solL_argcheck(L, pose <= (sol_Integer)len, 3, "out of bounds");
  if (posi > pose) return 0;  /* empty interval; return no values */
  if (pose - posi >= INT_MAX)  /* (sol_Integer -> int) overflow? */
    return solL_error(L, "string slice too long");
  n = (int)(pose -  posi) + 1;  /* upper bound for number of returns */
  solL_checkstack(L, n, "string slice too long");
  n = 0;  /* count the number of returns */
  se = s + pose;  /* string end */
  for (s += posi - 1; s < se;) {
    utfint code;
    s = utf8_decode(s, &code, !lax);
    if (s == NULL)
      return solL_error(L, MSGInvalid);
    sol_pushinteger(L, code);
    n++;
  }
  return n;
}


static void pushutfchar (sol_State *L, int arg) {
  sol_Unsigned code = (sol_Unsigned)solL_checkinteger(L, arg);
  solL_argcheck(L, code <= MAXUTF, arg, "value out of range");
  sol_pushfstring(L, "%U", (long)code);
}


/*
** utfchar(n1, n2, ...)  -> char(n1)..char(n2)...
*/
static int utfchar (sol_State *L) {
  int n = sol_gettop(L);  /* number of arguments */
  if (n == 1)  /* optimize common case of single char */
    pushutfchar(L, 1);
  else {
    int i;
    solL_Buffer b;
    solL_buffinit(L, &b);
    for (i = 1; i <= n; i++) {
      pushutfchar(L, i);
      solL_addvalue(&b);
    }
    solL_pushresult(&b);
  }
  return 1;
}


/*
** offset(s, n, [i])  -> index where n-th character counting from
**   position 'i' starts; 0 means character at 'i'.
*/
static int byteoffset (sol_State *L) {
  size_t len;
  const char *s = solL_checklstring(L, 1, &len);
  sol_Integer n  = solL_checkinteger(L, 2);
  sol_Integer posi = (n >= 0) ? 1 : len + 1;
  posi = u_posrelat(solL_optinteger(L, 3, posi), len);
  solL_argcheck(L, 1 <= posi && --posi <= (sol_Integer)len, 3,
                   "position out of bounds");
  if (n == 0) {
    /* find beginning of current byte sequence */
    while (posi > 0 && iscontp(s + posi)) posi--;
  }
  else {
    if (iscontp(s + posi))
      return solL_error(L, "initial position is a continuation byte");
    if (n < 0) {
       while (n < 0 && posi > 0) {  /* move back */
         do {  /* find beginning of previous character */
           posi--;
         } while (posi > 0 && iscontp(s + posi));
         n++;
       }
     }
     else {
       n--;  /* do not move for 1st character */
       while (n > 0 && posi < (sol_Integer)len) {
         do {  /* find beginning of next character */
           posi++;
         } while (iscontp(s + posi));  /* (cannot pass final '\0') */
         n--;
       }
     }
  }
  if (n == 0)  /* did it find given character? */
    sol_pushinteger(L, posi + 1);
  else  /* no such character */
    solL_pushfail(L);
  return 1;
}


static int iter_aux (sol_State *L, int strict) {
  size_t len;
  const char *s = solL_checklstring(L, 1, &len);
  sol_Unsigned n = (sol_Unsigned)sol_tointeger(L, 2);
  if (n < len) {
    while (iscontp(s + n)) n++;  /* go to next character */
  }
  if (n >= len)  /* (also handles original 'n' being negative) */
    return 0;  /* no more codepoints */
  else {
    utfint code;
    const char *next = utf8_decode(s + n, &code, strict);
    if (next == NULL || iscontp(next))
      return solL_error(L, MSGInvalid);
    sol_pushinteger(L, n + 1);
    sol_pushinteger(L, code);
    return 2;
  }
}


static int iter_auxstrict (sol_State *L) {
  return iter_aux(L, 1);
}

static int iter_auxlax (sol_State *L) {
  return iter_aux(L, 0);
}


static int iter_codes (sol_State *L) {
  int lax = sol_toboolean(L, 2);
  const char *s = solL_checkstring(L, 1);
  solL_argcheck(L, !iscontp(s), 1, MSGInvalid);
  sol_pushcfunction(L, lax ? iter_auxlax : iter_auxstrict);
  sol_pushvalue(L, 1);
  sol_pushinteger(L, 0);
  return 3;
}


/* pattern to match a single UTF-8 character */
#define UTF8PATT	"[\0-\x7F\xC2-\xFD][\x80-\xBF]*"


static const solL_Reg funcs[] = {
  {"offset", byteoffset},
  {"codepoint", codepoint},
  {"char", utfchar},
  {"len", utflen},
  {"codes", iter_codes},
  /* placeholders */
  {"charpattern", NULL},
  {NULL, NULL}
};


SOLMOD_API int solopen_utf8 (sol_State *L) {
  solL_newlib(L, funcs);
  sol_pushlstring(L, UTF8PATT, sizeof(UTF8PATT)/sizeof(char) - 1);
  sol_setfield(L, -2, "charpattern");
  return 1;
}

