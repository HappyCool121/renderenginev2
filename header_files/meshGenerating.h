//
// Created by Imari on 11/2/26.
//

#ifndef RENDERENGINEV2_MESHGENERATING_H
#define RENDERENGINEV2_MESHGENERATING_H

#include "dataTypes.h"

void createUnitCube(Mesh& mesh);
void createUVSphere(Mesh& mesh, int slices, int stacks);
void createIcosahedron(Mesh& mesh);

#endif //RENDERENGINEV2_MESHGENERATING_H