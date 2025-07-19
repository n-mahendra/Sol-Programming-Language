/*
** $Id: lcode.h $
** Code generator for Sol
** See Copyright Notice in sol.h
*/

#ifndef lcode_h
#define lcode_h

#include "llex.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)


/*
** grep "ORDER OPR" if you change these enums  (ORDER OP)
*/
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR,
  OPR_SHL, OPR_SHR,
  /* string operator */
  OPR_CONCAT,
  /* comparison operators */
  OPR_EQ, OPR_LT, OPR_LE,
  OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;


/* true if operation is foldable (that is, it is arithmetic or bitwise) */
#define foldbinop(op)	((op) <= OPR_SHR)


#define solK_codeABC(fs,o,a,b,c)	solK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define solK_setmultret(fs,e)	solK_setreturns(fs, e, SOL_MULTRET)

#define solK_jumpto(fs,t)	solK_patchlist(fs, solK_jump(fs), t)

SOLI_FUNC int solK_code (FuncState *fs, Instruction i);
SOLI_FUNC int solK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx);
SOLI_FUNC int solK_codeABCk (FuncState *fs, OpCode o, int A,
                                            int B, int C, int k);
SOLI_FUNC int solK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
SOLI_FUNC void solK_fixline (FuncState *fs, int line);
SOLI_FUNC void solK_nil (FuncState *fs, int from, int n);
SOLI_FUNC void solK_reserveregs (FuncState *fs, int n);
SOLI_FUNC void solK_checkstack (FuncState *fs, int n);
SOLI_FUNC void solK_int (FuncState *fs, int reg, sol_Integer n);
SOLI_FUNC void solK_dischargevars (FuncState *fs, expdesc *e);
SOLI_FUNC int solK_exp2anyreg (FuncState *fs, expdesc *e);
SOLI_FUNC void solK_exp2anyregup (FuncState *fs, expdesc *e);
SOLI_FUNC void solK_exp2nextreg (FuncState *fs, expdesc *e);
SOLI_FUNC void solK_exp2val (FuncState *fs, expdesc *e);
SOLI_FUNC void solK_self (FuncState *fs, expdesc *e, expdesc *key);
SOLI_FUNC void solK_indexed (FuncState *fs, expdesc *t, expdesc *k);
SOLI_FUNC void solK_goiftrue (FuncState *fs, expdesc *e);
SOLI_FUNC void solK_goiffalse (FuncState *fs, expdesc *e);
SOLI_FUNC void solK_storevar (FuncState *fs, expdesc *var, expdesc *e);
SOLI_FUNC void solK_setreturns (FuncState *fs, expdesc *e, int nresults);
SOLI_FUNC void solK_setoneret (FuncState *fs, expdesc *e);
SOLI_FUNC int solK_jump (FuncState *fs);
SOLI_FUNC void solK_ret (FuncState *fs, int first, int nret);
SOLI_FUNC void solK_patchlist (FuncState *fs, int list, int target);
SOLI_FUNC void solK_patchtohere (FuncState *fs, int list);
SOLI_FUNC void solK_concat (FuncState *fs, int *l1, int l2);
SOLI_FUNC int solK_getlabel (FuncState *fs);
SOLI_FUNC void solK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
SOLI_FUNC void solK_infix (FuncState *fs, BinOpr op, expdesc *v);
SOLI_FUNC void solK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
SOLI_FUNC void solK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
SOLI_FUNC void solK_setlist (FuncState *fs, int base, int nelems, int tostore);
SOLI_FUNC void solK_finish (FuncState *fs);
SOLI_FUNC l_noret solK_semerror (LexState *ls, const char *msg);


#endif
