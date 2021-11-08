#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define FORCEINLINE inline __attribute__((always_inline))
#define EPSILON 0.000001

typedef float    f32;
typedef double   f64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

inline u64 align(u64 offset, u64 alignment)
{
    return (offset + alignment - 1) & ~(alignment - 1);
}


#endif // COMMON_H
