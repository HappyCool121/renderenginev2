//
// Created by Imari on 25/2/26.
//

#ifndef RENDERENGINEV2_RAYTRACER_H
#define RENDERENGINEV2_RAYTRACER_H

#include "dataTypes.h"
#include <glm/glm.hpp>
#include <vector>

// -----------------------------------------------------------
// Helper Math & Bounds
// -----------------------------------------------------------
void resetAABB(Vec3 &min, Vec3 &max);
void growAABB(Vec3 &min, Vec3 &max, const Vec3 &p);

inline std::vector<uint32_t> pixels;
inline std::vector<float> zBuffer;

// -----------------------------------------------------------
// BLAS Construction (Mesh Primitives)
// -----------------------------------------------------------

// Main entry point to build a BLAS for a specific mesh
void startBuild(BLAS &blas, const Mesh &mesh);

// Internal builder functions (exposed here if needed by other modules,
// otherwise could be static in cpp)
void updateNodeBounds(int nodeIdx, BLAS &blas,
                      const std::vector<Vertex> &vertices,
                      const std::vector<Triangle> &triangles);
void buildBVH(int nodeIdx, BLAS &blas, const std::vector<Vertex> &vertices,
              const std::vector<Triangle> &triangles);

// -----------------------------------------------------------
// TLAS Construction (Scene Instances)
// -----------------------------------------------------------

// Updates the transformation matrix (and inverse) for an instance based on
// rotation/translation/scale
void updateTLASInstanceTransform(const renderObject &obj,
                                 TLASinstance &instance);

// Updates the AABB of the instance based on the underlying BLAS and Transform
// matrix
void updateInstanceBounds(TLASinstance &instance, const BLAS &blas);

// Internal TLAS builder functions
void updateTLASNodeBounds(int nodeIdx, TLAS &tlas,
                          const std::vector<TLASinstance> &instances);
void buildTLAS(int nodeIdx, TLAS &tlas,
               const std::vector<TLASinstance> &instances);

// Main entry point to build the Top Level Acceleration Structure
void startTLASBuild();

// -----------------------------------------------------------
// Intersection Tests
// -----------------------------------------------------------

// Returns true if ray hits AABB within [0, tMax].
// tEntry is the distance to the box. tMax is the current closest hit distance
// (for culling).
bool intersectAABB(const pixelCoordinates &ray, const Vec3 &minAABB,
                   const Vec3 &maxAABB, float &tEntry, float tMax);

// Transform a ray from World Space to Local Space using an inverse matrix
// Note: Direction vector is NOT normalized to preserve distance scaling.
pixelCoordinates transformRay(const pixelCoordinates &worldRay,
                              const glm::mat4 &invTransform);

// Returns 't' distance to triangle intersection, or infinity if miss
float intersectTriangle(const pixelCoordinates &localRay, const Triangle &tri,
                        const std::vector<Vertex> &vertices);

// -----------------------------------------------------------
// Traversal
// -----------------------------------------------------------

// Traverses TLAS -> Instance -> BLAS -> Triangle
// Fills 'rec' with intersection data (position, normal, t) if a hit occurs
void traverseBVH(TLAS &tlas, std::vector<BLAS> &blasList,
                 const std::vector<Mesh> &meshes, pixelCoordinates &ray,
                 hitRecord &rec);

// Shadow Ray: Returns true if any geometry occludes the path to the light
bool isShadowed(TLAS &tlas, std::vector<BLAS> &blasList,
                const std::vector<Mesh> &meshes, Vec3 origin, Vec3 direction,
                float maxDist);

// -----------------------------------------------------------
// Main Render Loop
// -----------------------------------------------------------

// Executes the raytracing pass, calculates shading, and updates the global
// pixel buffer
void raytracer();

#endif // RENDERENGINEV2_RAYTRACER_H