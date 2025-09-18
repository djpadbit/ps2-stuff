#ifndef __CHUNK_MANAGER_H__
#define __CHUNK_MANAGER_H__

#include "draw_buffers.h"
#include <tamtypes.h>
#include <mesh.h>
#include <chunk_data.h>
#include <qdisp.h>

#define CHUNK_VIEWDIST 8

#define CHUNK_COMPILED_MAX_VERTS (4096*1)

#define CHUNK_DATA_COUNT ((CHUNK_VIEWDIST+2)*(CHUNK_VIEWDIST+2))
#define CHUNK_COMPILED_YCOUNT (WORLD_HEIGHT/CHUNK_HEIGHT)
#define CHUNK_COMPILED_COUNT_PLANE (CHUNK_VIEWDIST*CHUNK_VIEWDIST)
#define CHUNK_COMPILED_COUNT (CHUNK_COMPILED_COUNT_PLANE*CHUNK_COMPILED_YCOUNT)

typedef struct {
	// Account for border chunks
	chunkdata_t chunks[CHUNK_DATA_COUNT];
	struct {
		qvert_t __attribute__((aligned(128))) verts[CHUNK_COMPILED_MAX_VERTS];
		u32 size;
		u16 progress;
	} compiled_chunks[CHUNK_COMPILED_COUNT];

	// Tight memory layout for faster lookup
	struct {
		s64 x;
		s64 z;
	} compiled_pos[CHUNK_COMPILED_COUNT_PLANE];

	mesh_t meshes[CHUNK_COMPILED_COUNT];

	s64 old_x;
	s64 old_z;
} chunk_manager_t;

u8 chunk_manager_chunk_ready(chunk_manager_t *chunk_man, u16 compiled_idx);
u8 chunk_manager_chunk_empty(chunk_manager_t *chunk_man, u16 compiled_idx);
void chunk_manager_init(chunk_manager_t *chunk_man, texbuffer_t *texture);
int chunk_manager_work(chunk_manager_t *chunk_man, int budget);
void chunk_manager_update_pos(chunk_manager_t *chunk_man, s64 x, s64 z);

#endif