#ifndef _RAND_H_

#define _RAND_H_
#define SIM_RAND_MAX         32767

/*
 * This random generators are implementing 
 * by following POSIX.1-2001 directives.
 */

//__thread unsigned long next = 1;
#define IA 16807
#define IM 2147483647
#define AM (1.0/IM)
#define IQ 127773
#define IR 2836
#define MASK 123459876

extern __thread unsigned long next;

inline static long simRandom(void) {
    next = next * 1103515245 + 12345;
    return((unsigned)(next/65536) % 32768);
}

inline static void simSRandom(unsigned long seed) {
    next = seed;
}

/*
 * In Numerical Recipes in C: The Art of Scientific Computing 
 * (William H. Press, Brian P. Flannery, Saul A. Teukolsky, William T. Vetterling;
 *  New York: Cambridge University Press, 1992 (2nd ed., p. 277))
 */
inline static long simRandomRange(long low, long high) {
    return low + (long) ( ((double) high)* (simRandom() / (SIM_RAND_MAX + 1.0)));
}

inline static float random0(long *idum){
long k;
float ans;
*idum ^= MASK;
k=(*idum)/IQ; 
*idum=IA*(*idum-k*IQ)-IR*k;
if (*idum < 0) *idum += IM;
ans=AM*(*idum); 
*idum ^= MASK;
return ans;
}

#endif
