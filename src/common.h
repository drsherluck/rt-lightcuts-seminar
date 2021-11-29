#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <cfloat>

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

#define _randf() ((((f32) rand())/((f32) RAND_MAX)))
#define _randf2() (_randf() * (rand() % 2 ? -1.0 : 1.0))

inline u64 align(u64 offset, u64 alignment)
{
    return (offset + alignment - 1) & ~(alignment - 1);
}

#define SINGLETON(class_name) \
    public: \
        class_name(class_name const&) = delete;\
        class_name& operator=(class_name const&) = delete;\
        static class_name& get_instance() \
        {\
            static class_name instance;\
            return instance;\
        }\
    private: \
        class_name() = default;\
        ~class_name() = default;


#endif // COMMON_H
