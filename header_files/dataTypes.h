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

// important notes for BLAS/TLAS
// 1. each triangle is indexed by an integer
// 2. the triangle indexes are arranged in triangleIndices, such that triangles
// in the same node are grouped together
//      (so that they can be accessed with just a first index + triangle count)
// 3. when we rearrange or swap the assigment of triangles in the nodes, we
// change the position of indices in the
//      triangleIndices array
//      *** the actual triangles are not stored in the BLAS due to storage
//      concerns thus we need a separate array for the original indices. we will
//      be storing their index (in the array)

// -------------------------------------------
// Raytracing objects (BVH)
// -------------------------------------------

// BLAS and BLAS nodes

// single node for a BLAS. can either be a leaf node or pointer node
struct BLAS_node {
  // each node points to either a) another node or b) first triangle index (of
  // the leaf)
  Vec3 minAABB;
  Vec3 maxAABB;
  // If triangleCount > 0, it's a leaf.
  // If triangleCount == 0, it's an internal (pointer) node.
  int leftFirst; // Index of left child (if internal) OR first triangle index
                 // (if leaf)
  int triangleCount;
};

// for every primitive mesh in world space
struct BLAS {
  // contains all the nodes of the BVH for the mesh
  // contained in a BLAS list, indexed by an integer
  // TLAS instances will point to a specific BLAS in the BLAS list
  std::vector<BLAS_node> nodes;
  std::vector<uint32_t>
      triangleIndices; // indexes all triangles for mesh primitive with integer
};

// TLAS and TLAS instances

// for all ojects to be rendered in the world space
struct TLASinstance {
  // objects in the TLAS, contains all objects (including instances to be
  // rendered)
  uint32_t BLASindex;  // from BLAS list
  glm::mat4 transform; // matrix transform from original BLAS
  glm::mat4
      inverseTransform; // inv matrix transform for ray intersection calculation
  Vec3 transformedMinAABB{};
  Vec3 transformedMaxAABB{};
};

// single node in TLAS, either pointing to another node or a TLAS instance
struct TLASNode {
  Vec3 minAABB;
  Vec3 maxAABB;
  uint32_t nodeIndex;          // current node index
  uint32_t instanceCount;      // If > 0, it's a leaf
  uint32_t firstInstanceIndex; // either points to another node or a TLAS
                               // instance index
  uint32_t leftIndex;          // if count == 0, points to left child node
};

// all object instances for world space
struct TLAS {
  std::vector<TLASNode> nodes;
  std::vector<uint32_t> TLASindices;
};

// hit record when a ray intersects a TLAS/BLAS node:
struct hitRecord {
  float t = std::numeric_limits<float>::infinity();

  // properties to compute shading logic
  Vec3 worldPos{};
  Vec3 worldNormal{};
  Vec2 uv{};

  // for TLAS/BLAS traversal
  uint32_t instanceID{}; // TLAS instance index
  uint32_t triangleID{}; // triangle index for each mesh (integer!)
  bool hit = false;
};

// -------------------------------------------
// Object properties
// -------------------------------------------

struct renderObject {
  uint32_t meshIndex; // from std::vector<Mesh> meshList;
  Vec3 rotation;
  Vec3 translation;
  Vec3 scale;

  uint32_t textureID;     // which texture the object points to
  uint32_t materialIndex; // from std::vector<Texture> globalTextures;
};

struct Material {
  std::string name; // Good for debugging (e.g., "RedPlastic", "Mirror")

  // -------------------------------------------------------
  // 1. Surface Appearance (Color & Texture)
  // -------------------------------------------------------
  Vec3 albedo = {1.0f, 1.0f, 1.0f}; // Base color
  int albedoTextureID =
      -1; // Index in globalTextures (-1 = use albedo color only)

  // UV controls (allows tiling textures on a wall)
  Vec2 uvScale = {1.0f, 1.0f};

  // -------------------------------------------------------
  // 2. Phong Shading Coefficients (The "Feel" of the object)
  // -------------------------------------------------------
  float kAmbient = 0.1f;  // How much ambient light it reflects
  float kDiffuse = 0.7f;  // How much diffuse light it scatters
  float kSpecular = 0.5f; // How bright the shiny spot is
  float shininess =
      32.0f; // Specular exponent (High = small/sharp highlight, Low = big/dull)

  // -------------------------------------------------------
  // 3. Geometry / Normals
  // -------------------------------------------------------
  bool smoothShading =
      true; // TRUE = Interpolate normals, FALSE = Flat shading (low poly look)
  int normalMapID = -1; // (Optional) For bump mapping later

  // -------------------------------------------------------
  // 4. Optical Properties (Reflection & Refraction)
  // -------------------------------------------------------
  float reflectivity = 0.0f; // 0.0 (Matte) to 1.0 (Mirror)

  // Transparency / Refraction (For Glass/Water)
  float transparency = 0.0f; // 0.0 (Opaque) to 1.0 (Fully clear)
  float ior =
      1.0f; // Index of Refraction (1.0 = Air, 1.33 = Water, 1.52 = Glass)

  // -------------------------------------------------------
  // 5. Emission (For lights/glowing objects)
  // -------------------------------------------------------
  Vec3 emittance = {0.0f, 0.0f,
                    0.0f}; // The color/intensity of light this object emits
};

struct Texture {
  std::vector<uint32_t> pixels;
  int width = 0;
  int height = 0;
  bool loaded = false;
  std::string name; // For debugging
};

enum MappingType { SPHERICAL, CYLINDRICAL, CUBIC, CUSTOM_UV };
enum ShadingType { TEXTURE, VERTEX_COLOR, FLAT, GOURAUD, PHONG };
enum ShapeType { CUBE, UV_SPHERE, ICOSPHERE, CUSTOM };
enum SubdivMode { Linear, Loop, catmullClark };
enum ShadingMode { PHONG_MODE, FLAT_MODE };

const float PI = 3.14159265359f;

#endif // RENDERENGINEV2_DATATYPES_H