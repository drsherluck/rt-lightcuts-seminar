#define PI 3.1415926535;
#define INVALID_ID 0xffffffff
#define FLT_MAX 3.40282e38
#define FLT_MIN 1.17549e-38

// https://graphics.stanford.edu/~seander/bithacks.html#IntegerLog
uint get_msb(uint v)
{
    uint r;
    uint shift;
    r = uint(v > 0xFFFF) << 4; v >>= r;
    shift = uint(v > 0xFF) << 3; v >>= shift; r |= shift;
    shift = uint(v > 0xF ) << 2; v >>= shift; r |= shift;
    shift = uint(v > 0x3 ) << 1; v >>= shift; r |= shift;
    r |= (v >> 1);
    return r;
}

//https://amindforeverprogramming.blogspot.com/2013/07/random-floats-in-glsl-330.html
uint hash(uint x) 
{
    x += (x << 10u);
    x ^= (x >>  6u);
    x += (x <<  3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}

uint hash(uvec4 v)
{
    return hash(v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w));
}

float random(vec4 f)
{
    uint h = hash(floatBitsToUint(f));
    h &= 0x007FFFFFu;
    h |= 0x3F800000u;
    return uintBitsToFloat(h) - 1.0;
}

float random(float f) 
{
    uint h = hash(floatBitsToUint(f));
    h &= 0x007FFFFFu;
    h |= 0x3F800000u;
    return uintBitsToFloat(h) - 1.0;
}
