//
// Created by Imari on 11/2/26.
//


#include "application.h"
#include "meshProcessing.h"
#include "assetLoader.h"
#include "meshGenerating.h"
#include <iostream>

// initialize SDL window, return pointers window, renderer, texture
AppContext initSDL() {

    AppContext app = {nullptr, nullptr, nullptr};

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return app;

    app.window = SDL_CreateWindow("Multi-Object Renderer",
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED,
                                  WIDTH, HEIGHT, 0);

    app.renderer = SDL_CreateRenderer(app.window, -1,
                                      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    app.texture = SDL_CreateTexture(app.renderer,
                                    SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    WIDTH, HEIGHT);

    std::cout << "Initialized SDL window" << std::endl;

    return app;
}
void initIMGUI(AppContext app) {
    // --- Init Imgui ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(app.window, app.renderer);
    ImGui_ImplSDLRenderer2_Init(app.renderer);

    std::cout << "Initialized SDL Renderer" << std::endl;
}


void SetObjectType(ShapeType type) {
    currentShape = type;

    // Reset default texture state for primitives if not custom
    if (type != CUSTOM) {
        objectToTextureIndex.clear();
        objectToTextureIndex.push_back(0); // Use default texture
    }

    if (type == CUBE) createUnitCube(baseMesh);
    else if (type == UV_SPHERE) createUVSphere(baseMesh, 20, 20);
    else if (type == ICOSPHERE) createIcosahedron(baseMesh);
    else if (type == CUSTOM) {
        // NOTE: Replace with your specific path
        if(loadGLTF("FULL FILE PATH", baseMesh)) {
            currentMapping = CUSTOM_UV;
            currentShadingType = TEXTURE;
        } else {
            createUnitCube(baseMesh);
        }
    }

    normalizeMesh(baseMesh);
    SetSubdivision(0);
}
void SetSubdivision(int level) {
    if (level < 0) level = 0;
    subdivisionLevel = level;
    workingMesh = baseMesh;

    bool spherize = (currentShape == ICOSPHERE || currentShape == UV_SPHERE);
    for(int i=0; i<subdivisionLevel; i++) {
        doSubdivide(workingMesh, spherize, currentSubdivisionType);
    }
    UpdateMeshAttributes();
}
void UpdateMeshAttributes() {
    applyVertexColoring(workingMesh);
    computeNormals(workingMesh);
    SetUVType(currentMapping);
}
void SetUVType(MappingType type) {
    currentMapping = type;
    applyUVProjection(workingMesh, currentMapping);
    toRender = true;
}

void LogFrame(int frameID, Uint32 elapsedMS, const Mesh& mesh, ShapeType shape, ShadingType shading) {
    std::cout << "[Frame " << frameID << "] "
              << "Time: " << elapsedMS << "ms | "
              << "FPS: " << (elapsedMS > 0 ? 1000/elapsedMS : 999) << " | "
              << "Tris: " << mesh.indices.size() << " | "
              << "Verts: " << mesh.vertices.size() << " | "
              << "Textures: " << globalTextures.size()
              << std::endl;
}
