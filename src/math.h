#ifndef MATHC_H
#define MATHC_H

#include "common.h"
#include <math.h>
#include <cassert>

#define PI 3.1415926535

#ifdef USE_SIMD_MATH
#include "immintrin.h"
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif 

union v2
{
    f32 data[2];
    struct 
    {
        f32 x, y; 
    };
    struct 
    {
        f32 u, v;
    };
    struct 
    {
        f32 w, h;
    };

    f32& operator[](i32);
    f32  operator[](i32) const;
};

union v3
{
    f32 data[3];
    struct 
    { 
        f32 x, y, z; 
    };
    struct 
    { 
        f32 r, g, b; 
    };
    struct 
    {
        f32 w, h, d;
    };
    struct
    {
        f32 i, j, k;
    };
    
    f32& operator[](i32);
    f32  operator[](i32) const;
};

union v4 
{
    f32 data[4];
    struct 
    { 
        f32 x, y, z, w; 
    };
    struct 
    { 
        f32 r, g, b, a;
    };
    struct 
    {
        v3  xyz;
        f32 _i0;
    };
#ifdef USE_SIMD_MATH
    __m128 s4;
#endif

    f32& operator[](i32);
    f32  operator[](i32) const;
};

union m3x3
{
    f32 data[3][3];
    v3  col[3];

    v3& operator[](i32);
    const v3& operator[](i32) const;
};

union m4x4
{
    f32 data[4][4];
    v4  col[4];

    v4& operator[](i32);
    const v4& operator[](i32) const;
};

// remove later
typedef v4 aligned_v3;
typedef m4x4 aligned_m3x3;

union quat
{
    v4 data;
    struct 
    {
        f32 re;
        v3  im;
    };
    struct 
    {
        f32 a, b, c, d;
    };
    struct 
    {
        f32 s, x, y, z;
    };
};

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

// section utility

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

inline i32 mod(i32 k, i32 n)
{
	return (k %= n) < 0 ? k + n : k;
}

inline f32 radians(f32 deg)
{
	return deg * 0.0174532925199f;
}

inline f32 clamp(f32 value, f32 min_value, f32 max_value)
{
    if (value > max_value)
    {
        return max_value;
    }
    if (value < min_value)
    {
        return min_value;
    }
    return value;
}

inline u32 next_pow2(u32 v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
};

// section: v2 

inline v2 vec2()
{
    v2 res = {0};
    return res;
}

inline v2 vec2(f32 s)
{
    v2 res = {s, s};
    return res;
}

inline v2 vec2(f32 x, f32 y)
{
    v2 res = {x, y};
    return res;
}

inline f32& v2::operator[](i32 i) 
{
    return data[i];
}

inline f32 v2::operator[](i32 i) const
{
    return data[i];
}

inline v2 operator-(v2& v)
{
    v2 res = {-v.x, -v.y};
    return res;
}

inline void operator+=(v2& l, const v2& r)
{
    l.x += r.x;
    l.y += r.y;
}

inline void operator-=(v2& l, const v2& r)
{
    l.x -= r.x;
    l.y -= r.x;
}

inline void operator*=(v2& v, f32 s)
{
    v.x *= s;
    v.y *= s;
}

inline void operator*=(v2& v0, v2 v1)
{
    v0.x *= v1.x;
    v0.y *= v1.y;
}

inline void operator/=(v2& v, f32 s)
{
    v *= (1.0f/s);
}

inline v2 operator+(v2 l, const v2& r)
{
    v2 res = {l.x + r.x, l.y + r.y};
    return res;
}

inline v2 operator-(v2 l, const v2& r)
{
    v2 res = {l.x + r.x, l.y + r.y};
    return res;
}

inline v2 operator*(f32 s, v2 v)
{
    v2 res = {v.x*s, v.y*s};
    return res;
}

inline v2 operator*(v2 v, f32 s)
{
    v2 res = {v.x*s, v.y*s};
    return res;
}

inline v2 operator*(v2 v0, v2 v1)
{
    v2 res = {v0.x*v1.x, v0.y*v1.y};
    return res;
}

inline v2 operator/(v2 v, f32 s)
{
    f32 f = (1.0f/s);
    v2 res = {v.x*f, v.y*f};
    return res;
}

inline bool operator==(const v2& l, const v2& r)
{
    return (l.x == r.x && l.y == r.y);
}

inline bool operator!=(const v2& l, const v2& r)
{
    return !(l == r);
}

inline v2 floor(v2 v)
{
    v.x = floor(v.x);
    v.y = floor(v.y);
    return v;
}

// section: v3

inline v3 vec3()
{
    v3 res = {0};
    return res;
}

inline v3 vec3(f32 s)
{
    v3 res = {s, s, s};
    return res;
}

inline v3 vec3(f32 x, f32 y, f32 z)
{
    v3 res = {x, y, z};
    return res;
}

inline f32& v3::operator[](i32 i)
{
    return data[i];
}

inline f32 v3::operator[](i32 i) const
{
    return data[i];
}

inline v3 operator-(v3& v)
{
    v3 res = {-v.x, -v.y, -v.z};
    return res;
}

inline void operator+=(v3& l, const v3& r)
{
    l.x += r.x;
    l.y += r.y;
    l.z += r.z;
}

inline void operator-=(v3& l, const v3& r)
{
    l.x -= r.x;
    l.y -= r.y;
    l.z -= r.z;
}

inline void operator*=(v3& v, f32 s)
{
    v.x *= s;
    v.y *= s;
    v.z *= s;
}

inline void operator/=(v3& v, f32 s)
{
    v *= (1.0f/s);
}

inline v3 operator+(v3 l, const v3& r)
{
    v3 res = {l.x + r.x, l.y + r.y, l.z + r.z};
    return res;
}

inline v3 operator-(v3 l, const v3& r)
{
    v3 res = {l.x - r.x, l.y - r.y, l.z - r.z};
    return res;
}

inline v3 operator*(f32 s, v3 v)
{
    v3 res = {v.x*s, v.y*s, v.z*s};
    return res;
}

inline v3 operator*(v3 v, f32 s)
{
    v3 res = {v.x*s, v.y*s, v.z*s};
    return res;
}

inline v3 operator/(v3 v, f32 s)
{
    return v * (1.0f/s);
}

inline bool operator==(const v3& l, const v3& r)
{
    return (l.x == r.x && l.y == r.y && l.z == r.z);
}

inline bool operator!=(const v3& l, const v3& r)
{
    return !(l == r);
}

inline void setv3(v3& v, f32 s)
{
    v.x = s;
    v.y = s;
    v.z = s;
}

inline void setv3(v3& v, f32 x, f32 y, f32 z)
{
    v.x = x;
    v.y = y;
    v.z = z;
}

inline f32 dot(const v3& l, const v3& r)
{
    return l.x * r.x + l.y * r.y + l.z * r.z;
}

inline f32 length(const v3& v)
{
    return sqrtf(dot(v, v));
}

inline v3 normalize(v3 v)
{
    f32 len = length(v);
    if (len > 0)
    {
        f32 inv = 1.0f/len;
        v.x *= inv;
        v.y *= inv;
        v.z *= inv;
    }
    return v;
}

inline v3 normalized(f32 x, f32 y, f32 z)
{
   v3 res = {x, y, z};
   f32 len = length(res);
   if (len > 0)
   {
       res /= len;
   }
   return res;
}

inline v3 cross(const v3& l, const v3& r)
{
    v3 res;
    res[0] = l.y * r.z - l.z * r.y;
    res[1] = l.z * r.x - l.x * r.z;
    res[2] = l.x * r.y - l.y * r.x;
    return res;
}

// section: v4

inline v4 vec4()
{
    v4 res = {0};
    return res;
}

inline v4 vec4(f32 s)
{
    v4 res = {s, s, s};
    return res;
}

inline v4 vec4(f32 x, f32 y, f32 z, f32 w)
{
    v4 res = {x, y, z, w};
    return res;
}

inline v4 vec4(v3 xyz, f32 w)
{
    v4 res;
    res.xyz = xyz;
    res.w = w;
    return res;
}

inline f32& v4::operator[](i32 i)
{
    return data[i];
}

inline f32 v4::operator[](i32 i) const
{
    return data[i];
}

inline v4 operator-(v4& v)
{
    v4 res = {-v.x, -v.y, -v.z, -v.w};
    return res;
}

inline void operator+=(v4& l, const v4& r)
{
#ifdef USE_SIMD_MATH
    l.s4 = _mm_add_ps(l.s4, r.s4);
#else 
    l.x += r.x;
    l.y += r.y;
    l.z += r.z;
    l.w += l.w;
#endif
}

inline void operator-=(v4& l, const v4& r)
{
#ifdef USE_SIMD_MATH
    l.s4 = _mm_sub_ps(l.s4, r.s4);
#else
    l.x -= r.x;
    l.y -= r.y;
    l.z -= r.z;
    l.w -= l.w;
#endif
}

inline void operator*=(v4& v, f32 s)
{
#ifdef USE_SIMD_MATH
    __m128 s128 = _mm_set1_ps(s);
    v.s4 = _mm_mul_ps(v.s4, s128);
#else
    v.x *= s;
    v.y *= s;
    v.z *= s;
    v.w *= s;
#endif
}

inline void operator/=(v4& v, f32 s)
{
    v *= (1.0f/s);
}

inline v4 operator+(v4 l, const v4& r)
{
    l += r;
    return l;
}

inline v4 operator-(v4 l, const v4& r)
{
    l -= r;
    return l;
}

inline v4 operator*(f32 s, v4 v)
{
    v *= s;
    return v;
}

inline v4 operator*(v4 v, f32 s)
{
    v *= s;
    return v;
}

inline v4 operator/(v4 v, f32 s)
{
    v *= (1.0f/s);
    return v;
}

inline bool operator==(const v4& l, const v4& r)
{
    return (l.x == r.x && l.y == r.y && l.z == r.z && l.w == r.w);
}

inline bool operator!=(const v4& l, const v4& r)
{
    return !(l == r);
}

inline void setv4(v4& v, f32 s)
{
    v.x = s;
    v.y = s;
    v.z = s;
    v.w = s;
}

inline void setv4(v4& v, f32 x, f32 y, f32 z, f32 w)
{
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
}

inline f32 dot(const v4& l, const v4& r)
{
#ifdef USE_SIMD_MATH
    __m128 mulr, shufr, sumr;
    mulr  = _mm_mul_ps(l.s4, r.s4);
    shufr = _mm_movehdup_ps(mulr);
    sumr  = _mm_add_ps(mulr, shufr);
    shufr = _mm_movehl_ps(shufr, sumr);
    sumr  = _mm_add_ss(sumr, shufr);
    return _mm_cvtss_f32(sumr);
#else 
    return l.x * r.x + l.y * r.y + l.z * r.z + l.w * r.w;
#endif
}

inline f32 length(const v4& v)
{
    return sqrtf(dot(v, v));
}

inline v4 normalize(v4 v)
{
    f32 len = length(v);
    
    if (len)
    {
        f32 inv = 1.0f/len;
#ifdef USE_SIMD_MATH
        __m128 inv128 = _mm_set1_ps(inv);
        v.s4 = _mm_mul_ps(v.s4, inv128);
#else 
        v.x *= inv;
        v.y *= inv;
        v.z *= inv;
        v.w *= inv;
#endif 
    }
    return v;
}

inline v4 normalized(f32 x, f32 y, f32 z, f32 w)
{
   v4 res = {x, y, z, w};
   f32 len = length(res);
   if (len > 0)
   {
       res /= len;
   }
   return res;
}

// section: mat3x3

inline m3x3 mat3x3() 
{
    m3x3 res = {0};
    return res;
}

inline m3x3 mat3x3d(f32 d)
{
    m3x3 res = {
        d, 0, 0,
        0, d, 0,
        0, 0, d,
    };
    return res;
}

inline v3& m3x3::operator[](i32 i)
{
    return col[i];
}

inline const v3& m3x3::operator[](i32 i) const
{
    return col[i];
}

inline m3x3 operator*(const m3x3& l, const m3x3& r) 
{
    m3x3 res;
    // first column
    res.data[0][0] = l[0][0] * r[0][0] + l[1][0] * r[0][1] + l[2][0] * r[0][2];
    res.data[0][1] = l[0][1] * r[0][0] + l[1][1] * r[0][1] + l[2][1] * r[0][2];
    res.data[0][2] = l[0][2] * r[0][0] + l[1][2] * r[0][1] + l[2][2] * r[0][2];
    // second column
    res.data[1][0] = l[0][0] * r[1][0] + l[1][0] * r[1][1] + l[2][0] * r[1][2];
    res.data[1][1] = l[0][1] * r[1][0] + l[1][1] * r[1][1] + l[2][1] * r[1][2];
    res.data[1][2] = l[0][2] * r[1][0] + l[1][2] * r[1][1] + l[2][2] * r[1][2];
    // third column
    res.data[2][0] = l[0][0] * r[2][0] + l[1][0] * r[2][1] + l[2][0] * r[2][2];
    res.data[2][1] = l[0][1] * r[2][0] + l[1][1] * r[2][1] + l[2][1] * r[2][2];
    res.data[2][2] = l[0][2] * r[2][0] + l[1][2] * r[2][1] + l[2][2] * r[2][2];
    return res;
}

inline void operator*=(m3x3& l, const m3x3& r)
{
    l = l * r;
}

inline bool operator==(const m3x3& l, const m3x3& r)
{
    return (l.col[0] == r.col[0] && 
            l.col[1] == r.col[1] &&
            l.col[2] == r.col[2]);
}

inline bool operator!=(const m3x3& l, const m3x3& r)
{
    return !(l == r);
}

inline m3x3 transpose(const m3x3& m)
{
    m3x3 res ={
        m[0].x, m[1].x, m[2].x, 
        m[0].y, m[1].y, m[2].y,
        m[0].z, m[1].z, m[2].z,
    };
    return res;
}

#ifndef det3x3
#define det3x3(v00, v01, v02, v10, v11, v12, v20, v21, v22) \
(v00*v11*v22 + v10*v21*v02 + v20*v01*v12 \
 - v02*v11*v20 - v12*v21*v00 - v22*v01*v10)
#endif

inline f32 determinant(const m3x3& m)
{
    return det3x3(m.data[0][0], m.data[0][1], m.data[0][2],
            m.data[1][0], m.data[1][1], m.data[1][2],
            m.data[2][0], m.data[2][1], m.data[2][2]);
}

inline m3x3 translate3x3(v2 v)
{
    m3x3 res = {
        1, 0, 0,
        0, 1, 0,
        v.x, v.y, 1,
    };
    return res;
}

inline m3x3 scale3x3(f32 s)
{
    m3x3 res = {
        s, 0, 0,
        0, s, 0, 
        0, 0, 1,
    };
    return res;
}

inline m3x3 rotate3x3_x(f32 angle)
{
    f32 c = cosf(angle);
    f32 s = sinf(angle);
    m3x3 res = {
        1,  0, 0, 
        0,  c, s,
        0, -s, c,
    };
    return res;
}

inline m3x3 rotate3x3_y(f32 angle)
{
    f32 c = cosf(angle);
    f32 s = sinf(angle);
    m3x3 res = {
        c, 0, -s, 
        0, 1,  0,
        s, 0,  c,
    };
    return res;
}

inline m3x3 rotate3x3_z(f32 angle)
{
    f32 c = cosf(angle);
    f32 s = sinf(angle);
    m3x3 res = {
         c, s, 0, 
        -s, c, 0,
         0, 0, 1,
    };
    return res;
}

inline m3x3 inverse(const m3x3& m)
{
	m3x3 r;
    f32 d = determinant(m);
    assert(d);
	d = (1.0f/d);
	r[0][0] = (m[1][1]*m[2][2] - m[1][2]*m[2][1])*d;
	r[0][1] = (m[1][2]*m[2][0] - m[2][2]*m[1][0])*d;
	r[0][2] = (m[1][0]*m[2][1] - m[2][0]*m[1][1])*d;
	r[1][0] = (m[0][2]*m[2][1] - m[2][2]*m[0][1])*d;
	r[1][1] = (m[0][0]*m[2][2] - m[2][0]*m[0][2])*d;
	r[1][2] = (m[0][1]*m[2][0] - m[2][1]*m[0][0])*d;
	r[2][0] = (m[0][1]*m[1][2] - m[1][1]*m[0][2])*d;
	r[2][1] = (m[0][2]*m[1][0] - m[1][2]*m[0][0])*d;
	r[2][2] = (m[0][0]*m[1][1] - m[1][0]*m[1][1])*d;
	return r;
}

// section: mat4x4

inline m4x4 mat4x4()
{
    m4x4 res = {0};
    return res;
}

inline m4x4 mat4x4d(f32 d)
{
    m4x4 res = {
        d, 0, 0, 0,
        0, d, 0, 0,
        0, 0, d, 0,
        0, 0, 0, d,
    };
    return res;
}

inline v4& m4x4::operator[](i32 i)
{
    return col[i];
}

inline const v4& m4x4::operator[](i32 i) const
{
    return col[i];
}

inline void operator*=(m4x4& m, f32 s)
{
    m.col[0] *= s;
    m.col[1] *= s;
    m.col[2] *= s;
    m.col[3] *= s;
}

inline m4x4 operator*(m4x4 m, f32 s)
{
    m *= s;
    return m;
}

inline m4x4 operator*(f32 s, m4x4 m)
{
    m *= s;
    return m;
}

inline m4x4 operator*(const m4x4& l, const m4x4& r) 
{
    m4x4 res;
    // first column
    res.col[0][0] = l[0][0] * r[0][0] + l[1][0] * r[0][1] + l[2][0] * r[0][2] + l[3][0] * r[0][3];
    res.col[0][1] = l[0][1] * r[0][0] + l[1][1] * r[0][1] + l[2][1] * r[0][2] + l[3][1] * r[0][3];
    res.col[0][2] = l[0][2] * r[0][0] + l[1][2] * r[0][1] + l[2][2] * r[0][2] + l[3][2] * r[0][3];
    res.col[0][3] = l[0][3] * r[0][0] + l[1][3] * r[0][1] + l[2][3] * r[0][2] + l[3][3] * r[0][3];
    // second column
    res.col[1][0] = l[0][0] * r[1][0] + l[1][0] * r[1][1] + l[2][0] * r[1][2] + l[3][0] * r[1][3];
    res.col[1][1] = l[0][1] * r[1][0] + l[1][1] * r[1][1] + l[2][1] * r[1][2] + l[3][1] * r[1][3];
    res.col[1][2] = l[0][2] * r[1][0] + l[1][2] * r[1][1] + l[2][2] * r[1][2] + l[3][2] * r[1][3];
    res.col[1][3] = l[0][3] * r[1][0] + l[1][3] * r[1][1] + l[2][3] * r[1][2] + l[3][3] * r[1][3];
    // third column
    res.col[2][0] = l[0][0] * r[2][0] + l[1][0] * r[2][1] + l[2][0] * r[2][2] + l[3][0] * r[2][3];
    res.col[2][1] = l[0][1] * r[2][0] + l[1][1] * r[2][1] + l[2][1] * r[2][2] + l[3][1] * r[2][3];
    res.col[2][2] = l[0][2] * r[2][0] + l[1][2] * r[2][1] + l[2][2] * r[2][2] + l[3][2] * r[2][3];
    res.col[2][3] = l[0][3] * r[2][0] + l[1][3] * r[2][1] + l[2][3] * r[2][2] + l[3][3] * r[2][3];
    // fourth column
    res.col[3][0] = l[0][0] * r[3][0] + l[1][0] * r[3][1] + l[2][0] * r[3][2] + l[3][0] * r[3][3];
    res.col[3][1] = l[0][1] * r[3][0] + l[1][1] * r[3][1] + l[2][1] * r[3][2] + l[3][1] * r[3][3];
    res.col[3][2] = l[0][2] * r[3][0] + l[1][2] * r[3][1] + l[2][2] * r[3][2] + l[3][2] * r[3][3];
    res.col[3][3] = l[0][3] * r[3][0] + l[1][3] * r[3][1] + l[2][3] * r[3][2] + l[3][3] * r[3][3];
    return res;
}

inline void operator*=(m4x4& l, const m4x4& r)
{
    l = l * r;
}

inline v4 operator*(const m4x4& m, const v4& v)
{
    v4 res;
    res.x = m[0][0]*v.x + m[1][0]*v.y + m[2][0]*v.z + m[3][0]*v.w;
    res.y = m[0][1]*v.x + m[1][1]*v.y + m[2][1]*v.z + m[3][1]*v.w;
    res.z = m[0][2]*v.x + m[1][2]*v.y + m[2][2]*v.z + m[3][2]*v.w;
    res.w = m[0][3]*v.x + m[1][3]*v.y + m[2][3]*v.z + m[3][3]*v.w;
    return res;
}

inline bool operator==(const m4x4& l, const m4x4& r)
{
    return (l.col[0] == r.col[0] &&
            l.col[1] == r.col[1] &&
            l.col[2] == r.col[2] &&
            l.col[3] == r.col[3]);
}

inline bool operator!=(const m4x4& l, const m4x4& r)
{
    return !(l == r);
}

inline m4x4 transpose(const m4x4& m)
{
    m4x4 res = {
        m[0].x, m[1].x, m[2].x, m[3].x,
        m[0].y, m[1].y, m[2].y, m[3].y,
        m[0].z, m[1].z, m[2].z, m[3].z,
        m[0].w, m[1].w, m[2].w, m[3].w,
    };
    return res;
}

inline f32 determinant(const m4x4& m)
{
    f32 d = 0;
    d += m.col[0][0] * det3x3(m.col[1][1], m.col[1][2], m.col[1][3],
            m.col[2][1], m.col[2][2], m.col[2][3],
            m.col[3][1], m.col[3][2], m.col[3][3]);
    d -= m.col[1][0] * det3x3(m.col[0][1], m.col[0][2], m.col[0][3],
            m.col[2][1], m.col[2][2], m.col[2][3],
            m.col[3][1], m.col[3][2], m.col[3][3]);
    d += m.col[2][0] * det3x3(m.col[0][1], m.col[0][2], m.col[0][3],
            m.col[1][1], m.col[1][2], m.col[1][3],
            m.col[3][1], m.col[3][2], m.col[3][3]);
    d -= m.col[3][0] * det3x3(m.col[0][1], m.col[0][2], m.col[0][3],
            m.col[1][1], m.col[1][2], m.col[1][3],
            m.col[2][1], m.col[2][2], m.col[2][3]);
    return d;
}

inline m4x4 scale4x4(f32 s)
{
    m4x4 res = {
        s, 0, 0, 0,
        0, s, 0, 0,
        0, 0, s, 0,
        0, 0, 0, 1,
    };
    return res;
}

inline m4x4 translate4x4(f32 x, f32 y, f32 z)
{
    m4x4 res = {
        1, 0, 0, 0,
        0, 1, 0, 0, 
        0, 0, 1, 0, 
        x, y, z, 1,
    };
    return res;
}

inline m4x4 translate4x4(v3 v)
{
    m4x4 res = mat4x4d(1);
    res[3].xyz = v;
    return res;
}

inline m4x4 rotate4x4_x(f32 angle)
{
    f32 c = cosf(angle);
    f32 s = sinf(angle);
    m4x4 res = {
        1,  0, 0, 0,
        0,  c, s, 0,
        0, -s, c, 0,
        0,  0, 0, 1,
    };
    return res;
}

inline m4x4 rotate4x4_y(f32 angle)
{
    f32 c = cosf(angle);
    f32 s = sinf(angle);
    m4x4 res = {
        c, 0, -s, 0,
        0, 1,  0, 0,
        s, 0,  c, 0,
        0, 0,  0, 1
    };
    return res;
}

inline m4x4 rotate4x4_z(f32 angle)
{
    f32 c = cosf(angle);
    f32 s = sinf(angle);
    m4x4 res = {
         c, s, 0, 0,
        -s, c, 0, 0,
         0, 0, 1, 0,
         0, 0, 0, 1,
    };
    return res;
}

inline m4x4 inverse_transpose(const m4x4& m)
{
    m4x4 c;
    c[0][0] = det3x3(m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], m[3][3]);
    c[0][1] = -det3x3(m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], m[3][3]);
    c[0][2] = det3x3(m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], m[3][3]);
    c[0][3] = -det3x3(m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]);
    c[1][0] = -det3x3(m[0][1], m[0][2], m[0][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], m[3][3]);
    c[1][1] = det3x3(m[0][0], m[0][2], m[0][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], m[3][3]);
    c[1][2] = -det3x3(m[0][0], m[0][1], m[0][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], m[3][3]);
    c[1][3] = det3x3(m[0][0], m[0][1], m[0][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]);
    c[2][0] = det3x3(m[0][1], m[0][2], m[0][3], m[1][1], m[1][2], m[1][3], m[3][1], m[3][2], m[3][3]);
    c[2][1] = -det3x3(m[0][0], m[0][2], m[0][3], m[1][0], m[1][2], m[1][3], m[3][0], m[3][2], m[3][3]);
    c[2][2] = det3x3(m[0][0], m[0][1], m[0][3], m[1][0], m[1][1], m[1][3], m[3][0], m[3][1], m[3][3]);
    c[2][3] = -det3x3(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[3][0], m[3][1], m[3][2]);
    c[3][0] = -det3x3(m[0][1], m[0][2], m[0][3], m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], m[2][3]);
    c[3][1] = det3x3(m[0][0], m[0][2], m[0][3], m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], m[2][3]);
    c[3][2] = -det3x3(m[0][0], m[0][1], m[0][3], m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], m[2][3]);
    c[3][3] = det3x3(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]);

    f32 det = m[0][0] * c[0][0] + m[0][1] * c[0][1] + m[0][2] * c[0][2] + m[0][3] * c[0][3];
    assert(det);
    c *= (1.0f/det); 
    return c;
}

inline m4x4 inverse(const m4x4& m)
{
    return transpose(inverse_transpose(m));
}

inline m4x4 perspective(f32 fov, f32 aspect, f32 near, f32 far)
{
    f32 f = 1.0f/tanf(fov * 0.5f);
    f32 c = -far/(far - near);
    f32 d = -near*(far - near);
//    c = -(far + near)/(far - near);
//    d = -2*far*near/(far-near);
    m4x4 res;
    res[0] = {f/aspect, 0, 0, 0};
    res[1] = {0, -f, 0, 0}; 
    res[2] = {0, 0, c, -1};
    res[3] = {0, 0, d, 0};
    return res;
}

inline m4x4 perspective(f32 fov, u32 width, u32 height, f32 near, f32 far)
{
    f32 aspect = (f32)height/(f32)width;
    return perspective(fov, aspect, near, far);
}

inline m4x4 view_matrix(v3 position, v3 forward, v3 up, v3 right)
{
    m4x4 m;
    m = mat4x4d(1);
    m[0].xyz = {right.x, up.x, -forward.x};
    m[1].xyz = {right.y, up.y, -forward.y};
    m[2].xyz = {right.z, up.z , -forward.z};
    m[3].xyz = {-dot(position, right), -dot(position, up), dot(position, forward)};
    return m;
}

// padding

inline m4x4 mat4(const m3x3& m)
{
    m4x4 res = {
        m[0][0], m[0][1], m[0][2], 0,
        m[1][0], m[1][1], m[1][2], 0,
        m[2][0], m[2][1], m[2][2], 0,
        0, 0, 0, 0
    };
    return res;
}

// section: quaternion

inline quat quaternion(f32 a, f32 b, f32 c, f32 d)
{
    quat res = {a, b, c, d};
    return res;
}

inline quat quaternion(f32 x, f32 y, f32 z)
{
    quat res = {1, x, y, z};
    return res;
}

inline quat qauternion_identity()
{
    quat res = {1, 0, 0, 0};
    return res;
}

inline quat euler_quaternion(f32 rad, f32 i, f32 j, f32 k)
{
    quat res;
    f32 cos = cosf(rad/2);
    f32 sin = sinf(rad/2);
    res.re = cos;
    res.im = normalized(i, j, k) * sin;
    return res;
}

inline void operator*=(quat& q, f32 s)
{
    q.data *= s; 
}

inline void operator/=(quat& q, f32 s)
{
    q.data *= (1.0f/s);
}

inline void operator+=(quat& q1, const quat& q2)
{
    q1.data += q2.data;
}

inline void operator-=(quat& q1, const quat& q2)
{
    q1.data -= q2.data;
}

inline void operator*=(quat& q1, const quat& q2)
{
    f32 s = q1.re*q2.re - dot(q1.im, q2.im);
    q1.im = q1.re*q2.im + q2.re*q1.im + cross(q1.im, q2.im);
    q1.re = s;
}

inline quat operator*(quat q1, const quat& q2)
{
    q1 *= q2;
    return q1;
}

inline quat operator*(quat q, f32 s)
{
    q *= s;
    return q;
}

inline quat operator*(f32 s, quat q)
{
    q *= s;
    return q;
}

inline quat operator/(quat q1, f32 s)
{
    q1 /= s;
    return q1;
}

inline quat operator+(quat q1, const quat& q2)
{
    q1 += q2;
    return q1;
}

inline quat operator-(quat q1, const quat& q2)
{
    q1 -= q2;
    return q1;
}

inline quat square(const quat& q)
{
    quat res;
    res.re = q.re*q.re - dot(q.im, q.im);
    res.im = 2 * q.re * q.im;
    return res;
}

inline f32 norm(const quat& q)
{
    return length(q.data);
}

inline quat conjugate(quat q)
{
    q.im = -q.im;
    return q;
}

inline quat inverse(quat q)
{
    f32 norm = length(q.data);
    if (norm)
    {
        q.im *= -1;
        q /= (norm * norm);
    }
    return q;
}

inline quat nomralize(quat q)
{
    f32 norm = length(q.data);
    if (norm)
    {
        q /= norm;
    }
    return q;
}

inline v3 rotate_vector(quat q, v3 v)
{
    f32 sin = length(q.im);
    f32 cos2 = q.a * q.a;
    f32 sin2 = sin * sin;
    return (cos2 - sin2) * v + 2 * dot(q.im, v) * q.im + 2 * q.a * cross(q.im, v);
}

inline m3x3 to_mat3x3(const quat& q)
{
    m3x3 res;
    res[0] = {1 - 2*(q.c*q.c + q.d*q.d), 2*(q.b*q.c + q.d*q.a), 2*(q.b*q.d - q.c*q.a)};
    res[1] = {2*(q.b*q.c - q.d*q.a), 1 - 2*(q.b*q.b + q.d*q.d), 2*(q.c*q.d + q.b*q.a)};
    res[2] = {2*(q.b*q.d + q.c*q.a), 2*(q.c*q.d - q.b*q.a), 1 - 2*(q.b*q.b + q.c*q.c)};
    return res;
}

inline m4x4 to_mat4x4(const quat& q)
{
    m4x4 res;
    res[0] = {1 - 2*(q.c*q.c + q.d*q.d), 2*(q.b*q.c + q.d*q.a), 2*(q.b*q.d - q.c*q.a), 0};
    res[1] = {2*(q.b*q.c - q.d*q.a), 1 - 2*(q.b*q.b + q.d*q.d), 2*(q.c*q.d + q.b*q.a), 0};
    res[2] = {2*(q.b*q.d + q.c*q.a), 2*(q.c*q.d - q.b*q.a), 1 - 2*(q.b*q.b + q.c*q.c), 0};
    res[3] = {0, 0, 0, 1};
    return res;
}

#endif // MATCHC_H
