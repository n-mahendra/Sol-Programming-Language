/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in sol.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Sol function (given call info) */
#define ci_func(ci)		(clLvalue(s2v((ci)->func.p)))


#define resethookcount(L)	(L->hookcount = L->basehookcount)

/*
** mark for entries in 'lineinfo' array that has absolute information in
** 'abslineinfo' array
*/
#define ABSLINEINFO	(-0x80)


/*
** MAXimum number of successive Instructions WiTHout ABSolute line
** information. (A power of two allows fast divisions.)
*/
#if !defined(MAXIWTHABS)
#define MAXIWTHABS	128
#endif


SOLI_FUNC int solG_getfuncline (const Proto *f, int pc);
SOLI_FUNC const char *solG_findlocal (sol_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
SOLI_FUNC l_noret solG_typeerror (sol_State *L, const TValue *o,
                                                const char *opname);
SOLI_FUNC l_noret solG_callerror (sol_State *L, const TValue *o);
SOLI_FUNC l_noret solG_forerror (sol_State *L, const TValue *o,
                                               const char *what);
SOLI_FUNC l_noret solG_concaterror (sol_State *L, const TValue *p1,
                                                  const TValue *p2);
SOLI_FUNC l_noret solG_opinterror (sol_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
SOLI_FUNC l_noret solG_tointerror (sol_State *L, const TValue *p1,
                                                 const TValue *p2);
SOLI_FUNC l_noret solG_ordererror (sol_State *L, const TValue *p1,
                                                 const TValue *p2);
SOLI_FUNC l_noret solG_runerror (sol_State *L, const char *fmt, ...);
SOLI_FUNC const char *solG_addinfo (sol_State *L, const char *msg,
                                                  TString *src, int line);
SOLI_FUNC l_noret solG_errormsg (sol_State *L);
SOLI_FUNC int solG_traceexec (sol_State *L, const Instruction *pc);
SOLI_FUNC int solG_tracecall (sol_State *L);


#endif
