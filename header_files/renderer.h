//
// Created by Imari on 11/2/26.
//

#ifndef RENDERENGINEV2_RENDERER_H
#define RENDERENGINEV2_RENDERER_H

#include "dataTypes.h"
#include "math.h"
#include <vector>
#include "application.h"
#include <cstdint>

// serves as the main render engine that produces the final result;
// will take in all state variables, objects, textures, positions, camera settings



// containers used by render engine (frameBuffer and zBuffer)
inline std::vector<uint32_t> pixels;
inline std::vector<float> zBuffer;

void clearBuffers();

void render();

// reset Framebuffer/ Zbuffer

// pass in camera settings

// pass in base mesh (with subdivision, UV mapping, object texture ID, vertex coloring, etc)

// pass in state variables (shading type)

// apply translations/rotations to base mesh -> world space to view space

// final working mesh + check that all textures are available

// calculate necessary parameters for vertices in view space

// project vertices onto screen, transform vertices to pixel screen

// loop through triangles

// rasterize (calculate color values depending on shading type, perform/append depth test)

// write colors to framebuffer

// pass Framebuffer to application for display


#endif //RENDERENGINEV2_RENDERER_H