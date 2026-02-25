//
// Created by Imari on 11/2/26.
//

#ifndef RENDERENGINEV2_ASSETLOADER_H
#define RENDERENGINEV2_ASSETLOADER_H

#include "dataTypes.h"
#include "tiny_gltf.h"

bool loadGLTF(const std::string& filename, Mesh& mesh);

Texture generateDefaultTexture();
Texture loadTextureFromGLTFImage(const tinygltf::Image& img);

#endif //RENDERENGINEV2_ASSETLOADER_H