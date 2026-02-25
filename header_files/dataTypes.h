//
// Created by Ahmad Zuhri on 11/2/26.
//

#ifndef RENDERENGINEV2_DATATYPES_H
#define RENDERENGINEV2_DATATYPES_H

#include "vectorMath.h"
#include <vector>

struct Vertex {
    Vec3 pos;
    Vec3 color;
    Vec2 uv;
    Vec3 normal;
    int objectID;   // Crucial: Identifies which sub-mesh this vertex belongs to
};

struct Triangle { int v0, v1, v2; };

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<Triangle> indices;
};

struct PolygonFace {
    std::vector<int> indices;
};


struct Texture {
    std::vector<uint32_t> pixels;
    int width = 0;
    int height = 0;
    bool loaded = false;
    std::string name; // For debugging
};

enum MappingType { SPHERICAL, CYLINDRICAL, CUBIC, CUSTOM_UV};
enum ShadingType { TEXTURE, VERTEX_COLOR, FLAT, GOURAUD, PHONG};
enum ShapeType { CUBE, UV_SPHERE, ICOSPHERE, CUSTOM};
enum SubdivMode {Linear, Loop, catmullClark};

const float PI = 3.14159265359f;

#endif //RENDERENGINEV2_DATATYPES_H