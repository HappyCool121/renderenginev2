#include "application.h"
#include "dataTypes.h"
#include "meshProcessing.h"
#include "raytracer.h"
#include <iostream>

const int TARGET_FPS = 60;
const int FRAME_DELAY = 1000 / TARGET_FPS;

// =========================================================
// MAIN RUN
// =========================================================

int main(int argc, char *argv[]) {
  std::cout << "--- Raytracer Engine V2 Start ---" << std::endl;

  // 1. INIT SDL and IMGUI
  app = initSDL();
  initIMGUI(app);

  if (app.window == nullptr) {
    return 1;
  }

  pixels.resize(WIDTH * HEIGHT);

  // 2. LOAD PRIMITIVE GEOMETRY
  std::cout << "Loading Geometry..." << std::endl;

  // Load the primitives to object space (likely loads "custom" gltf or default
  // shape)
  objectToTextureIndex.clear();
  currentObjectID = 0;

  SetObjectType(CUSTOM);
  SetObjectType(CUBE);
  SetObjectType(UV_SPHERE);

  // 3. MATERIAL INITIALIZATION
  globalMaterials.clear();

  // 0: Cube Material (Flat, very shiny)
  Material cubeMat;
  cubeMat.name = "Cube Material";
  cubeMat.shadingMode = FLAT_MODE;
  cubeMat.kAmbient = 0.1f;
  cubeMat.kDiffuse = 0.8f;
  cubeMat.kSpecular = 0.5f;
  cubeMat.reflectivity = 0.8f;
  globalMaterials.push_back(cubeMat);

  // 1: Cube Material (phong, shiny) for custom model
  Material phongMat;
  phongMat.name = "Phong Material";
  phongMat.shadingMode = PHONG_MODE;
  phongMat.kAmbient = 0.1f;
  phongMat.kDiffuse = 0.8f;
  phongMat.kSpecular = 0.5f;
  phongMat.reflectivity = 0.3f;
  globalMaterials.push_back(phongMat);

  // 2: Glass Material (Refractive)
  Material glassMat;
  glassMat.name = "Glass";
  glassMat.shadingMode = PHONG_MODE;
  glassMat.kAmbient = 0.0f;
  glassMat.kDiffuse = 0.1f;
  glassMat.kSpecular = 1.0f;
  glassMat.shininess = 128.0f;
  glassMat.reflectivity = 0.1f;
  glassMat.transmission = 1.0f;
  glassMat.ior = 1.5f;
  glassMat.albedo = {1.0f, 1.0f, 1.0f};
  globalMaterials.push_back(glassMat);

  // 4. COMPUTE NORMALS FOR MESH (*normals not extracted for custom objects)
  std::cout << "Computing Normals..." << std::endl;
  for (Mesh &mesh : meshList) {
    computeNormals(mesh);
  }

  std::cout << "Creating Instances..." << std::endl;
  renderObjectList.clear();

  std::cout << "first texture name: " << globalTextures[0].name << std::endl;

  // 5. DEFINE OBJECTS TO RENDER IN SCENE

  // cube
  renderObject obj1{};
  obj1.meshIndex = 1;
  obj1.translation = {1.0f, 0.0f, 3.0f};
  obj1.rotation = {0.0f, 60.0f, 0.0f};
  obj1.scale = {1.0f, 1.0f, 1.0f};
  obj1.materialID = 0; // cube material
  renderObjectList.push_back(obj1);

  // custom object
  renderObject obj2{};
  obj2.meshIndex = 0;
  obj2.translation = {-1.0f, -0.6f, 3.0f};
  obj2.rotation = {180.0f, 180.0f, 0.0f};
  obj2.scale = {2.0f, 2.0f, 2.0f};
  obj2.materialID = 1; // phong material
  renderObjectList.push_back(obj2);

  // window object for refraction
  renderObject obj3{};
  obj3.meshIndex = 2;
  obj3.translation = {0.0f, 0.0f, 1.1f};
  obj3.rotation = {45.0f, 20.0f, 0.0f};
  obj3.scale = {0.5f, 0.5f, 0.5f};
  obj3.materialID = 2; // glass material
  renderObjectList.push_back(obj3);

  // duplicate mesh primitives (only for complex scenes default to zero)
  for (size_t mIdx = 0; mIdx < meshList.size(); mIdx++) {

    int instancesToCreate = 0; // Let's make 3 of each

    for (int i = 0; i < instancesToCreate; i++) {
      renderObject newObject{};

      // Link to geometry
      newObject.meshIndex = (uint32_t)mIdx;

      // Reset Rotation/Scale
      // Note: You might need {180, 0, 0} if your GLTF comes in upside down
      newObject.rotation = {180, 180 + 100 * (float)mIdx, 0};
      newObject.scale = {1.0f, 1.0f, 1.0f};

      // --- IMPROVED POSITIONING ---
      // X axis: Spreads the instances (i)
      // Y axis: Spreads the different mesh types (mIdx) so they don't overlap
      float spacing = 1.0f; // Gap between objects

      // float xPos = -(float)i * spacing;
      // float yPos = -(float)mIdx * spacing;
      float xPos = 1.0f * mIdx;
      float yPos = -0.7f;
      float zPos = 3.0f;

      // Center the group slightly (optional)
      // xPos -= (instancesToCreate * spacing) / 2.0f;

      newObject.translation = {xPos, yPos, zPos};

      renderObjectList.push_back(newObject);
    }
  }

  // light position (only one light source for now)
  lightPos = {0.0f, 10.0f, 0.0f};

  // 6. Build BVH (both BLAS and TLAS)
  std::cout << "Building BLAS..." << std::endl;
  BLASlist.clear();
  Uint32 BVHbuildStart = SDL_GetTicks(); // start timer

  for (const Mesh &mesh : meshList) {
    BLAS blas;
    startBuild(blas, mesh);
    BLASlist.push_back(blas);
  }

  std::cout << "Building TLAS..." << std::endl;
  TLASinstanceList.clear();
  for (renderObject &obj : renderObjectList) {
    TLASinstance tlasInstance;
    tlasInstance.BLASindex = obj.meshIndex;
    updateTLASInstanceTransform(obj, tlasInstance);

    if (tlasInstance.BLASindex < BLASlist.size()) {
      updateInstanceBounds(tlasInstance, BLASlist[tlasInstance.BLASindex]);
    }
    TLASinstanceList.push_back(tlasInstance);
  }

  startTLASBuild();
  std::cout << "BVH Build Complete in " << (SDL_GetTicks() - BVHbuildStart)
            << "ms" << std::endl;

  // 7. RAYTRACING PIPELINE
  raytracer();

  // 8. DISPLAY LOOP
  std::cout << "Displaying Result..." << std::endl;
  bool running = true;
  SDL_Event event;

  while (running) {
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT)
        running = false;

      // Re-render
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
        raytracer();
      }
    }

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Status");
    ImGui::Text("Raytracer V2");
    ImGui::Text("Objects: %d", (int)renderObjectList.size());

    if (ImGui::Button("Re-render (R)")) {
      raytracer();
    }
    ImGui::End();

    ImGui::Render();
    SDL_SetRenderDrawColor(app.renderer, 0, 0, 0, 255);
    SDL_RenderClear(app.renderer);
    SDL_RenderCopy(app.renderer, app.texture, nullptr, nullptr);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), app.renderer);
    SDL_RenderPresent(app.renderer);
    SDL_Delay(30);
  }

  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyTexture(app.texture);
  SDL_DestroyRenderer(app.renderer);
  SDL_DestroyWindow(app.window);
  SDL_Quit();

  return 0;
}
