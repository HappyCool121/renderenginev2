
//
// Created by Imari on 25/2/26.
//

#include "raytracer.h"
#include "application.h"
#include "meshProcessing.h"
#include <algorithm>
#include <cmath>
#include <cstring> // For memcpy
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <numeric>

// ===========================================================
// PART 1: BVH CONSTRUCTION (Helper Functions)
// ===========================================================

void resetAABB(Vec3 &min, Vec3 &max) {
  min = {1e30f, 1e30f, 1e30f};
  max = {-1e30f, -1e30f, -1e30f};
}

void growAABB(Vec3 &min, Vec3 &max, const Vec3 &p) {
  if (p.x < min.x)
    min.x = p.x;
  if (p.y < min.y)
    min.y = p.y;
  if (p.z < min.z)
    min.z = p.z;
  if (p.x > max.x)
    max.x = p.x;
  if (p.y > max.y)
    max.y = p.y;
  if (p.z > max.z)
    max.z = p.z;
}

void updateNodeBounds(int nodeIdx, BLAS &blas,
                      const std::vector<Vertex> &vertices,
                      const std::vector<Triangle> &triangles) {
  BLAS_node &node = blas.nodes[nodeIdx];
  resetAABB(node.minAABB, node.maxAABB);

  for (int i = 0; i < node.triangleCount; i++) {
    int triIdx = blas.triangleIndices[node.leftFirst + i];
    const Triangle &tri = triangles[triIdx];

    growAABB(node.minAABB, node.maxAABB, vertices[tri.v0].pos);
    growAABB(node.minAABB, node.maxAABB, vertices[tri.v1].pos);
    growAABB(node.minAABB, node.maxAABB, vertices[tri.v2].pos);
  }
}

// recursive BVH BLAS builder
void buildBVH(int nodeIdx, BLAS &blas, const std::vector<Vertex> &vertices,
              const std::vector<Triangle> &triangles) {
  updateNodeBounds(nodeIdx, blas, vertices, triangles);

  int count = blas.nodes[nodeIdx].triangleCount;
  int first = blas.nodes[nodeIdx].leftFirst;

  if (count <= 4)
    return; // Leaf condition

  Vec3 minAABB = blas.nodes[nodeIdx].minAABB;
  Vec3 maxAABB = blas.nodes[nodeIdx].maxAABB;

  Vec3 extent = {maxAABB.x - minAABB.x, maxAABB.y - minAABB.y,
                 maxAABB.z - minAABB.z};
  int axis = 0;
  if (extent.y > extent.x)
    axis = 1;
  if (extent.z > ((axis == 0) ? extent.x : extent.y))
    axis = 2;

  float splitPos = (axis == 0)   ? (minAABB.x + maxAABB.x) * 0.5f
                   : (axis == 1) ? (minAABB.y + maxAABB.y) * 0.5f
                                 : (minAABB.z + maxAABB.z) * 0.5f;

  int mid = first;
  auto it = std::partition(
      blas.triangleIndices.begin() + first,
      blas.triangleIndices.begin() + first + count, [&](uint32_t idx) {
        const Triangle &tri = triangles[idx];
        Vec3 v0 = vertices[tri.v0].pos;
        Vec3 v1 = vertices[tri.v1].pos;
        Vec3 v2 = vertices[tri.v2].pos;
        float centroid = (axis == 0)   ? (v0.x + v1.x + v2.x) / 3.0f
                         : (axis == 1) ? (v0.y + v1.y + v2.y) / 3.0f
                                       : (v0.z + v1.z + v2.z) / 3.0f;
        return centroid < splitPos;
      });

  mid = (int)std::distance(blas.triangleIndices.begin(), it);

  if (mid == first || mid == first + count) {
    mid = first + (count / 2);
  }

  int leftChildIdx = (int)blas.nodes.size();
  int rightChildIdx = leftChildIdx + 1;

  blas.nodes.emplace_back();
  blas.nodes.emplace_back();

  blas.nodes[nodeIdx].leftFirst = leftChildIdx;
  blas.nodes[nodeIdx].triangleCount = 0; // 0 marks this as an internal node

  blas.nodes[leftChildIdx].leftFirst = first;
  blas.nodes[leftChildIdx].triangleCount = mid - first;

  blas.nodes[rightChildIdx].leftFirst = mid;
  blas.nodes[rightChildIdx].triangleCount = (first + count) - mid;

  buildBVH(leftChildIdx, blas, vertices, triangles);
  buildBVH(rightChildIdx, blas, vertices, triangles);
}

void startBuild(BLAS &blas, const Mesh &mesh) {
  blas.nodes.clear();
  blas.triangleIndices.resize(mesh.indices.size());
  std::iota(blas.triangleIndices.begin(), blas.triangleIndices.end(), 0);

  BLAS_node root;
  root.leftFirst = 0;
  root.triangleCount = (int)mesh.indices.size();
  blas.nodes.push_back(root);

  buildBVH(0, blas, mesh.vertices, mesh.indices);
}

void updateTLASInstanceTransform(const renderObject &obj,
                                 TLASinstance &instance) {
  glm::mat4 transform = glm::mat4(1.0f);
  transform =
      glm::translate(transform, glm::vec3(obj.translation.x, obj.translation.y,
                                          obj.translation.z));
  transform =
      glm::rotate(transform, glm::radians(obj.rotation.y), glm::vec3(0, 1, 0));
  transform =
      glm::rotate(transform, glm::radians(obj.rotation.x), glm::vec3(1, 0, 0));
  transform =
      glm::rotate(transform, glm::radians(obj.rotation.z), glm::vec3(0, 0, 1));
  transform =
      glm::scale(transform, glm::vec3(obj.scale.x, obj.scale.y, obj.scale.z));

  instance.transform = transform;
  instance.inverseTransform = glm::inverse(transform);
  instance.BLASindex = obj.meshIndex;
}

void updateInstanceBounds(TLASinstance &instance, const BLAS &blas) {
  if (blas.nodes.empty()) {
    instance.transformedMinAABB = {0, 0, 0};
    instance.transformedMaxAABB = {0, 0, 0};
    return;
  }

  const BLAS_node &rootNode = blas.nodes[0];
  Vec3 min = rootNode.minAABB;
  Vec3 max = rootNode.maxAABB;

  glm::vec4 corners[8] = {glm::vec4(min.x, min.y, min.z, 1.0f),
                          glm::vec4(min.x, min.y, max.z, 1.0f),
                          glm::vec4(min.x, max.y, min.z, 1.0f),
                          glm::vec4(min.x, max.y, max.z, 1.0f),
                          glm::vec4(max.x, min.y, min.z, 1.0f),
                          glm::vec4(max.x, min.y, max.z, 1.0f),
                          glm::vec4(max.x, max.y, min.z, 1.0f),
                          glm::vec4(max.x, max.y, max.z, 1.0f)};

  glm::vec3 worldMin(1e30f);
  glm::vec3 worldMax(-1e30f);
  const glm::mat4 &transform = instance.transform;

  for (int i = 0; i < 8; i++) {
    glm::vec4 transformedCorner = transform * corners[i];
    worldMin.x = std::min(worldMin.x, transformedCorner.x);
    worldMin.y = std::min(worldMin.y, transformedCorner.y);
    worldMin.z = std::min(worldMin.z, transformedCorner.z);
    worldMax.x = std::max(worldMax.x, transformedCorner.x);
    worldMax.y = std::max(worldMax.y, transformedCorner.y);
    worldMax.z = std::max(worldMax.z, transformedCorner.z);
  }

  instance.transformedMinAABB = {worldMin.x, worldMin.y, worldMin.z};
  instance.transformedMaxAABB = {worldMax.x, worldMax.y, worldMax.z};
}

void updateTLASNodeBounds(int nodeIdx, TLAS &tlas,
                          const std::vector<TLASinstance> &instances) {
  TLASNode &node = tlas.nodes[nodeIdx];
  resetAABB(node.minAABB, node.maxAABB);

  for (int i = 0; i < node.instanceCount; i++) {
    uint32_t instanceIdx = tlas.TLASindices[node.firstInstanceIndex + i];
    const TLASinstance &inst = instances[instanceIdx];
    growAABB(node.minAABB, node.maxAABB, inst.transformedMinAABB);
    growAABB(node.minAABB, node.maxAABB, inst.transformedMaxAABB);
  }
}

void buildTLAS(int nodeIdx, TLAS &tlas,
               const std::vector<TLASinstance> &instances) {
  updateTLASNodeBounds(nodeIdx, tlas, instances);

  int count = tlas.nodes[nodeIdx].instanceCount;
  int first = tlas.nodes[nodeIdx].firstInstanceIndex;

  if (count <= 2)
    return;

  Vec3 minAABB = tlas.nodes[nodeIdx].minAABB;
  Vec3 maxAABB = tlas.nodes[nodeIdx].maxAABB;
  Vec3 extent = {maxAABB.x - minAABB.x, maxAABB.y - minAABB.y,
                 maxAABB.z - minAABB.z};
  int axis = 0;
  if (extent.y > extent.x)
    axis = 1;
  if (extent.z > ((axis == 0) ? extent.x : extent.y))
    axis = 2;

  float splitPos = (axis == 0)   ? (minAABB.x + maxAABB.x) * 0.5f
                   : (axis == 1) ? (minAABB.y + maxAABB.y) * 0.5f
                                 : (minAABB.z + maxAABB.z) * 0.5f;

  int mid = first;
  auto it = std::partition(
      tlas.TLASindices.begin() + first,
      tlas.TLASindices.begin() + first + count, [&](uint32_t idx) {
        const TLASinstance &inst = instances[idx];
        float centroid =
            (axis == 0)
                ? (inst.transformedMinAABB.x + inst.transformedMaxAABB.x) * 0.5f
            : (axis == 1)
                ? (inst.transformedMinAABB.y + inst.transformedMaxAABB.y) * 0.5f
                : (inst.transformedMinAABB.z + inst.transformedMaxAABB.z) *
                      0.5f;
        return centroid < splitPos;
      });

  mid = (int)std::distance(tlas.TLASindices.begin(), it);
  if (mid == first || mid == first + count)
    mid = first + (count / 2);

  int leftChildIdx = (int)tlas.nodes.size();
  int rightChildIdx = leftChildIdx + 1;

  tlas.nodes.emplace_back();
  tlas.nodes.emplace_back();

  tlas.nodes[nodeIdx].leftIndex = leftChildIdx;
  tlas.nodes[nodeIdx].instanceCount = 0; // Mark as internal

  tlas.nodes[leftChildIdx].firstInstanceIndex = first;
  tlas.nodes[leftChildIdx].instanceCount = mid - first;
  tlas.nodes[leftChildIdx].nodeIndex = leftChildIdx;

  tlas.nodes[rightChildIdx].firstInstanceIndex = mid;
  tlas.nodes[rightChildIdx].instanceCount = (first + count) - mid;
  tlas.nodes[rightChildIdx].nodeIndex = rightChildIdx;

  buildTLAS(leftChildIdx, tlas, instances);
  buildTLAS(rightChildIdx, tlas, instances);
}

void startTLASBuild() {
  if (TLASinstanceList.empty())
    return;

  mainTLAS.nodes.clear();
  mainTLAS.TLASindices.resize(TLASinstanceList.size());
  std::iota(mainTLAS.TLASindices.begin(), mainTLAS.TLASindices.end(), 0);

  TLASNode root;
  resetAABB(root.minAABB, root.maxAABB);
  root.firstInstanceIndex = 0;
  root.instanceCount = (uint32_t)TLASinstanceList.size();
  root.nodeIndex = 0;
  root.leftIndex = 0;
  mainTLAS.nodes.push_back(root);

  buildTLAS(0, mainTLAS, TLASinstanceList);
}

// ===========================================================
// PART 2: RAY TRAVERSAL & INTERSECTION
// ===========================================================

// Optimized Slab Method
bool intersectAABB(const pixelCoordinates &ray, const Vec3 &minAABB,
                   const Vec3 &maxAABB, float &tEntry, float tMax) {
  // Avoid division by zero
  float invDirX =
      1.0f /
      (std::abs(ray.directionVector.x) < 1e-6f ? 1e-6f : ray.directionVector.x);
  float invDirY =
      1.0f /
      (std::abs(ray.directionVector.y) < 1e-6f ? 1e-6f : ray.directionVector.y);
  float invDirZ =
      1.0f /
      (std::abs(ray.directionVector.z) < 1e-6f ? 1e-6f : ray.directionVector.z);

  float t0x = (minAABB.x - ray.origin.x) * invDirX;
  float t1x = (maxAABB.x - ray.origin.x) * invDirX;
  if (invDirX < 0.0f)
    std::swap(t0x, t1x);

  float t0y = (minAABB.y - ray.origin.y) * invDirY;
  float t1y = (maxAABB.y - ray.origin.y) * invDirY;
  if (invDirY < 0.0f)
    std::swap(t0y, t1y);

  float t0z = (minAABB.z - ray.origin.z) * invDirZ;
  float t1z = (maxAABB.z - ray.origin.z) * invDirZ;
  if (invDirZ < 0.0f)
    std::swap(t0z, t1z);

  float tMinVal = std::max({t0x, t0y, t0z});
  float tMaxVal = std::min({t1x, t1y, t1z});

  if (tMaxVal < tMinVal || tMaxVal < 0 || tMinVal > tMax) {
    return false;
  }

  tEntry = tMinVal;
  return true;
}

// Transform Ray to Local Space
pixelCoordinates transformRay(const pixelCoordinates &worldRay,
                              const glm::mat4 &invTransform) {
  pixelCoordinates localRay;
  glm::vec4 localOrigin =
      invTransform *
      glm::vec4(worldRay.origin.x, worldRay.origin.y, worldRay.origin.z, 1.0f);
  localRay.origin = {localOrigin.x, localOrigin.y, localOrigin.z};

  // Note: W = 0 for direction vectors
  glm::vec4 localDir =
      invTransform * glm::vec4(worldRay.directionVector.x,
                               worldRay.directionVector.y,
                               worldRay.directionVector.z, 0.0f);
  localRay.directionVector = {localDir.x, localDir.y, localDir.z};
  return localRay;
}

// Moeller-Trumbore intersection
// UPDATED: Now returns u and v by reference for barycentric interpolation
float intersectTriangle(const pixelCoordinates &localRay, const Triangle &tri,
                        const std::vector<Vertex> &vertices, float &u,
                        float &v) {
  const Vec3 &v0 = vertices[tri.v0].pos;
  const Vec3 &v1 = vertices[tri.v1].pos;
  const Vec3 &v2 = vertices[tri.v2].pos;

  Vec3 edge1 = {v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
  Vec3 edge2 = {v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};

  Vec3 h = cross(localRay.directionVector, edge2);
  float a = dot(edge1, h);

  if (a > -1e-6f && a < 1e-6f)
    return 1e30f; // Parallel

  float f = 1.0f / a;
  Vec3 s = {localRay.origin.x - v0.x, localRay.origin.y - v0.y,
            localRay.origin.z - v0.z};
  u = f * dot(s, h);

  if (u < 0.0f || u > 1.0f)
    return 1e30f;

  Vec3 q = cross(s, edge1);
  v = f * dot(localRay.directionVector, q);

  if (v < 0.0f || u + v > 1.0f)
    return 1e30f;

  float t = f * dot(edge2, q);

  if (t > 1e-4f)
    return t;
  return 1e30f;
}

// Traverse BVH
void traverseBVH(TLAS &tlas, std::vector<BLAS> &blasList,
                 const std::vector<Mesh> &meshes, pixelCoordinates &ray,
                 hitRecord &rec) {
  rec.t = 1e30f;
  rec.hit = false;

  uint32_t tlasStack[64];
  uint32_t tlasStackPtr = 0;
  tlasStack[tlasStackPtr++] = 0;

  while (tlasStackPtr > 0) {
    TLASNode &node = tlas.nodes[tlasStack[--tlasStackPtr]];
    float tBoxEntry;

    if (!intersectAABB(ray, node.minAABB, node.maxAABB, tBoxEntry, rec.t))
      continue;

    if (node.instanceCount > 0) {
      // It's a Leaf TLAS node (contains instances)
      for (uint32_t i = 0; i < node.instanceCount; i++) {
        uint32_t instIdx = tlas.TLASindices[node.firstInstanceIndex + i];
        TLASinstance &instance = TLASinstanceList[instIdx];

        pixelCoordinates localRay =
            transformRay(ray, instance.inverseTransform);
        BLAS &currentBlas = blasList[instance.BLASindex];
        const Mesh &currentMesh = meshes[instance.BLASindex];

        uint32_t blasStack[64];
        uint32_t blasStackPtr = 0;
        blasStack[blasStackPtr++] = 0;

        while (blasStackPtr > 0) {
          BLAS_node &blasNode = currentBlas.nodes[blasStack[--blasStackPtr]];

          if (!intersectAABB(localRay, blasNode.minAABB, blasNode.maxAABB,
                             tBoxEntry, rec.t))
            continue;

          if (blasNode.triangleCount > 0) {
            // Leaf BLAS node (contains triangles)
            for (int t = 0; t < blasNode.triangleCount; t++) {
              uint32_t triIdx =
                  currentBlas.triangleIndices[blasNode.leftFirst + t];
              const Triangle &tri = currentMesh.indices[triIdx];

              float u, v;
              float t_hit =
                  intersectTriangle(localRay, tri, currentMesh.vertices, u, v);

              if (t_hit < rec.t) {
                rec.t = t_hit;
                rec.hit = true;
                rec.instanceID = instIdx;
                rec.triangleID = triIdx;

                // Store barycentric coordinates for later shading
                rec.uv.x = u;
                rec.uv.y = v;

                // Note: We do NOT compute normals here.
                // We delay that until we know for sure which triangle is the
                // CLOSEST.
              }
            }
          } else {
            // Internal BLAS node
            blasStack[blasStackPtr++] = blasNode.leftFirst + 1;
            blasStack[blasStackPtr++] = blasNode.leftFirst;
          }
        }
      }
    } else {
      // Internal TLAS node
      tlasStack[tlasStackPtr++] = node.leftIndex + 1;
      tlasStack[tlasStackPtr++] = node.leftIndex;
    }
  }
}

bool isShadowed(TLAS &tlas, std::vector<BLAS> &blasList,
                const std::vector<Mesh> &meshes, Vec3 origin, Vec3 direction,
                float maxDist) {
  pixelCoordinates ray;
  ray.origin = origin;
  ray.directionVector = direction;

  uint32_t tlasStack[64];
  uint32_t tlasStackPtr = 0;
  tlasStack[tlasStackPtr++] = 0;

  while (tlasStackPtr > 0) {
    TLASNode &node = tlas.nodes[tlasStack[--tlasStackPtr]];
    float tBoxEntry;

    if (!intersectAABB(ray, node.minAABB, node.maxAABB, tBoxEntry, maxDist))
      continue;

    if (node.instanceCount > 0) {
      for (uint32_t i = 0; i < node.instanceCount; i++) {
        uint32_t instIdx = tlas.TLASindices[node.firstInstanceIndex + i];
        TLASinstance &instance = TLASinstanceList[instIdx];

        pixelCoordinates localRay =
            transformRay(ray, instance.inverseTransform);
        BLAS &currentBlas = blasList[instance.BLASindex];
        const Mesh &currentMesh = meshes[instance.BLASindex];

        uint32_t blasStack[64];
        uint32_t blasStackPtr = 0;
        blasStack[blasStackPtr++] = 0;

        while (blasStackPtr > 0) {
          BLAS_node &blasNode = currentBlas.nodes[blasStack[--blasStackPtr]];

          if (!intersectAABB(localRay, blasNode.minAABB, blasNode.maxAABB,
                             tBoxEntry, maxDist))
            continue;

          if (blasNode.triangleCount > 0) {
            for (int t = 0; t < blasNode.triangleCount; t++) {
              uint32_t triIdx =
                  currentBlas.triangleIndices[blasNode.leftFirst + t];
              const Triangle &tri = currentMesh.indices[triIdx];

              float u, v;
              float t_hit =
                  intersectTriangle(localRay, tri, currentMesh.vertices, u, v);

              if (t_hit < maxDist) {
                return true; // Shadow found
              }
            }
          } else {
            blasStack[blasStackPtr++] = blasNode.leftFirst + 1;
            blasStack[blasStackPtr++] = blasNode.leftFirst;
          }
        }
      }
    } else {
      tlasStack[tlasStackPtr++] = node.leftIndex + 1;
      tlasStack[tlasStackPtr++] = node.leftIndex;
    }
  }
  return false;
}

// -----------------------------------------------------------
// 3. MAIN RAYTRACER PIPELINE
// -----------------------------------------------------------

// Helper to clamp colors
uint32_t packColor(Vec3 color) {
  int r = std::min(255, std::max(0, (int)(color.x * 255)));
  int g = std::min(255, std::max(0, (int)(color.y * 255)));
  int b = std::min(255, std::max(0, (int)(color.z * 255)));
  return (0xFF << 24) | (r << 16) | (g << 8) | b;
}

// THE CORE RECURSIVE FUNCTION
Vec3 castRay(pixelCoordinates &ray, int depth) {
  // 1. Base Case: Stop recursion to prevent infinite loops
  if (depth <= 0) {
    return {0.0f, 0.0f, 0.0f};
  }

  // 2. Traversal
  hitRecord rec;
  traverseBVH(mainTLAS, BLASlist, meshList, ray, rec);

  // 3. Miss Shader (Background Color)
  if (!rec.hit) {
    return {0.01f, 0.01f, 0.01f}; // Dark Grey Background
  }

  // --- DATA RETRIEVAL ---
  TLASinstance &instance = TLASinstanceList[rec.instanceID];
  const Mesh &mesh = meshList[instance.BLASindex];
  const Triangle &tri = mesh.indices[rec.triangleID];

  // Vertices
  const Vertex &v0 = mesh.vertices[tri.v0];
  const Vertex &v1 = mesh.vertices[tri.v1];
  const Vertex &v2 = mesh.vertices[tri.v2];

  // Barycentrics
  float u = rec.uv.x;
  float v = rec.uv.y;
  float w = 1.0f - u - v;

  // --- NORMAL CALCULATION ---
  Vec3 worldNormal;

  if (globalShadingMode == FLAT_MODE) {
    // 1. FLAT SHADING PIPELINE: Uses the geometric face normal
    // No barycentric interpolation is required for the normal itself
    Vec3 faceNormal = calculateFaceNormal(v0.pos, v1.pos, v2.pos);

    // Transform Normal to World Space
    glm::mat4 invTrans = glm::transpose(instance.inverseTransform);
    glm::vec4 worldN =
        invTrans * glm::vec4(faceNormal.x, faceNormal.y, faceNormal.z, 0.0f);
    worldNormal = norm({worldN.x, worldN.y, worldN.z});
  } else {
    // 2. PHONG SHADING PIPELINE: Interpolates normals per vertex
    Vec3 smoothNormal;
    smoothNormal.x = w * v0.normal.x + u * v1.normal.x + v * v2.normal.x;
    smoothNormal.y = w * v0.normal.y + u * v1.normal.y + v * v2.normal.y;
    smoothNormal.z = w * v0.normal.z + u * v1.normal.z + v * v2.normal.z;

    // Transform Normal to World Space
    glm::mat4 invTrans = glm::transpose(instance.inverseTransform);
    glm::vec4 worldN = invTrans * glm::vec4(smoothNormal.x, smoothNormal.y,
                                            smoothNormal.z, 0.0f);
    worldNormal = norm({worldN.x, worldN.y, worldN.z});
  }

  // --- WORLD POSITION ---
  Vec3 worldPos = {ray.origin.x + ray.directionVector.x * rec.t,
                   ray.origin.y + ray.directionVector.y * rec.t,
                   ray.origin.z + ray.directionVector.z * rec.t};

  // --- TEXTURING ---
  // Interpolate UVs
  float texU = w * v0.uv.x + u * v1.uv.x + v * v2.uv.x;
  float texV = w * v0.uv.y + u * v1.uv.y + v * v2.uv.y;

  Vec3 baseColor = {1.0f, 1.0f, 1.0f}; // Default White

  // Texture Lookup
  int objID = v0.objectID;
  int texIndex = (objID >= 0 && objID < objectToTextureIndex.size())
                     ? objectToTextureIndex[objID]
                     : 0;

  if (texIndex >= 0 && texIndex < globalTextures.size()) {
    const Texture &tex = globalTextures[texIndex];
    if (tex.loaded) {
      int tx = (int)(texU * tex.width) % tex.width;
      int ty = (int)(texV * tex.height) % tex.height;
      if (tx < 0)
        tx += tex.width;
      if (ty < 0)
        ty += tex.height;
      uint32_t pix = tex.pixels[ty * tex.width + tx];

      // Texture is usually sRGB, convert to Linear for correct lighting math
      float r = ((pix >> 16) & 0xFF) / 255.0f;
      float g = ((pix >> 8) & 0xFF) / 255.0f;
      float b = (pix & 0xFF) / 255.0f;

      // Simple approximate sRGB -> Linear
      baseColor = {std::pow(r, 2.2f), std::pow(g, 2.2f), std::pow(b, 2.2f)};
    }
  }

  // --- LIGHTING (Blinn-Phong) ---
  Vec3 lightVec = sub(lightPos, worldPos);
  float distToLight = len(lightVec);
  Vec3 lightDir = norm(lightVec);

  Vec3 viewDir = norm(scale(ray.directionVector, -1.0f));
  Vec3 halfway = norm(add(lightDir, viewDir));

  // Ambient
  float ambientStrength = 0.1f;
  Vec3 ambient = scale(baseColor, ambientStrength);

  // Shadow Check
  bool shadowed = false;
  float NdotL = dot(worldNormal, lightDir);

  if (NdotL > 0.0f) {
    // Offset to prevent shadow acne
    Vec3 shadowOrigin = add(worldPos, scale(worldNormal, 0.001f));
    shadowed = isShadowed(mainTLAS, BLASlist, meshList, shadowOrigin, lightDir,
                          distToLight);
  } else {
    shadowed = true; // Face points away from light
  }

  // Diffuse
  float diff = shadowed ? 0.0f : std::max(0.0f, NdotL);
  Vec3 diffuse = scale(baseColor, diff);

  // Specular
  float specStrength = 0.5f;
  float spec = 0.0f;
  if (!shadowed && diff > 0.0f) {
    float NdotH = std::max(0.0f, dot(worldNormal, halfway));
    spec = std::pow(NdotH, 32.0f);
  }
  Vec3 specular = {spec * specStrength, spec * specStrength,
                   spec * specStrength};

  Vec3 localColor = add(add(ambient, diffuse), specular);

  // --- REFLECTION ---
  // Hardcoded reflectivity for now (0.0 = Matte, 1.0 = Mirror)
  // Ideally store this in a Material struct
  float reflectivity = 0.5f;

  if (reflectivity > 0.0f) {
    // R = I - 2(N.I)N
    float dotNI = dot(worldNormal, ray.directionVector);
    Vec3 reflectionDir =
        sub(ray.directionVector, scale(worldNormal, 2.0f * dotNI));
    reflectionDir = norm(reflectionDir);

    // Offset origin to prevent acne
    Vec3 offsetOrigin = add(worldPos, scale(worldNormal, 0.0000001f));

    pixelCoordinates reflectedRay;
    reflectedRay.origin = offsetOrigin;
    reflectedRay.directionVector = reflectionDir;

    // Recursive call
    Vec3 reflectedColor = castRay(reflectedRay, depth - 1);

    // E. Mix Local and Reflected Color
    // Formula: (1 - k) * Local + k * Reflected
    Vec3 localPart = scale(localColor, 1.0f - reflectivity);
    Vec3 reflPart = scale(reflectedColor, reflectivity);
    Vec3 finalColor = add(localPart, reflPart);

    return finalColor;
  }

  return localColor;
}
void raytracer() {
  std::cout << "Starting Raytrace..." << std::endl;
  Uint32 renderStart = SDL_GetTicks();

  Vec3 cameraOrigin = {0, 0, 0};
  float aspectRatio = (float)WIDTH / (float)HEIGHT;
  float scale = std::tan(cameraFOV * 0.5f * PI / 180.0f);

  if (pixels.size() != WIDTH * HEIGHT)
    pixels.resize(WIDTH * HEIGHT);

  const int MAX_DEPTH = 3; // Max bounces

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {

      // 1. Generate Primary Ray
      float px = (2.0f * (x + 0.5f) / WIDTH - 1.0f) * scale * aspectRatio;
      float py = (1.0f - 2.0f * (y + 0.5f) / HEIGHT) * scale;

      pixelCoordinates ray;
      ray.origin = cameraOrigin;
      ray.directionVector = norm({px, py, 1.0f});

      // 2. Cast Ray (Returns Linear Color)
      Vec3 linearColor = castRay(ray, MAX_DEPTH);

      // 3. Gamma Correction (Fixes Dark Artifacts)
      // Monitors are Gamma 2.2. We need to raise color to 1/2.2 (~0.4545)
      Vec3 finalColor;
      finalColor.x = std::pow(linearColor.x, 0.4545f);
      finalColor.y = std::pow(linearColor.y, 0.4545f);
      finalColor.z = std::pow(linearColor.z, 0.4545f);

      // 4. Pack Color
      int r = std::min(255, std::max(0, (int)(finalColor.x * 255.0f)));
      int g = std::min(255, std::max(0, (int)(finalColor.y * 255.0f)));
      int b = std::min(255, std::max(0, (int)(finalColor.z * 255.0f)));

      pixels[y * WIDTH + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
    }
  }

  void *mPixels;
  int mPitch;
  SDL_LockTexture(app.texture, nullptr, &mPixels, &mPitch);
  std::memcpy(mPixels, pixels.data(), pixels.size() * sizeof(uint32_t));
  SDL_UnlockTexture(app.texture);

  std::cout << "Raytrace Complete in " << (SDL_GetTicks() - renderStart) << "ms"
            << std::endl;
}