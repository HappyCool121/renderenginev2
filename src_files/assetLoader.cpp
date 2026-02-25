//
// Created by Imari on 11/2/26.
//

#include <iostream>
#include "assetLoader.h"
#include "dataTypes.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "application.h"
#include "tiny_gltf.h"

// --- REWRITTEN: GLTF LOADING FOR MULTIPLE OBJECTS & BASE COLOR TEXTURES ---
bool loadGLTF(const std::string& filename, Mesh& mesh) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = false;
    // Handle binary (.glb) vs text (.gltf)
    if (filename.find(".glb") != std::string::npos)
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    else
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!warn.empty()) std::cout << "Warn: " << warn << std::endl;
    if (!err.empty()) std::cerr << "Err: " << err << std::endl;
    if (!ret) return false;

    // 1. Reset Scene Data
    mesh.vertices.clear();
    mesh.indices.clear();
    objectToTextureIndex.clear();

    // 2. Texture Management
    // If this is the first load, ensure default texture exists at index 0.
    // If reloading, clear old model textures but keep the default at index 0.
    if (globalTextures.empty()) {
        globalTextures.push_back(generateDefaultTexture());
    } else {
        // Keep only the default texture (index 0), remove previous model textures
        globalTextures.resize(1);
    }

    // Mapping: glTF Texture Index (0, 1, 2...) -> Engine Texture Index (Start at 1, 2...)
    std::vector<int> gltfToEngineTexIndex(model.textures.size(), 0);

    // Loop through all textures defined in the GLTF file
    for (size_t i = 0; i < model.textures.size(); ++i) {
        int sourceImageIdx = model.textures[i].source;

        // Check if the texture points to a valid image source
        if (sourceImageIdx >= 0 && sourceImageIdx < model.images.size()) {
            tinygltf::Image& img = model.images[sourceImageIdx];

            // Load using the helper function
            Texture newTex = loadTextureFromGLTFImage(img);

            // Add to the global engine registry
            globalTextures.push_back(newTex);

            // Map the glTF ID to the new Engine ID
            // (Engine ID is size - 1 because we just pushed back)
            gltfToEngineTexIndex[i] = static_cast<int>(globalTextures.size()) - 1;

            std::cout << "Loaded GLTF Tex [" << i << "] -> Engine Tex ["
                      << gltfToEngineTexIndex[i] << "] (" << newTex.width << "x" << newTex.height << ")" << std::endl;
        }
    }

    // 3. Process Meshes
    // We assign a unique objectID for every primitive (sub-mesh) found
    int currentObjectID = 0;

    for (const auto& gltfMesh : model.meshes) {
        for (const auto& primitive : gltfMesh.primitives) {

            // Capture current vertex count for index offsetting
            int vertexStartOffset = static_cast<int>(mesh.vertices.size());

            // --- A. Determine Texture Index ---
            int engineTextureIndex = 0; // Default to pink/black checkerboard

            // If the primitive has a material assigned
            if (primitive.material >= 0 && primitive.material < model.materials.size()) {
                const auto& mat = model.materials[primitive.material];

                // Look specifically for the "baseColorTexture" (PBR standard)
                auto it = mat.values.find("baseColorTexture");
                if (it != mat.values.end()) {
                    // Get the glTF texture index
                    int gltfTexIdx = it->second.TextureIndex();

                    // Convert to Engine texture index using our map
                    if (gltfTexIdx >= 0 && gltfTexIdx < gltfToEngineTexIndex.size()) {
                        engineTextureIndex = gltfToEngineTexIndex[gltfTexIdx];
                    }
                }
            }

            // --- B. Register Object ID mapping ---
            // Ensure map is large enough
            if (objectToTextureIndex.size() <= currentObjectID) {
                objectToTextureIndex.resize(currentObjectID + 1);
            }
            objectToTextureIndex[currentObjectID] = engineTextureIndex;

            // --- C. Extract Geometry ---
            auto itPos = primitive.attributes.find("POSITION");
            auto itTex = primitive.attributes.find("TEXCOORD_0");

            // Skip if no positions
            if (itPos == primitive.attributes.end()) continue;

            // Position Accessor
            const tinygltf::Accessor& posAccessor = model.accessors[itPos->second];
            const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];
            const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

            // UV Accessor (Optional)
            const float* uvs = nullptr;
            if (itTex != primitive.attributes.end()) {
                const tinygltf::Accessor& uvAccessor = model.accessors[itTex->second];
                const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
                const tinygltf::Buffer& uvBuffer = model.buffers[uvView.buffer];
                uvs = reinterpret_cast<const float*>(&uvBuffer.data[uvView.byteOffset + uvAccessor.byteOffset]);
            }

            // Fill Vertices
            for (size_t i = 0; i < posAccessor.count; ++i) {
                Vertex v;

                // Coordinate Conversion (GLTF Y-Up -> Engine Z-Up)
                float x = positions[i * 3 + 0];
                float y = positions[i * 3 + 1];
                float z = positions[i * 3 + 2];

                // Adjust logic to match previous coordinate system preference
                v.pos.x = x;
                v.pos.y = -y;
                v.pos.z = -z;

                // UVs
                if (uvs) {
                    v.uv.x = uvs[i * 2 + 0];
                    v.uv.y = uvs[i * 2 + 1];
                } else {
                    v.uv = {0.0f, 0.0f};
                }

                v.color = {1.0f, 1.0f, 1.0f};
                v.normal = {0.0f, 0.0f, 0.0f}; // Computed later in main loop
                v.objectID = currentObjectID;  // IMPORTANT: Assign current ID

                mesh.vertices.push_back(v);
            }

            // Fill Indices
            // 1. Check if indices exist
            if (primitive.indices >= 0) {
                const tinygltf::Accessor& idxAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& idxView = model.bufferViews[idxAccessor.bufferView];
                const tinygltf::Buffer& idxBuffer = model.buffers[idxView.buffer];

                // 2. Extract raw indices into a temporary vector to handle types (Short vs Int)
                // This removes the need to duplicate logic for every topology type
                std::vector<uint32_t> rawIndices;
                rawIndices.reserve(idxAccessor.count);

                const uint8_t* dataPtr = &idxBuffer.data[idxView.byteOffset + idxAccessor.byteOffset];

                if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const unsigned short* buf = reinterpret_cast<const unsigned short*>(dataPtr);
                    for (size_t i = 0; i < idxAccessor.count; ++i) {
                        rawIndices.push_back(buf[i]);
                    }
                } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const unsigned int* buf = reinterpret_cast<const unsigned int*>(dataPtr);
                    for (size_t i = 0; i < idxAccessor.count; ++i) {
                        rawIndices.push_back(buf[i]);
                    }
                } else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    // GLTF sometimes uses bytes for very small meshes
                    const uint8_t* buf = reinterpret_cast<const uint8_t*>(dataPtr);
                    for (size_t i = 0; i < idxAccessor.count; ++i) {
                        rawIndices.push_back(buf[i]);
                    }
                }

                // 3. Process based on Topology Mode (Triangulate Strips/Fans)
                // Mode 4: Triangles (Standard)
                // Mode 5: Triangle Strip
                // Mode 6: Triangle Fan
                int mode = primitive.mode;
                // Default to Triangles if mode is -1 (some exporters omit it)
                if (mode == -1) mode = TINYGLTF_MODE_TRIANGLES;

                if (mode == TINYGLTF_MODE_TRIANGLES) {
                    // Standard: Read 3 indices at a time
                    for (size_t i = 0; i < rawIndices.size(); i += 3) {
                        mesh.indices.push_back({
                            (int)rawIndices[i] + vertexStartOffset,
                            (int)rawIndices[i+1] + vertexStartOffset,
                            (int)rawIndices[i+2] + vertexStartOffset
                        });
                    }
                }
                else if (mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
                    // Strip: Re-use previous two vertices
                    // Note: Winding order flips every odd triangle in a strip
                    for (size_t i = 0; i < rawIndices.size() - 2; ++i) {
                        int a, b, c;
                        if (i % 2 == 0) {
                            // Even: A, B, C
                            a = rawIndices[i];
                            b = rawIndices[i+1];
                            c = rawIndices[i+2];
                        } else {
                            // Odd: A, C, B (Flip to maintain normal direction)
                            a = rawIndices[i];
                            b = rawIndices[i+2];
                            c = rawIndices[i+1];
                        }
                        mesh.indices.push_back({
                            a + vertexStartOffset,
                            b + vertexStartOffset,
                            c + vertexStartOffset
                        });
                    }
                }
                else if (mode == TINYGLTF_MODE_TRIANGLE_FAN) {
                    // Fan: First vertex is the "Hub", connects to current and next
                    // Triangle 0: [0, 1, 2], Triangle 1: [0, 2, 3], etc.
                    int center = rawIndices[0];
                    for (size_t i = 1; i < rawIndices.size() - 1; ++i) {
                        mesh.indices.push_back({
                            center + vertexStartOffset,
                            (int)rawIndices[i] + vertexStartOffset,
                            (int)rawIndices[i+1] + vertexStartOffset
                        });
                    }
                }
            }

            // Move to next Object ID
            currentObjectID++;
        }
    }

    return !mesh.vertices.empty();
}

// create placeholder if texture is missing
Texture generateDefaultTexture() {
    Texture tex;
    tex.width = 256;
    tex.height = 256;
    tex.pixels.resize(256 * 256);
    tex.loaded = true;
    tex.name = "Default Pink/Black";

    for (int i = 0; i < 256 * 256; i++) {
        int x = i % 256;
        int y = i / 256;
        bool check = ((x / 32) + (y / 32)) % 2 == 0;
        tex.pixels[i] = check ? 0xFFFF00FF : 0xFF000000;
    }
    return tex;
}

Texture loadTextureFromGLTFImage(const tinygltf::Image& img) {
    Texture tex;
    tex.width = img.width;
    tex.height = img.height;
    tex.pixels.resize(tex.width * tex.height);
    tex.loaded = true;
    tex.name = img.name;

    const unsigned char* data = img.image.data();
    int components = img.component;

    for (int i = 0; i < tex.width * tex.height; ++i) {
        uint8_t r, g, b, a;
        if (components == 4) {
            r = data[i * 4 + 0]; g = data[i * 4 + 1]; b = data[i * 4 + 2]; a = data[i * 4 + 3];
        } else if (components == 3) {
            r = data[i * 3 + 0]; g = data[i * 3 + 1]; b = data[i * 3 + 2]; a = 255;
        } else {
            r = g = b = data[i]; a = 255;
        }
        tex.pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    return tex;
}


