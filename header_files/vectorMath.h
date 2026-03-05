//
// Created by Imari on 11/2/26.
//


#ifndef RENDERENGINEV2_VECTORMATH_H
#define RENDERENGINEV2_VECTORMATH_H

#include <cmath>

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };
struct Vec4 { float x, y, z, w;};
struct mat4 {
    // Column-major storage: columns[0] is the first column
    Vec4 columns[4];

    // Default constructor: Identity Matrix
    mat4() {
        for(int i = 0; i < 4; ++i)
            for(int j = 0; j < 4; ++j)
                (*this)[i][j] = (i == j) ? 1.0f : 0.0f;
    }

    // Access operators
    float* operator[](int index) {
        return &(columns[index].x);
    }
};


// vector math operations
inline Vec3 sub(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vec3 add(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vec3 cross(Vec3 a, Vec3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float len(Vec3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
inline Vec3 norm(Vec3 v) { float l = len(v); return (l == 0) ? Vec3{0,0,0} : Vec3{v.x/l, v.y/l, v.z/l}; }
inline Vec3 scale(Vec3 v, float s) { return { v.x * s, v.y * s, v.z * s }; }

inline float edgeFunction(const Vec2& a, const Vec2& b, const Vec2& p) {
    return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
}

#endif //RENDERENGINEV2_VECTORMATH_H
