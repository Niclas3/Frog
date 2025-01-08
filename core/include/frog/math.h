#ifndef _FROG_MATH_H
#define _FROG_MATH_H

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })
#define ABS(x)  (((x)<0)?-(x):(x))

#endif
