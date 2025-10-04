#ifndef __QDISP_H__
#define __QDISP_H__

#include "renderer.h"
#include <mesh.h>
#include <facing.h>
#include <math3d.h>

typedef union {
	struct __attribute__((packed)) {
		s16 x,y,z,w;
	};
	s16 vals[4];
	u64 val;
} mesh_qvert_t;

// Currently upload VU1 code, doesn't check if it's the same & uploads texture if not uploaded
void mesh_quad_prepare_renderer(renderer_t *rend, mesh_type_t *type);

void mesh_quad_type_init(mesh_type_t *type, texture_t *tbuff);
int mesh_quad_create(mesh_t *mesh, mesh_type_t *type, u32 count);
void mesh_quad_draw(mesh_t *mesh, renderer_t *rend);

// Vertex operations
u32 mesh_quad_put(mesh_qvert_t *verts, mesh_qvert_t position, mesh_qvert_t points[4], u8 texId);
u32 mesh_quad_put_dir(mesh_qvert_t *verts, mesh_qvert_t position, enum facing face, u8 texId);

#endif