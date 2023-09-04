#ifndef STDBOOL_H_
#define STDBOOL_H_

/**
 * stdbool.h
 * Author    - zm
 * E-mail    - xxx&@cc.com
 * Date      - 
 * Copyright - You are free to use for any purpose except illegal acts
 * Warrenty  - None: don't blame me if it breaks something
 *
 * In ISO C99, stdbool.h is a standard header and _Bool is a keyword, but
 * some compilers don't offer these yet. This header file is an 
 * implementation of the stdbool.h header file.
 *
 */

#if defined __STDC__ && defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L
/* have a C99 compiler */
typedef _Bool boolean;
#else
/* do not have a C99 compiler */
typedef unsigned char boolean;
#endif

/**
 * Define the Boolean macros only if they are not already defined.
 */
#ifndef __bool_true_false_are_defined
#define bool boolean
#define false 0 
#define true 1
#define __bool_true_false_are_defined 1
#endif /* __bool_true_false_are_defined */


#endif /* STDBOOL_H_ */

