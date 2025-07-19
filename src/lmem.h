/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in sol.h
*/

#ifndef lmem_h
#define lmem_h


#include <stddef.h>

#include "llimits.h"
#include "sol.h"


#define solM_error(L)	solD_throw(L, SOL_ERRMEM)


/*
** This macro tests whether it is safe to multiply 'n' by the size of
** type 't' without overflows. Because 'e' is always constant, it avoids
** the runtime division MAX_SIZET/(e).
** (The macro is somewhat complex to avoid warnings:  The 'sizeof'
** comparison avoids a runtime comparison when overflow cannot occur.
** The compiler should be able to optimize the real test by itself, but
** when it does it, it may give a warning about "comparison is always
** false due to limited range of data type"; the +1 tricks the compiler,
** avoiding this warning but also this optimization.)
*/
#define solM_testsize(n,e)  \
	(sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define solM_checksize(L,n,e)  \
	(solM_testsize(n,e) ? solM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' or 'unsigned int' and that 'int' is not larger than 'size_t'.)
*/
#define solM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_uint((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define solM_reallocvchar(L,b,on,n)  \
  cast_charp(solM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define solM_freemem(L, b, s)	solM_free_(L, (b), (s))
#define solM_free(L, b)		solM_free_(L, (b), sizeof(*(b)))
#define solM_freearray(L, b, n)   solM_free_(L, (b), (n)*sizeof(*(b)))

#define solM_new(L,t)		cast(t*, solM_malloc_(L, sizeof(t), 0))
#define solM_newvector(L,n,t)	cast(t*, solM_malloc_(L, (n)*sizeof(t), 0))
#define solM_newvectorchecked(L,n,t) \
  (solM_checksize(L,n,sizeof(t)), solM_newvector(L,n,t))

#define solM_newobject(L,tag,s)	solM_malloc_(L, (s), tag)

#define solM_growvector(L,v,nelems,size,t,limit,e) \
	((v)=cast(t *, solM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         solM_limitN(limit,t),e)))

#define solM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, solM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define solM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, solM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

SOLI_FUNC l_noret solM_toobig (sol_State *L);

/* not to be called directly */
SOLI_FUNC void *solM_realloc_ (sol_State *L, void *block, size_t oldsize,
                                                          size_t size);
SOLI_FUNC void *solM_saferealloc_ (sol_State *L, void *block, size_t oldsize,
                                                              size_t size);
SOLI_FUNC void solM_free_ (sol_State *L, void *block, size_t osize);
SOLI_FUNC void *solM_growaux_ (sol_State *L, void *block, int nelems,
                               int *size, int size_elem, int limit,
                               const char *what);
SOLI_FUNC void *solM_shrinkvector_ (sol_State *L, void *block, int *nelem,
                                    int final_n, int size_elem);
SOLI_FUNC void *solM_malloc_ (sol_State *L, size_t size, int tag);

#endif

