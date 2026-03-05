//
// Created by Imari on 11/2/26.
//

// APPLICATION HEADER FILE
// Handles windows, imgui overlay, scene data (object states, textures, texture
// maps)

#ifndef RENDERENGINEV2_APPLICATION_H
#define RENDERENGINEV2_APPLICATION_H

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "dataTypes.h"
#include "imgui.h"
#include <SDL.h>

// scene data: contains globals and state variables

// will handle all variables and objects in the scene, including the render
// settings serves as the orchestrator for the whole application

// -------------------------------------------
// Single object rendering (rasterizer)
// -------------------------------------------

// texture handling
inline std::vector<Texture> globalTextures;
inline std::vector<int> objectToTextureIndex;
inline std::vector<Material> globalMaterials;

inline int currentObjectID = 0;

// mesh handling
inline Mesh baseMesh;
inline Mesh workingMesh;

// object and light positions/rotation
inline Vec3 objPos;
inline Vec3 objRot;
inline Vec3 lightPos = {0.0f, 2.0f, -2.0f};

// camera settings
inline float cameraDist = 5.0f;
inline const int WIDTH = 800;
inline const int HEIGHT = 600;

inline float cameraFOV = 90.0f;
inline float FOVscale =
    (HEIGHT / 2.0f) / std::tan(cameraFOV * 0.5f * 3.14159f / 180.0f);

// state variables (ONLY for SINGLE OBJECT RENDERING)
inline ShapeType currentShape;
inline ShadingType currentShadingType;
inline MappingType currentMapping;
inline int subdivisionLevel;
inline SubdivMode currentSubdivisionType;
inline ShadingMode globalShadingMode = PHONG_MODE;

inline bool autoRotateObject;
inline bool autoRotateLight;
inline bool moveObjectToggle;

// continuous render flags
inline bool continuousRender;
inline bool toRender = true;

// Debug vars
inline int frameCount;
inline Uint32 lastLogTime = 0;
inline int frameStart = 0;
inline float frameTime = 0.0f;

// =================================
// Raytracing parameters
// =================================

// main structures (multiple object rendering)

// world space:
// TLAS -> TLAS nodes -> TLAS leaf node -> TLAS instance -> BLAS

// object space:
// BLAS -> BLAS nodes -> BLAS leaf node -> triangles

// object space:
// list of BLAS (for each mesh primitive)
inline std::vector<BLAS> BLASlist;
// list of primitive meshes
inline std::vector<Mesh> meshList;

// world space:
// main TLAS:
inline TLAS mainTLAS;
// list of TLAS instances
inline std::vector<TLASinstance> TLASinstanceList;
// list of objects to be rendered
inline std::vector<renderObject> renderObjectList;

// world pixel coordinates and rays
inline std::vector<pixelCoordinates> worldPixels;

// =================================
// Init SDL window
// =================================

// SLD window pointers
typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
} AppContext;

inline AppContext app;

// initialize SDL window
AppContext initSDL();

// =================================
// Init imgui interface
// =================================

void initIMGUI(AppContext app);

// =================================
// Cascading state updates
// =================================

// object hierarchy from top to bottom

void SetObjectType(ShapeType type);
void SetSubdivision(int level);
void UpdateMeshAttributes();
void SetUVType(MappingType type);

// =================================
// Debug Log
// =================================

void LogFrame(int frameID, Uint32 elapsedMS, const Mesh &mesh, ShapeType shape,
              ShadingType shading);

#endif // RENDERENGINEV2_APPLICATION_H