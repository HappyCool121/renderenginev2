#include <iostream>
#include "application.h"
#include "assetLoader.h"
#include "dataTypes.h"
#include "renderer.h"

const int TARGET_FPS = 60;
const int FRAME_DELAY = 1000 / TARGET_FPS; // Results in ~16ms per frame// TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.


int main() {

    std::cout << "Run started" << std::endl;
    // initialize SDL window

    app = initSDL();
    initIMGUI(app);

    if (app.window == nullptr) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1; // Exit gracefully instead of crashing
    }

    pixels.resize(WIDTH * HEIGHT);
    zBuffer.resize(WIDTH * HEIGHT);

    currentSubdivisionType = Loop;

    // load placeholder texture with index 0
    globalTextures.push_back(generateDefaultTexture());

    // try load secondary custom image texture (.bmp file)
    SDL_Surface* surf = SDL_LoadBMP("texture.bmp");
    if (surf) {
        SDL_Surface* fmt = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ARGB8888, 0);
        Texture bmpTex;
        bmpTex.width = fmt->w;
        bmpTex.height = fmt->h;
        bmpTex.pixels.resize(fmt->w * fmt->h);
        std::memcpy(bmpTex.pixels.data(), fmt->pixels, fmt->w * fmt->h * 4);
        bmpTex.loaded = true;
        bmpTex.name = "texture.bmp";

        // If we load this, we might want to replace the default at index 0 or add it.
        // For this demo, let's update index 0 to this loaded texture for basic shapes.
        globalTextures[0] = bmpTex;

        SDL_FreeSurface(surf);
        SDL_FreeSurface(fmt);
    }

    // initialize vars for logs
    bool running = true;
    SDL_Event event;
    int frameCount = 0;
    Uint32 lastLogTime = 0;

    SetObjectType(CUBE);

    // main frame loop
    while (running) {

        frameStart = SDL_GetTicks();

        // get all inputs
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                SDL_Keycode k = event.key.keysym.sym;
                if (k == SDLK_c) SetObjectType(CUBE);
                if (k == SDLK_u) SetObjectType(UV_SPHERE);
                if (k == SDLK_i) SetObjectType(ICOSPHERE);
                if (k == SDLK_o) SetObjectType(CUSTOM);
                if (k == SDLK_EQUALS) SetSubdivision(subdivisionLevel + 1);
                if (k == SDLK_MINUS)  SetSubdivision(subdivisionLevel - 1);
                if (k == SDLK_SPACE) { currentShadingType = TEXTURE; toRender = true; }
                if (k == SDLK_v)     { currentShadingType = VERTEX_COLOR; toRender = true; }
                if (k == SDLK_f)     { currentShadingType = FLAT; toRender = true; }
                if (k == SDLK_g)     { currentShadingType = GOURAUD; toRender = true; }
                if (k == SDLK_p)     { currentShadingType = PHONG; toRender = true; }
                if (k == SDLK_1) SetUVType(SPHERICAL);
                if (k == SDLK_2) SetUVType(CYLINDRICAL);
                if (k == SDLK_3) SetUVType(CUBIC);
                if (k == SDLK_4) SetUVType(CUSTOM_UV);
                if (k == SDLK_n) { continuousRender = !continuousRender; toRender = true; }
                if (k == SDLK_r) autoRotateObject = !autoRotateObject;
                if (k == SDLK_m) moveObjectToggle = !moveObjectToggle;
                if (k == SDLK_l)  autoRotateLight = !autoRotateLight;
            }
        }

        if (autoRotateObject) { objRot.y += 0.02f; toRender = true; }
        if (autoRotateLight) {
            static float angle = 0; angle += 0.05f;
            lightPos.x = std::sin(angle) * 5.0f;
            lightPos.z = std::cos(angle) * 5.0f;
            toRender = true;
        }

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (moveObjectToggle) {
            if (keys[SDL_SCANCODE_W]) { objPos.z += 0.1f; toRender = true; }
            if (keys[SDL_SCANCODE_S]) { objPos.z -= 0.1f; toRender = true; }
            if (keys[SDL_SCANCODE_A]) { objPos.x -= 0.1f; toRender = true; }
            if (keys[SDL_SCANCODE_D]) { objPos.x += 0.1f; toRender = true; }
            if (keys[SDL_SCANCODE_Q]) { objPos.y -= 0.1f; toRender = true; }
            if (keys[SDL_SCANCODE_E]) { objPos.y += 0.1f; toRender = true; }
        }
        if (!autoRotateLight) {
            if (keys[SDL_SCANCODE_RIGHT]) {lightPos.x += 0.2f; toRender = true; }
            if (keys[SDL_SCANCODE_LEFT])  {lightPos.x -= 0.2f; toRender = true; }
            if (keys[SDL_SCANCODE_UP])  {lightPos.y -= 0.2f; toRender = true; }
            if (keys[SDL_SCANCODE_DOWN])  {lightPos.y += 0.2f; toRender = true; }
            if (keys[SDL_SCANCODE_Z])  {lightPos.z -= 0.2f; toRender = true; }
            if (keys[SDL_SCANCODE_X])  {lightPos.z += 0.2f; toRender = true; }
        }
        if (!autoRotateObject) {
            int mx, my;
            if (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                objRot.y = (mx - static_cast<float>(WIDTH)/2) * 0.01f;
                objRot.x = (my - static_cast<float>(HEIGHT)/2) * 0.01f;
                toRender = true;
            }
        }

        // --- ImGui Update ---
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::SetNextWindowBgAlpha(0.35f);
        if (ImGui::Begin("Debug Stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Separator();

            // Mapping Enums to Strings for display
            const char* shapeNames[] = { "Cube", "UV Sphere", "Icosahedron", "Custom" };
            const char* shadingNames[] = { "Flat", "Gouraud", "Phong", "Texture", "Vertex Color" };
            const char* uvNames[] = { "Spherical", "Cylindrical", "Cubic", "Custom UV" };

            // --- NEW: DISPLAY READABLE TYPES ---
            // Ensure the index is within bounds of the arrays before accessing
            ImGui::Text("Shape: %s", shapeNames[(int)currentShape]);
            ImGui::Text("Shading: %s", shadingNames[(int)currentShadingType]);
            ImGui::Text("UV Mapping: %s", uvNames[(int)currentMapping]);

            ImGui::Separator();
            ImGui::Text("Pos: (%.1f, %.1f, %.1f)", objPos.x, objPos.y, objPos.z);
            ImGui::Text("Sub-Meshes: %d", (int)objectToTextureIndex.size());
            ImGui::Text("Textures Loaded: %d", (int)globalTextures.size());
        }
        ImGui::End();

        if (continuousRender || toRender) {

            clearBuffers();
            render();

        } else {
            SDL_Delay(10);
        }

        SDL_RenderCopy(app.renderer, app.texture, nullptr, nullptr);
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), app.renderer);
        SDL_RenderPresent(app.renderer);

        // How long did this frame take?
        frameTime = SDL_GetTicks() - frameStart;

        // If the frame was too fast, wait the remaining time
        if (FRAME_DELAY > frameTime) {
            SDL_Delay(FRAME_DELAY - frameTime);
        }


    }
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyTexture(app.texture);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();

    return 0;// TIP See CLion help at <a href="https://www.jetbrains.com/help/clion/">jetbrains.com/help/clion/</a>. Also, you can try interactive lessons for CLion by selecting 'Help | Learn IDE Features' from the main menu.
}