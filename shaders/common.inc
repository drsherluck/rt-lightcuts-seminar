
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

