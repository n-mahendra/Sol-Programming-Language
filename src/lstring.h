/*
** $Id: lstring.h $
** String table (keep all strings handled by Sol)
** See Copyright Notice in sol.h
*/

#ifndef lstring_h
#define lstring_h

#include "lgc.h"
#include "lobject.h"
#include "lstate.h"


/*
** Memory-allocation error message must be preallocated (it cannot
** be created after memory is exhausted)
*/
#define MEMERRMSG       "not enough memory"


/*
** Size of a TString: Size of the header plus space for the string
** itself (including final '\0').
*/
#define sizelstring(l)  (offsetof(TString, contents) + ((l) + 1) * sizeof(char))

#define solS_newliteral(L, s)	(solS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	((s)->tt == SOL_VSHRSTR && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == SOL_VSHRSTR, (a) == (b))


SOLI_FUNC unsigned int solS_hash (const char *str, size_t l, unsigned int seed);
SOLI_FUNC unsigned int solS_hashlongstr (TString *ts);
SOLI_FUNC int solS_eqlngstr (TString *a, TString *b);
SOLI_FUNC void solS_resize (sol_State *L, int newsize);
SOLI_FUNC void solS_clearcache (global_State *g);
SOLI_FUNC void solS_init (sol_State *L);
SOLI_FUNC void solS_remove (sol_State *L, TString *ts);
SOLI_FUNC Udata *solS_newudata (sol_State *L, size_t s, int nuvalue);
SOLI_FUNC TString *solS_newlstr (sol_State *L, const char *str, size_t l);
SOLI_FUNC TString *solS_new (sol_State *L, const char *str);
SOLI_FUNC TString *solS_createlngstrobj (sol_State *L, size_t l);


#endif
