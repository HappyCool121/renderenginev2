//
// Created by Imari on 11/2/26.
//

#ifndef RENDERENGINEV2_MESHPROCESSING_H
#define RENDERENGINEV2_MESHPROCESSING_H

#include "dataTypes.h"

void normalizeMesh(Mesh& mesh);
void triangulateMesh(Mesh& mesh, const std::vector<PolygonFace>& inputFaces);
void doSubdivide(Mesh& mesh, bool spherize, SubdivMode mode);
void computeNormals(Mesh& mesh);

void applyVertexColoring(Mesh& mesh);
void applyUVProjection(Mesh& mesh, MappingType type);

#endif //RENDERENGINEV2_MESHPROCESSING_H