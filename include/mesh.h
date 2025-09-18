#ifndef __MESH_H__
#define __MESH_H__

#include "qdisp.h"
#include <math3d.h>
#include <draw_buffers.h>
#include <draw_primitives.h>
#include <draw_sampling.h>

#include <renderer.h>

typedef struct {
	VECTOR __attribute__((aligned(16))) pos;
	VECTOR __attribute__((aligned(16))) rot;

	qvert_t *vertices;
	u32 vert_count;

	prim_t prim;
	clutbuffer_t clut;
	lod_t lod;
	texbuffer_t *texture;

	VECTOR *vu1_extra_data;
	u16 vu1_extra_data_size; // qw
	u16 vu1_min_vert_send;
} mesh_t;

int mesh_create(mesh_t *mesh, u32 count);
void mesh_init(mesh_t *mesh, u32 count, qvert_t *verts);
// Doesn't zero out all the mesh data
void mesh_update(mesh_t *mesh, u32 count, qvert_t *verts);
void mesh_set_quad_prim(mesh_t *mesh, texbuffer_t *tbuff);
void mesh_draw(mesh_t *mesh, renderer_t *rend);

#endif