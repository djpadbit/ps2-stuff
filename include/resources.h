#ifndef __RESOURCES_H__
#define __RESOURCES_H__

#include <math3d.h>

// Terrain texture
extern unsigned int size_terrain;
extern unsigned char terrain[] __attribute__((aligned(16)));

// Cube data
extern const int cube_vertex_count;
extern const int cube_sts_count;
extern const VECTOR cube_vertices[24];
extern const VECTOR cube_sts[4];

#endif