#ifndef __MTWISTER_H__
#define __MTWISTER_H__

// Source code from https://github.com/ESultanik/mtwister

#include <stdint.h>

#define STATE_VECTOR_LENGTH 624
#define STATE_VECTOR_M 397 /* changes to STATE_VECTOR_LENGTH also require changes to this */

typedef struct tagMTRand {
  uint32_t mt[STATE_VECTOR_LENGTH];
  int32_t index;
} MTRand;

MTRand seedRand(uint32_t seed);
uint32_t genRandLong(MTRand* rand);
double genRand(MTRand* rand);

#endif /* #ifndef __MTWISTER_H__ */
