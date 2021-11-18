
#define PI 3.1415926535;

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

float random(float f) 
{
    uint h = hash(floatBitsToUint(f));
    h &= 0x007FFFFFu;
    h |= 0x3F800000u;
    return uintBitsToFloat(h) - 1.0;
}
