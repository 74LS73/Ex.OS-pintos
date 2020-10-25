#ifndef __LIB_FIXED_POINT_H
#define __LIB_FIXED_POINT_H

/* define fixed point base data type to int, length = 31, signed */
typedef int fixpt32_t;

/* define float point length = 16*/
#define FLOAT_LENGTH 16

#define FPT_BASE (1 << FLOAT_LENGTH)

/* convert int to fixpt32_t */
#define FPT32(NUM) (NUM << FLOAT_LENGTH)

/* convert fixpt32_t to int (rounding toward zero)*/
#define FPT_INTZ(NUM) (NUM >> FLOAT_LENGTH)

/* convert fixpt32_t to int (rounding to nearest) */
#define FPT_INTN(NUM) (NUM >= 0 ? (NUM + (FPT_BASE >> 1)) / FPT_BASE : (NUM - (FPT_BASE >> 1)) / FPT_BASE)

/* add two fixpt32_t number */
#define FPT_ADD(NUMA,NUMB) (NUMA + NUMB)

/* subtract two fixpt32_t number */
#define FPT_SUB(NUMA,NUMB) (NUMA - NUMB)

/* add fixpt32_t and int */
#define FPT_ADD_INT(NUMA,INTB) (NUMA + FPT32(INTB))

/* subtract int from fixpt32_t */
#define FPT_SUB_INT(NUMA,INTB) (NUMA - FPT32(INTB))

/* multiply two fixpt32_t */
#define FPT_MUL(NUMA,NUMB) (((int64_t)NUMA) * NUMB / FPT_BASE)

/* multiply fixpt32_t by int */
#define FPT_MUL_INT(NUMA,INTB) (NUMA * INTB)

/* divide two fixpt32_t */
#define FPT_DIV(NUMA,NUMB) (((int64_t)NUMA) * FPT_BASE / NUMB)

/* divide fixpt32_t by int */
#define FPT_DIV_INT(NUMA,INTB) (NUMA / INTB)

#endif /* lib/fixed-point.h */