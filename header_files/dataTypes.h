//
// Created by Ahmad Zuhri on 11/2/26.
//

#ifndef RENDERENGINEV2_DATATYPES_H
#define RENDERENGINEV2_DATATYPES_H

#include "tiny_gltf.h"
#include "vectorMath.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

enum MappingType { SPHERICAL, CYLINDRICAL, CUBIC, CUSTOM_UV };
enum ShadingType { TEXTURE, VERTEX_COLOR, FLAT, GOURAUD, PHONG };
enum ShapeType { CUBE, UV_SPHERE, ICOSPHERE, CUSTOM };
enum SubdivMode { Linear, Loop, catmullClark };
enum ShadingMode { PHONG_MODE, FLAT_MODE };

const float PI = 3.14159265359f;

// -------------------------------------------
// Basic mathematical structures
// -------------------------------------------

struct Vertex {
  Vec3 pos;
  Vec3 color;
  Vec2 uv;
  Vec3 normal;
  int objectID; // Crucial: Identifies which sub-mesh this vertex belongs to
};

struct Triangle {
  int v0, v1, v2;
}; // contains vertices in triangle

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<Triangle> indices;
};

struct PolygonFace {
  std::vector<int> indices;
};

struct pixelCoordinates { // INCLUDES THE RAY DATA
  Vec3 pos;
  Vec2 pixelIndex;
  Vec3 directionVector;
  Vec3 origin;
};

// -------------------------------------------
// Raytracing objects (BVH)
// -------------------------------------------

struct BLAS_node {
  Vec3 minAABB;
  Vec3 maxAABB;
  int leftFirst;
  int triangleCount;
};

struct BLAS {
  std::vector<BLAS_node> nodes;
  std::vector<uint32_t> triangleIndices;
};

struct TLASinstance {
  uint32_t BLASindex;
  glm::mat4 transform;
  glm::mat4 inverseTransform;
  Vec3 transformedMinAABB{};
  Vec3 transformedMaxAABB{};
  uint32_t materialID; // Pointer to globalMaterials
};

struct TLASNode {
  Vec3 minAABB;
  Vec3 maxAABB;
  uint32_t nodeIndex;
  uint32_t instanceCount;
  uint32_t firstInstanceIndex;
  uint32_t leftIndex;
};

struct TLAS {
  std::vector<TLASNode> nodes;
  std::vector<uint32_t> TLASindices;
};

struct hitRecord {
  float t = std::numeric_limits<float>::infinity();
  Vec3 worldPos{};
  Vec3 worldNormal{};
  Vec2 uv{};
  uint32_t instanceID{};
  uint32_t triangleID{};
  bool hit = false;
};

// -------------------------------------------
// Object properties
// -------------------------------------------

struct renderObject {
  uint32_t meshIndex;
  Vec3 rotation;
  Vec3 translation;
  Vec3 scale;

  uint32_t textureID;
  uint32_t materialID;
  uint32_t materialIndex;
};

struct Material { // used for shading techniques (doesn't contain any image
                  // texture data!)
  std::string name;

  // Shading Parameters
  float kAmbient = 0.1f;
  float kDiffuse = 0.7f;
  float kSpecular = 0.5f;
  float shininess = 32.0f;
  float reflectivity = 0.0f;
  bool receivesShadows = true;
  ShadingMode shadingMode = PHONG_MODE;

  // Appearance
  Vec3 albedo = {1.0f, 1.0f, 1.0f};
  int albedoTextureID = -1;
  Vec2 uvScale = {1.0f, 1.0f};

  // Optical Properties
  float transmission = 0.0f;
  float ior = 1.0f;

  // Emission
  Vec3 emittance = {0.0f, 0.0f, 0.0f};
};

struct Texture {
  std::vector<uint32_t> pixels;
  int width = 0;
  int height = 0;
  bool loaded = false;
  std::string name;
};

#endif // RENDERENGINEV2_DATATYPES_H