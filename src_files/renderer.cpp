//
// Created by Imari on 11/2/26.
//

#include "renderer.h"
#include "application.h"
#include <vector>
#include <iostream>

// reset Framebuffer and Zbuffer
void clearBuffers() {
    std::fill(pixels.begin(), pixels.end(), 0xFF000000);
    std::fill(zBuffer.begin(), zBuffer.end(), 0.0f);
}

void render() {
    // start timer
    Uint32 renderStart = SDL_GetTicks();

    // setup view space mesh
    float cY = std::cos(objRot.y), sY = std::sin(objRot.y);
    float cX = std::cos(objRot.x), sX = std::sin(objRot.x);

    // CHANGED: Include objectID in transformed vertex
    struct TransformedVert {
        Vec3 viewPos; Vec3 color; Vec2 uv; float intensity;
        Vec3 invNorm; Vec3 invCoords; float invDepth;
        int objectID; // Carry ID through transform
    };

    std::vector<TransformedVert> viewVerts;
    viewVerts.reserve(workingMesh.vertices.size());

    // apply rotations/transformations from world space to view space
    for (const auto& v : workingMesh.vertices) {
        // Rotation
        float x1 = v.pos.z * sY + v.pos.x * cY;
        float z1 = v.pos.z * cY - v.pos.x * sY;
        float y1 = v.pos.y;
        float x2 = x1;
        float y2 = y1 * cX - z1 * sX;
        float z2 = y1 * sX + z1 * cX;

        Vec3 viewPos;
        viewPos.x = x2 + objPos.x;
        viewPos.y = y2 + objPos.y;
        viewPos.z = z2 + objPos.z + cameraDist;

        float nx1 = v.normal.z * sY + v.normal.x * cY;
        float nz1 = v.normal.z * cY - v.normal.x * sY;
        float ny1 = v.normal.y;
        float nx2 = nx1;
        float ny2 = ny1 * cX - nz1 * sX;
        float nz2 = ny1 * sX + nz1 * cX;

        Vec3 rotatedNormal = {nx2, ny2, nz2};
        Vec3 lightDir = norm(sub(lightPos, viewPos));
        float dotProduct = dot(rotatedNormal, lightDir);
        float intensity = std::max(0.1f, std::min(1.0f, dotProduct));

        float invDepth = 1.0f/viewPos.z;
        viewVerts.push_back({viewPos, v.color, v.uv, intensity,
                             {nx2/viewPos.z, ny2/viewPos.z, nz2/viewPos.z},
                             {viewPos.x, viewPos.y, viewPos.z},
                             invDepth,
                             v.objectID});
    }

    // Iterate through triangles in view space
    for (const auto& tri : workingMesh.indices) {
        const auto& v0 = viewVerts[tri.v0];
        const auto& v1 = viewVerts[tri.v1];
        const auto& v2 = viewVerts[tri.v2];

        // Culling vertices that are too close to screen
        if (v0.viewPos.z < 0.1f || v1.viewPos.z < 0.1f || v2.viewPos.z < 0.1f) continue;

        // calculate normals vector or triangle
        Vec3 edge1 = sub(v1.viewPos, v0.viewPos);
        Vec3 edge2 = sub(v2.viewPos, v0.viewPos);
        Vec3 normVec = norm(cross(edge1, edge2));

        // Backface culling
        if (dot(normVec, v0.viewPos) > 0) continue;

        // Match texture to current object
        Texture* currentTexture = nullptr;
        int texIndex = 0;
        if (v0.objectID < objectToTextureIndex.size()) {
            texIndex = objectToTextureIndex[v0.objectID];
        }
        if (texIndex < globalTextures.size()) {
            currentTexture = &globalTextures[texIndex];
        }

        // Flat shading color calculation
        uint32_t flatColor = 0;
        if (currentShadingType == FLAT) {
            Vec3 mid = { (v0.viewPos.x+v1.viewPos.x+v2.viewPos.x)/3, (v0.viewPos.y+v1.viewPos.y+v2.viewPos.y)/3, (v0.viewPos.z+v1.viewPos.z+v2.viewPos.z)/3 };
            Vec3 lightDir = norm(sub(lightPos, mid));
            float intensity = std::max(0.1f, std::min(1.0f, dot(normVec, lightDir)));
            int c = (int)(255 * intensity);
            flatColor = (0xFF << 24) | (c << 16) | (c << 8) | c;
        }

        // Project vertices to screen space
        auto project = [&](Vec3 p) {
            return Vec2{ ((p.x / p.z) * FOVscale) + (WIDTH / 2.0f), ((p.y / p.z) * FOVscale) + (HEIGHT / 2.0f) };
        };
        Vec2 p0 = project(v0.viewPos);
        Vec2 p1 = project(v1.viewPos);
        Vec2 p2 = project(v2.viewPos);

        // Check if the triangle is completely outside the screen bounds
        float minX_raw = std::min({p0.x, p1.x, p2.x});
        float maxX_raw = std::max({p0.x, p1.x, p2.x});
        float minY_raw = std::min({p0.y, p1.y, p2.y});
        float maxY_raw = std::max({p0.y, p1.y, p2.y});

        if (maxX_raw < 0 || minX_raw >= WIDTH || maxY_raw < 0 || minY_raw >= HEIGHT) continue;

        // Bounding box calculation for rasterization
        int minX = std::max(0, (int)std::min({p0.x, p1.x, p2.x}));
        int minY = std::max(0, (int)std::min({p0.y, p1.y, p2.y}));
        int maxX = std::min(WIDTH - 1, (int)std::max({p0.x, p1.x, p2.x}));
        int maxY = std::min(HEIGHT - 1, (int)std::max({p0.y, p1.y, p2.y}));

        // Calculate area for barycentric coordinates
        float area = edgeFunction(p0, p1, p2);
        if (std::abs(area) < 1e-5) continue;

        // Interpolating UV coordinates of texture
        float invZ0 = 1.0f/v0.viewPos.z; float invZ1 = 1.0f/v1.viewPos.z; float invZ2 = 1.0f/v2.viewPos.z;
        float u0z = v0.uv.x * invZ0, v0z = v0.uv.y * invZ0;
        float u1z = v1.uv.x * invZ1, v1z = v1.uv.y * invZ1;
        float u2z = v2.uv.x * invZ2, v2z = v2.uv.y * invZ2;

        // Rasterization loop
        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                Vec2 p = { x + 0.5f, y + 0.5f };
                float w0 = edgeFunction(p1, p2, p);
                float w1 = edgeFunction(p2, p0, p);
                float w2 = edgeFunction(p0, p1, p);

                bool inside = (w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0);

                if (inside) {
                    float alpha = w0 / area;
                    float beta  = w1 / area;
                    float gamma = w2 / area;

                    float pInvZ = alpha * invZ0 + beta * invZ1 + gamma * invZ2;
                    if (pInvZ > zBuffer[y * WIDTH + x]) {
                        zBuffer[y * WIDTH + x] = pInvZ;

                        if (currentShadingType == PHONG) {
                            float pInvDepth = v0.invDepth * alpha + v1.invDepth * beta + v2.invDepth * gamma;
                            float normalX = v0.invNorm.x * alpha + v1.invNorm.x * beta + v2.invNorm.x * gamma;
                            float normalY = v0.invNorm.y * alpha + v1.invNorm.y * beta + v2.invNorm.y * gamma;
                            float normalZ = v0.invNorm.z * alpha + v1.invNorm.z * beta + v2.invNorm.z * gamma;
                            Vec3 pixelNormal = norm({normalX/pInvDepth, normalY/pInvDepth, normalZ/pInvDepth});

                            float pCoordX = v0.invCoords.x * alpha + v1.invCoords.x * beta + v2.invCoords.x * gamma;
                            float pCoordY = v0.invCoords.y * alpha + v1.invCoords.y * beta + v2.invCoords.y * gamma;
                            float pCoordZ = v0.invCoords.z * alpha + v1.invCoords.z * beta + v2.invCoords.z * gamma;
                            Vec3 pWorld = {pCoordX/pInvDepth, pCoordY/pInvDepth, pCoordZ/pInvDepth};

                            float baseR = 1.0f, baseG = 1.0f, baseB = 1.0f;

                            // --- CHANGED: USE CURRENT TRIANGLE'S TEXTURE ---
                            if (currentTexture && currentTexture->loaded) {
                                float u = (u0z * alpha + u1z * beta + u2z * gamma) / pInvDepth;
                                float v = (v0z * alpha + v1z * beta + v2z * gamma) / pInvDepth;
                                int tx = (int)(u * currentTexture->width) % currentTexture->width;
                                int ty = (int)(v * currentTexture->height) % currentTexture->height;
                                if (tx < 0) tx += currentTexture->width;
                                if (ty < 0) ty += currentTexture->height;

                                uint32_t texPixel = currentTexture->pixels[ty * currentTexture->width + tx];
                                baseR = ((texPixel >> 16) & 0xFF) / 255.0f;
                                baseG = ((texPixel >> 8) & 0xFF) / 255.0f;
                                baseB = (texPixel & 0xFF) / 255.0f;
                            } else {
                                baseR = 0.8f; baseG = 0.8f; baseB = 0.8f;
                            }

                            Vec3 lightDir = norm(sub(lightPos, pWorld));
                            float diff = std::max(0.0f, dot(pixelNormal, lightDir));

                            Vec3 viewDir = norm(sub({0,0,0}, pWorld));
                            float NdotL = dot(pixelNormal, lightDir);
                            Vec3 reflection = sub({ 2.0f * NdotL * pixelNormal.x, 2.0f * NdotL * pixelNormal.y, 2.0f * NdotL * pixelNormal.z }, lightDir);
                            reflection = norm(reflection);

                            float spec = std::pow(std::max(0.0f, dot(reflection, viewDir)), 32.0f);
                            float amb = 0.1f; float specStr = 0.6f;

                            float rFinal = (amb + diff) * baseR + (specStr * spec);
                            float gFinal = (amb + diff) * baseG + (specStr * spec);
                            float bFinal = (amb + diff) * baseB + (specStr * spec);

                            int ir = (int)(std::min(1.0f, rFinal) * 255);
                            int ig = (int)(std::min(1.0f, gFinal) * 255);
                            int ib = (int)(std::min(1.0f, bFinal) * 255);
                            pixels[y * WIDTH + x] = (0xFF << 24) | (ir << 16) | (ig << 8) | ib;
                        }

                        else if (currentShadingType == FLAT) {
                            pixels[y * WIDTH + x] = flatColor;
                        }
                        else if (currentShadingType == TEXTURE && currentTexture && currentTexture->loaded) {
                            float z = 1.0f / pInvZ;
                            float u = (alpha * u0z + beta * u1z + gamma * u2z) * z;
                            float v = (alpha * v0z + beta * v1z + gamma * v2z) * z;
                            int tx = (int)(u * currentTexture->width) % currentTexture->width;
                            int ty = (int)(v * currentTexture->height) % currentTexture->height;
                            if (tx < 0) tx += currentTexture->width;
                            if (ty < 0) ty += currentTexture->height;
                            pixels[y * WIDTH + x] = currentTexture->pixels[ty * currentTexture->width + tx];
                        }
                        else if (currentShadingType == VERTEX_COLOR) {
                            float r = alpha * v0.color.x + beta * v1.color.x + gamma * v2.color.x;
                            float g = alpha * v0.color.y + beta * v1.color.y + gamma * v2.color.y;
                            float b = alpha * v0.color.z + beta * v1.color.z + gamma * v2.color.z;
                            int ir = (int)(r * 255); int ig = (int)(g * 255); int ib = (int)(b * 255);
                            pixels[y * WIDTH + x] = (0xFF << 24) | (ir << 16) | (ig << 8) | ib;
                        }
                        else if (currentShadingType == GOURAUD) {
                            float intensity = alpha * v0.intensity + beta * v1.intensity + gamma * v2.intensity;
                            int ir = (int)(std::min(1.0f, intensity) * 255);
                            pixels[y * WIDTH + x] = (0xFF << 24) | (ir << 16) | (ir << 8) | ir;
                        }
                    }
                }
            }
        }
    }


    SDL_UpdateTexture(app.texture, nullptr, pixels.data(), WIDTH * sizeof(uint32_t));
    // SDL_RenderCopy(app.renderer, app.texture, nullptr, nullptr);
    // SDL_RenderPresent(app.renderer);

    Uint32 elapsed = SDL_GetTicks() - renderStart;
    frameCount++;
    if (continuousRender && SDL_GetTicks() - lastLogTime > 1000) {
        LogFrame(frameCount, elapsed, workingMesh, currentShape, currentShadingType);
        lastLogTime = SDL_GetTicks();
    } else if (!continuousRender) {
        LogFrame(frameCount, elapsed, workingMesh, currentShape, currentShadingType);
    }
    toRender = false;
}