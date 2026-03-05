//
// Created by Imari on 11/2/26.
//

#include "meshGenerating.h"
#include "dataTypes.h"
#include <iostream>

#include "application.h"

void createUnitCube(Mesh &mesh) {
  mesh.vertices.clear();
  // Ensure objectToTextureIndex is large enough for the new ID
  if (objectToTextureIndex.size() <= currentObjectID) {
    objectToTextureIndex.resize(currentObjectID + 1);
  }
  objectToTextureIndex[currentObjectID] =
      0; // Point to default pink/black texture
  mesh.indices.clear();

  // 1. Define the 8 unique corners of a cube
  std::vector<Vec3> corners = {
      {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, // Back face z=-1
      {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}   // Front face z=1
  };

  // 2. Add vertices (Shared!)
  // We only push 8 vertices total.
  for (const auto &pos : corners) {
    Vertex v;
    v.pos = pos;
    v.color = {1, 1, 1};
    v.uv = {0, 0};        // UVs are tricky on a welded cube, keeping 0 for now
    v.normal = norm(pos); // Initial normal pointing out from center
    v.objectID = currentObjectID;
    mesh.vertices.push_back(v);
  }

  currentObjectID++; // Move to next ID for subsequent meshes

  // 3. Add Indices (Topology)
  // We reference the SAME indices for adjacent faces.
  // Indices based on the 'corners' vector above:
  // 0: BL-Back, 1: BR-Back, 2: TR-Back, 3: TL-Back
  // 4: BL-Front, 5: BR-Front, 6: TR-Front, 7: TL-Front

  auto addQuad = [&](int a, int b, int c, int d) {
    mesh.indices.push_back({a, b, c});
    mesh.indices.push_back({a, c, d});
  };

  // Front Face (z=1)
  addQuad(4, 5, 6, 7);

  // Back Face (z=-1)
  // Note: winding order reversed to point outward
  addQuad(1, 0, 3, 2);

  // Top Face (y=1)
  addQuad(7, 6, 2, 3);

  // Bottom Face (y=-1)
  addQuad(0, 1, 5, 4);

  // Right Face (x=1)
  addQuad(5, 1, 2, 6);

  // Left Face (x=-1)
  addQuad(0, 4, 7, 3);
}

void createUVSphere(Mesh &mesh, int slices, int stacks) {
  mesh.vertices.clear();
  // Ensure objectToTextureIndex is large enough for the new ID
  if (objectToTextureIndex.size() <= currentObjectID) {
    objectToTextureIndex.resize(currentObjectID + 1);
  }
  objectToTextureIndex[currentObjectID] = 0; // Default texture (Pink/Black)
  mesh.indices.clear();

  const float PI = 3.14159f;

  // --- Vertex Generation ---
  // (Logic kept exactly as is to preserve Normal/UV placeholders and Seam
  // logic)
  for (int i = 0; i <= stacks; ++i) {
    float lat = static_cast<float>(i) / static_cast<float>(stacks) * PI;
    for (int j = 0; j <= slices; ++j) {
      float lon =
          static_cast<float>(j) / static_cast<float>(slices) * 2.0f * PI;
      float x = std::cos(lon) * std::sin(lat);
      float y = std::cos(lat);
      float z = std::sin(lon) * std::sin(lat);

      // Placeholder attributes preserved as requested
      mesh.vertices.push_back(
          {{x, y, z}, {1, 1, 1}, {0, 0}, {0, 0, 0}, currentObjectID});
    }
  }

  // --- Index Generation ---
  // (Modified to fix topology at the poles)
  for (int i = 0; i < stacks; ++i) {
    for (int j = 0; j < slices; ++j) {
      int first = (i * (slices + 1)) + j;
      int second = first + slices + 1;

      // Top Triangle (Top-Left, Bottom-Left, Top-Right)
      // Skip this triangle if we are at the North Pole (i=0)
      // because 'first' and 'first+1' are geometrically the same point.
      if (i != 0) {
        mesh.indices.push_back({first, second, first + 1});
      }

      // Bottom Triangle (Bottom-Left, Bottom-Right, Top-Right)
      // Skip this triangle if we are at the South Pole (i=stacks-1)
      // because 'second' and 'second+1' are geometrically the same point.
      if (i != (stacks - 1)) {
        mesh.indices.push_back({second, second + 1, first + 1});
      }
    }
  }
  currentObjectID++;

  std::cout << " currentObjectID = " << currentObjectID
            << " texture mapped to: " << objectToTextureIndex[currentObjectID]
            << std::endl;
}

void createIcosahedron(Mesh &mesh) {
  mesh.vertices.clear();
  // Ensure objectToTextureIndex is large enough
  if (objectToTextureIndex.size() <= currentObjectID) {
    objectToTextureIndex.resize(currentObjectID + 1);
  }
  objectToTextureIndex[currentObjectID] = 0;
  mesh.indices.clear();

  const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
  std::vector<Vec3> verts = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
                             {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
                             {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
  for (auto &v : verts) {
    Vec3 n = norm(v);
    mesh.vertices.push_back({n, {1, 1, 1}, {0, 0}, {0, 0, 0}, currentObjectID});
  }
  mesh.indices = {{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
                  {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                  {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
                  {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}};
  currentObjectID++;
}
