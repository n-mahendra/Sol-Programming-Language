/*
** $Id: ltable.h $
** Sol tables (hash)
** See Copyright Notice in sol.h
*/

#ifndef ltable_h
#define ltable_h

#include "lobject.h"


#define gnode(t,i)	(&(t)->node[i])
#define gval(n)		(&(n)->i_val)
#define gnext(n)	((n)->u.next)


/*
** Clear all bits of fast-access metamethods, which means that the table
** may have any of these metamethods. (First access that fails after the
** clearing will set the bit again.)
*/
#define invalidateTMcache(t)	((t)->flags &= ~maskflags)


/* true when 't' is using 'dummynode' as its hash part */
#define isdummy(t)		((t)->lastfree == NULL)


/* allocated size for hash nodes */
#define allocsizenode(t)	(isdummy(t) ? 0 : sizenode(t))


/* returns the Node, given the value of a table entry */
#define nodefromval(v)	cast(Node *, (v))


SOLI_FUNC const TValue *solH_getint (Table *t, sol_Integer key);
SOLI_FUNC void solH_setint (sol_State *L, Table *t, sol_Integer key,
                                                    TValue *value);
SOLI_FUNC const TValue *solH_getshortstr (Table *t, TString *key);
SOLI_FUNC const TValue *solH_getstr (Table *t, TString *key);
SOLI_FUNC const TValue *solH_get (Table *t, const TValue *key);
SOLI_FUNC void solH_set (sol_State *L, Table *t, const TValue *key,
                                                 TValue *value);
SOLI_FUNC void solH_finishset (sol_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
SOLI_FUNC Table *solH_new (sol_State *L);
SOLI_FUNC void solH_resize (sol_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
SOLI_FUNC void solH_resizearray (sol_State *L, Table *t, unsigned int nasize);
SOLI_FUNC void solH_free (sol_State *L, Table *t);
SOLI_FUNC int solH_next (sol_State *L, Table *t, StkId key);
SOLI_FUNC sol_Unsigned solH_getn (Table *t);
SOLI_FUNC unsigned int solH_realasize (const Table *t);


#if defined(SOL_DEBUG)
SOLI_FUNC Node *solH_mainposition (const Table *t, const TValue *key);
#endif


#endif
