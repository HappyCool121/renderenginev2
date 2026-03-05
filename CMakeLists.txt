cmake_minimum_required(VERSION 4.1)
project(renderenginev2)

set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")

find_package(SDL2 REQUIRED)
find_package(glm REQUIRED)

set(IMGUI_SOURCES
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/imgui_demo.cpp
        imgui/backends/imgui_impl_sdl2.cpp
        imgui/backends/imgui_impl_sdlrenderer2.cpp
        src_files/renderer.cpp
        header_files/application.h
        src_files/application.cpp
        src_files/raytracer.cpp
        src_files/raytracer.cpp
        header_files/raytracer.h
)

add_executable(renderenginev2
        main.cpp
        src_files/application.cpp
        src_files/assetLoader.cpp
        src_files/meshGenerating.cpp
        src_files/meshProcessing.cpp
        src_files/renderer.cpp
        ${IMGUI_SOURCES})

target_include_directories(renderenginev2 PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/header_files"
        imgui
        imgui/backends
        tinygltf
)

target_link_libraries(renderenginev2
        SDL2::SDL2
        glm::glm)
