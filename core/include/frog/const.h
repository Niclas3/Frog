#ifndef _FROG_CONST_H
#define _FROG_CONST_H

/* AC stands for as const
 * AT stands for as type
 **/
#define __AC(N, S) (N##S)
#define __AT(T, E) ((T)(E))

#define _AC(N, S) (__AC(N,S))
#define _AT(T, E) (__AT(T,E))

#define _UL(n)  (_AC(n,UL))
#define _ULL(n)  (_AC(n,ULL))

/*
 * set n as 1
 * */
#define _BITUL(n) (_UL(1) << (n))
#define _BITULL(n) (_ULL(1) << (n))


#endif
