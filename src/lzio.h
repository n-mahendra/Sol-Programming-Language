/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in sol.h
*/


#ifndef lzio_h
#define lzio_h

#include "sol.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : solZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define solZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define solZ_buffer(buff)	((buff)->buffer)
#define solZ_sizebuffer(buff)	((buff)->buffsize)
#define solZ_bufflen(buff)	((buff)->n)

#define solZ_buffremove(buff,i)	((buff)->n -= (i))
#define solZ_resetbuffer(buff) ((buff)->n = 0)


#define solZ_resizebuffer(L, buff, size) \
	((buff)->buffer = solM_reallocvchar(L, (buff)->buffer, \
				(buff)->buffsize, size), \
	(buff)->buffsize = size)

#define solZ_freebuffer(L, buff)	solZ_resizebuffer(L, buff, 0)


SOLI_FUNC void solZ_init (sol_State *L, ZIO *z, sol_Reader reader,
                                        void *data);
SOLI_FUNC size_t solZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  sol_Reader reader;		/* reader function */
  void *data;			/* additional data */
  sol_State *L;			/* Sol state (for reader) */
};


SOLI_FUNC int solZ_fill (ZIO *z);

#endif
