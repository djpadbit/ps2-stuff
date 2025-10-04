#ifndef __CHUNK_MANAGER_H__
#define __CHUNK_MANAGER_H__

#include <facing.h>
#include <tamtypes.h>
#include <chunk_data.h>
#include <mesh.h>
#include <mesh_quad.h>

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
		mesh_qvert_t __attribute__((aligned(128))) verts[CHUNK_COMPILED_MAX_VERTS];
		u32 size;
		u16 progress;
	} compiled_chunks[CHUNK_COMPILED_COUNT];

	// Tight memory layout for faster lookup
	struct {
		s64 x;
		s64 z;
	} compiled_pos[CHUNK_COMPILED_COUNT_PLANE];

	mesh_t meshes[CHUNK_COMPILED_COUNT];
	mesh_type_t __attribute__((aligned(128))) chunk_mesh_type;

	s64 old_x;
	s64 old_z;
	u16 player_idx;
} chunk_manager_t;

int chunk_manager_find_compiled_idx(chunk_manager_t *chunk_man, s64 x, s64 z);
u8 chunk_manager_chunk_ready(chunk_manager_t *chunk_man, u16 compiled_idx);
u8 chunk_manager_chunk_empty(chunk_manager_t *chunk_man, u16 compiled_idx);
void chunk_manager_init(chunk_manager_t *chunk_man, texture_t *texture);
int chunk_manager_work(chunk_manager_t *chunk_man, int budget);
// Returns the current complied chunk idx player pos
u16 chunk_manager_update_pos(chunk_manager_t *chunk_man, s64 cx, s64 cz);

#endif