#ifndef __MESH_H__
#define __MESH_H__

#include <math3d.h>
#include <draw_buffers.h>
#include <draw_primitives.h>
#include <draw_sampling.h>

#include <renderer.h>

// Generic mesh data type, might be changed
typedef struct {
	prim_t prim;
	clutbuffer_t clut;
	lod_t lod;
	texture_t *texture;

	union {
		u8 vals[4];
		struct {
			u8 r,g,b,a;
		};
		u32 val;
	} color;

	VECTOR *vu1_extra_data;
	u16 vu1_extra_data_size; // qw
	u16 vu1_min_vert_send;
} mesh_type_t;

// mesh_t is in instance of a mesh_type_t
typedef struct {
	MATRIX __attribute__((aligned(64))) local_world;
	VECTOR __attribute__((aligned(16))) pos;
	VECTOR __attribute__((aligned(16))) rot;

	void *vertices;
	u32 vert_count;

	mesh_type_t *type;
} mesh_t;

void mesh_init(mesh_t *mesh, mesh_type_t *type, u32 count, void *verts);
// Doesn't zero out all the mesh data
void mesh_update(mesh_t *mesh, u32 count, void *verts);
// To call when updated pos || rot
void mesh_update_matrix(mesh_t *mesh);

#endif