//
// Created by Imari on 11/2/26.
//

#include "dataTypes.h"
#include "meshProcessing.h"
#include <unordered_map>

void normalizeMesh(Mesh& mesh) {
    float maxDist = 0;
    for (const auto& v : mesh.vertices) {
        float d = len(v.pos);
        if(d > maxDist) maxDist = d;
    }
    if (maxDist == 0) maxDist = 1.0f;
    float scale = 1.0f / maxDist;
    for (auto& v : mesh.vertices) {
        v.pos.x *= scale; v.pos.y *= scale; v.pos.z *= scale;
    }
}

#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>

void triangulateMesh(Mesh& mesh, const std::vector<PolygonFace>& inputFaces) {
    mesh.indices.clear();

    for (const auto& face : inputFaces) {
        size_t numVerts = face.indices.size();
        if (numVerts < 3) continue; // Skip lines or points

        // "Triangle Fan" triangulation
        // We anchor at index 0 and fan out to the others.
        // For a Quad [0,1,2,3], this creates tris: [0,1,2] and [0,2,3]
        int v0 = face.indices[0];

        for (size_t i = 1; i < numVerts - 1; ++i) {
            int v1 = face.indices[i];
            int v2 = face.indices[i + 1];

            // Push to your specific Triangle format
            mesh.indices.push_back({v0, v1, v2});
        }
    }
}



// Helper: Safe Normalize
Vec3 safeNorm(Vec3 v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-6f) return { 0, 1, 0 };
    return { v.x / len, v.y / len, v.z / len };
}

// Helper: Basic Vector math helpers if not already defined
Vertex interpolateVertex(const Vertex& v1, const Vertex& v2, float t) {
    Vertex m;
    m.pos = { v1.pos.x + (v2.pos.x - v1.pos.x) * t, v1.pos.y + (v2.pos.y - v1.pos.y) * t, v1.pos.z + (v2.pos.z - v1.pos.z) * t };

    // Lerp other attributes
    m.uv = { v1.uv.x + (v2.uv.x - v1.uv.x) * t, v1.uv.y + (v2.uv.y - v1.uv.y) * t };
    m.color = { v1.color.x + (v2.color.x - v1.color.x) * t, v1.color.y + (v2.color.y - v1.color.y) * t, v1.color.z + (v2.color.z - v1.color.z) * t };
    m.objectID = v1.objectID;

    // Normal interpolation
    Vec3 tmpN = { v1.normal.x + (v2.normal.x - v1.normal.x) * t, v1.normal.y + (v2.normal.y - v1.normal.y) * t, v1.normal.z + (v2.normal.z - v1.normal.z) * t };
    m.normal = safeNorm(tmpN);
    return m;
}

void doSubdivide(Mesh& mesh, bool spherize, SubdivMode mode) {

    // 1. Prepare new data containers
    std::vector<Triangle> newIndices;
    std::vector<Vertex> newVertices = mesh.vertices; // Starts with copy of old vertices

    // Catmull-Clark produces more geometry (6 tris per original tri)
    int multiplier = (mode == catmullClark) ? 6 : 4;
    newIndices.reserve(mesh.indices.size() * multiplier);

    // Update EdgeInfo to store Face Indices (needed for Catmull-Clark)
    struct EdgeInfo {
        int v1, v2;
        int o1 = -1; // Opposite vertex 1
        int o2 = -1; // Opposite vertex 2 (if -1, it's boundary)
        int f1 = -1; // Face Index 1 (Index in mesh.indices)
        int f2 = -1; // Face Index 2
        int newIndex = -1;
    };

    std::unordered_map<uint64_t, EdgeInfo> edgeMap;
    std::vector<std::vector<int>> boundaryNeighbors(mesh.vertices.size());

    // For Loop: Stores all connected vertices
    // For CC: Stores connected Edge Indices (to calculate R_avg)
    std::vector<std::vector<int>> vertexNeighbors(mesh.vertices.size());
    // For CC: Stores connected Face Indices (to calculate F_avg)
    std::vector<std::vector<int>> vertexFaces(mesh.vertices.size());

    auto makeKey = [](int i1, int i2) -> uint64_t {
        uint64_t smaller = (i1 < i2) ? i1 : i2;
        uint64_t larger  = (i1 < i2) ? i2 : i1;
        return (smaller << 32) | larger;
    };

    // =========================================================
    // PASS 1: Connectivity Analysis
    // =========================================================
    for (int faceIdx = 0; faceIdx < mesh.indices.size(); ++faceIdx) {
        const auto& t = mesh.indices[faceIdx];
        int v[3] = { t.v0, t.v1, t.v2 };

        // For CC: Register face to vertices
        if (mode == catmullClark) {
            vertexFaces[v[0]].push_back(faceIdx);
            vertexFaces[v[1]].push_back(faceIdx);
            vertexFaces[v[2]].push_back(faceIdx);
        }

        for (int i = 0; i < 3; ++i) {
            int a = v[i];
            int b = v[(i + 1) % 3];
            int c = v[(i + 2) % 3];

            uint64_t key = makeKey(a, b);

            if (edgeMap.find(key) == edgeMap.end()) {
                // New Edge: Assign f1
                edgeMap[key] = { a, b, c, -1, faceIdx, -1, -1 };
            } else {
                // Existing Edge: Assign f2 and o2
                edgeMap[key].o2 = c;
                edgeMap[key].f2 = faceIdx;
            }

            // Neighbor logic
            auto addUnique = [&](int target, int val) {
                for(int n : vertexNeighbors[target]) if(n == val) return;
                vertexNeighbors[target].push_back(val);
            };

            if (mode == Loop) {
                // Loop needs connected Vertices
                addUnique(a, b);
                addUnique(b, a);
            } else if (mode == catmullClark) {
                // CC needs connected Vertices to find edges later, but simple neighbor list is fine
                addUnique(a, b);
                addUnique(b, a);
            }
        }
    }

    // Identify Boundaries
    for (const auto& pair : edgeMap) {
        const EdgeInfo& e = pair.second;
        if (e.o2 == -1) {
            boundaryNeighbors[e.v1].push_back(e.v2);
            boundaryNeighbors[e.v2].push_back(e.v1);
        }
    }

    // =========================================================
    // CATMULL-CLARK SPECIFIC LOGIC
    // =========================================================
    if (mode == catmullClark) {
        // A. Calculate "Face Points" (One per triangle)
        std::vector<int> facePointIndices;
        facePointIndices.reserve(mesh.indices.size());

        for (const auto& t : mesh.indices) {
            const Vertex& v0 = mesh.vertices[t.v0];
            const Vertex& v1 = mesh.vertices[t.v1];
            const Vertex& v2 = mesh.vertices[t.v2];

            Vertex fPoint;
            // Average Position
            fPoint.pos = { (v0.pos.x + v1.pos.x + v2.pos.x) / 3.0f,
                           (v0.pos.y + v1.pos.y + v2.pos.y) / 3.0f,
                           (v0.pos.z + v1.pos.z + v2.pos.z) / 3.0f };
            // Average Attributes
            fPoint.normal = safeNorm({ (v0.normal.x + v1.normal.x + v2.normal.x), (v0.normal.y + v1.normal.y + v2.normal.y), (v0.normal.z + v1.normal.z + v2.normal.z) });
            fPoint.uv = { (v0.uv.x + v1.uv.x + v2.uv.x)/3.0f, (v0.uv.y + v1.uv.y + v2.uv.y)/3.0f };
            fPoint.color = { (v0.color.x + v1.color.x + v2.color.x)/3.0f, (v0.color.y + v1.color.y + v2.color.y)/3.0f, (v0.color.z + v1.color.z + v2.color.z)/3.0f };
            fPoint.objectID = v0.objectID;

            if (spherize) fPoint.pos = safeNorm(fPoint.pos);

            newVertices.push_back(fPoint);
            facePointIndices.push_back(newVertices.size() - 1);
        }

        // B. Calculate "Edge Points"
        for (auto& pair : edgeMap) {
            EdgeInfo& e = pair.second;
            Vertex& v1 = mesh.vertices[e.v1];
            Vertex& v2 = mesh.vertices[e.v2];
            Vertex ePoint;

            if (e.o2 == -1) {
                // Boundary Edge: Just the midpoint
                ePoint = interpolateVertex(v1, v2, 0.5f);
            } else {
                // Interior Edge: (v1 + v2 + FacePoint1 + FacePoint2) / 4
                Vertex& fp1 = newVertices[facePointIndices[e.f1]];
                Vertex& fp2 = newVertices[facePointIndices[e.f2]];

                ePoint.pos.x = (v1.pos.x + v2.pos.x + fp1.pos.x + fp2.pos.x) * 0.25f;
                ePoint.pos.y = (v1.pos.y + v2.pos.y + fp1.pos.y + fp2.pos.y) * 0.25f;
                ePoint.pos.z = (v1.pos.z + v2.pos.z + fp1.pos.z + fp2.pos.z) * 0.25f;

                // Interpolate other attributes linearly between original verts for simplicity
                Vertex mid = interpolateVertex(v1, v2, 0.5f);
                ePoint.normal = mid.normal;
                ePoint.uv = mid.uv;
                ePoint.color = mid.color;
            }

            if (spherize) ePoint.pos = safeNorm(ePoint.pos);

            newVertices.push_back(ePoint);
            e.newIndex = newVertices.size() - 1;
        }

        // C. Update Original Vertices (Move them)
        // Formula: (F_avg + 2 * R_avg + (n-3) * P) / n
        for (int i = 0; i < mesh.vertices.size(); ++i) {
            Vertex& newV = newVertices[i]; // This currently holds the OLD position
            const Vertex& oldV = mesh.vertices[i];

            // Boundary Rule
            if (!boundaryNeighbors[i].empty()) {
                if (boundaryNeighbors[i].size() == 2) {
                    const Vertex& n1 = mesh.vertices[boundaryNeighbors[i][0]];
                    const Vertex& n2 = mesh.vertices[boundaryNeighbors[i][1]];
                    newV.pos.x = oldV.pos.x * 0.75f + (n1.pos.x + n2.pos.x) * 0.125f;
                    newV.pos.y = oldV.pos.y * 0.75f + (n1.pos.y + n2.pos.y) * 0.125f;
                    newV.pos.z = oldV.pos.z * 0.75f + (n1.pos.z + n2.pos.z) * 0.125f;
                }
                continue; // Skip the CC interior logic for boundary verts
            }

            const auto& neighbors = vertexNeighbors[i];
            const auto& faces = vertexFaces[i];
            float n = (float)faces.size(); // Valence (approximate for general meshes)
            if (n < 3) continue;

            // F_avg: Average of Face Points touching this vertex
            Vec3 F_avg = {0,0,0};
            for (int fIdx : faces) {
                const Vertex& fp = newVertices[facePointIndices[fIdx]];
                F_avg.x += fp.pos.x; F_avg.y += fp.pos.y; F_avg.z += fp.pos.z;
            }
            F_avg.x /= n; F_avg.y /= n; F_avg.z /= n;

            // R_avg: Average of Midpoints of Edges touching this vertex
            // Note: CC definition uses pure midpoints of edges, not the new EdgePoints calculated in step B
            Vec3 R_avg = {0,0,0};
            for (int neighborIdx : neighbors) {
                const Vertex& nb = mesh.vertices[neighborIdx];
                R_avg.x += (oldV.pos.x + nb.pos.x) * 0.5f;
                R_avg.y += (oldV.pos.y + nb.pos.y) * 0.5f;
                R_avg.z += (oldV.pos.z + nb.pos.z) * 0.5f;
            }
            R_avg.x /= n; R_avg.y /= n; R_avg.z /= n;

            // Apply Formula
            float weightOld = (n - 3.0f) / n;
            float weightF   = 1.0f / n;
            float weightR   = 2.0f / n;

            newV.pos.x = (F_avg.x * weightF) + (R_avg.x * weightR) + (oldV.pos.x * weightOld);
            newV.pos.y = (F_avg.y * weightF) + (R_avg.y * weightR) + (oldV.pos.y * weightOld);
            newV.pos.z = (F_avg.z * weightF) + (R_avg.z * weightR) + (oldV.pos.z * weightOld);

            if (spherize) newV.pos = safeNorm(newV.pos);
        }

        // D. Stitching (Quad -> 2 Triangles)
        for (int i = 0; i < mesh.indices.size(); ++i) {
            const auto& t = mesh.indices[i];
            int v0 = t.v0;
            int v1 = t.v1;
            int v2 = t.v2;
            int fp = facePointIndices[i];

            int e01 = edgeMap[makeKey(v0, v1)].newIndex;
            int e12 = edgeMap[makeKey(v1, v2)].newIndex;
            int e20 = edgeMap[makeKey(v2, v0)].newIndex;

            // Quad 1: v0 -> e01 -> fp -> e20
            newIndices.push_back({ v0, e01, fp });
            newIndices.push_back({ v0, fp, e20 });

            // Quad 2: v1 -> e12 -> fp -> e01
            newIndices.push_back({ v1, e12, fp });
            newIndices.push_back({ v1, fp, e01 });

            // Quad 3: v2 -> e20 -> fp -> e12
            newIndices.push_back({ v2, e20, fp });
            newIndices.push_back({ v2, fp, e12 });
        }
    }
    // =========================================================
    // LOOP / LINEAR LOGIC (Legacy path)
    // =========================================================
    else {
        // ... (Your existing PASS 2 code here) ...
        for (auto& pair : edgeMap) {
            EdgeInfo& e = pair.second;
            const Vertex& v1 = mesh.vertices[e.v1];
            const Vertex& v2 = mesh.vertices[e.v2];
            Vertex m = interpolateVertex(v1, v2, 0.5f); // Use helper

            if (mode == Loop && e.o2 != -1) {
                const Vertex& o1 = mesh.vertices[e.o1];
                const Vertex& o2 = mesh.vertices[e.o2];
                m.pos.x = (v1.pos.x + v2.pos.x) * 0.375f + (o1.pos.x + o2.pos.x) * 0.125f;
                m.pos.y = (v1.pos.y + v2.pos.y) * 0.375f + (o1.pos.y + o2.pos.y) * 0.125f;
                m.pos.z = (v1.pos.z + v2.pos.z) * 0.375f + (o1.pos.z + o2.pos.z) * 0.125f;
            }

            if (spherize) m.pos = safeNorm(m.pos);
            newVertices.push_back(m);
            e.newIndex = newVertices.size() - 1;
        }

        // ... (Your existing PASS 3 code here for Loop Vertex Smoothing) ...
        if (mode == Loop) {
             for (int i = 0; i < mesh.vertices.size(); ++i) {
                Vertex& newV = newVertices[i];
                const Vertex& oldV = mesh.vertices[i];

                if (!boundaryNeighbors[i].empty()) {
                    if (boundaryNeighbors[i].size() == 2) {
                        const Vertex& n1 = mesh.vertices[boundaryNeighbors[i][0]];
                        const Vertex& n2 = mesh.vertices[boundaryNeighbors[i][1]];
                        newV.pos.x = oldV.pos.x * 0.75f + (n1.pos.x + n2.pos.x) * 0.125f;
                        newV.pos.y = oldV.pos.y * 0.75f + (n1.pos.y + n2.pos.y) * 0.125f;
                        newV.pos.z = oldV.pos.z * 0.75f + (n1.pos.z + n2.pos.z) * 0.125f;
                    }
                } else {
                    const std::vector<int>& neighbors = vertexNeighbors[i];
                    size_t n = neighbors.size();
                    if (n >= 2) {
                        float beta = (n == 3) ? (3.0f/16.0f) : (1.0f/n)*(0.625f - std::pow(0.375f + 0.25f*std::cos(2.0f*3.14159f/n), 2));
                        float selfWeight = 1.0f - (n * beta);
                        newV.pos = { oldV.pos.x * selfWeight, oldV.pos.y * selfWeight, oldV.pos.z * selfWeight };
                        for (int nid : neighbors) {
                            newV.pos.x += mesh.vertices[nid].pos.x * beta;
                            newV.pos.y += mesh.vertices[nid].pos.y * beta;
                            newV.pos.z += mesh.vertices[nid].pos.z * beta;
                        }
                    }
                }
                if (spherize) newV.pos = safeNorm(newV.pos);
             }
        } else if (spherize) {
             for(auto& v : newVertices) v.pos = safeNorm(v.pos);
        }

        // ... (Your existing PASS 4 Stitching for Loop/Linear) ...
        for (const auto& t : mesh.indices) {
            int a = t.v0; int b = t.v1; int c = t.v2;
            int ab = edgeMap[makeKey(a, b)].newIndex;
            int bc = edgeMap[makeKey(b, c)].newIndex;
            int ca = edgeMap[makeKey(c, a)].newIndex;

            newIndices.push_back({a, ab, ca});
            newIndices.push_back({b, bc, ab});
            newIndices.push_back({c, ca, bc});
            newIndices.push_back({ab, bc, ca});
        }
    }

    mesh.vertices = newVertices;
    mesh.indices = newIndices;
}
void computeNormals(Mesh& mesh) {
    for (auto& v : mesh.vertices) v.normal = { 0.0f, 0.0f, 0.0f };

    for (const auto& t : mesh.indices) {
        Vertex& v0 = mesh.vertices[t.v0];
        Vertex& v1 = mesh.vertices[t.v1];
        Vertex& v2 = mesh.vertices[t.v2];

        Vec3 e1 = sub(v1.pos, v0.pos);
        Vec3 e2 = sub(v2.pos, v0.pos);
        Vec3 facenormal = cross(e1, e2);

        v0.normal = add(v0.normal, facenormal);
        v1.normal = add(v1.normal, facenormal);
        v2.normal = add(v2.normal, facenormal);
    }
    for (auto& v : mesh.vertices) v.normal = norm(v.normal);
}

void applyUVProjection(Mesh& mesh, MappingType type) {
    const float PI = 3.14159f;
    for (auto& v : mesh.vertices) {
        Vec3 p = v.pos;
        if (type == SPHERICAL || type == CYLINDRICAL) {
            float theta = std::atan2(p.z, p.x);
            v.uv.x = (theta + PI) / (2.0f * PI);
            v.uv.y = 1.0f - ((p.y + 1.0f) * 0.5f);
        } else if (type == CUBIC) {
            float ax = std::abs(p.x), ay = std::abs(p.y), az = std::abs(p.z);
            if (ax >= ay && ax >= az) { v.uv.x=(p.z+1)*0.5f; v.uv.y=1-(p.y+1)*0.5f; if(p.x>0) v.uv.x=1-v.uv.x; }
            else if (ay >= ax && ay >= az) { v.uv.x=(p.x+1)*0.5f; v.uv.y=1-(p.z+1)*0.5f; if(p.y<0) v.uv.y=1-v.uv.y; }
            else { v.uv.x=(p.x+1)*0.5f; v.uv.y=1-(p.y+1)*0.5f; if(p.z<0) v.uv.x=1-v.uv.x; }
        }
    }
}

void applyVertexColoring(Mesh& mesh) {
    for (auto& v : mesh.vertices) {
        v.color.x = (v.pos.x + 1.0f) * 0.5f;
        v.color.y = (v.pos.y + 1.0f) * 0.5f;
        v.color.z = (v.pos.z + 1.0f) * 0.5f;
    }
}
