#include "blocks.h"
#include "facing.h"
#include "qdisp.h"
#include <assert.h>
#include <chunk_manager.h>
#include <chunk_data.h>
#include <stdio.h>
#include <string.h>

static inline u8 chunk_manager_chunk_ready_inner(chunk_manager_t *chunk_man, u16 compiled_idx) {
	return chunk_man->compiled_chunks[compiled_idx].progress == CHUNK_DEPTH*CHUNK_WIDTH;
}

u8 chunk_manager_chunk_ready(chunk_manager_t *chunk_man, u16 compiled_idx) {
	if (!chunk_man || compiled_idx >= CHUNK_COMPILED_COUNT)
		return 0;
	return chunk_manager_chunk_ready_inner(chunk_man, compiled_idx);
}

u8 chunk_manager_chunk_empty(chunk_manager_t *chunk_man, u16 compiled_idx) {
	if (!chunk_man || compiled_idx >= CHUNK_COMPILED_COUNT)
		return 0;
	return chunk_man->compiled_chunks[compiled_idx].progress != CHUNK_DEPTH*CHUNK_WIDTH || chunk_man->compiled_chunks[compiled_idx].size == 0;
}

static chunkdata_t* chunk_manager_find_data(chunk_manager_t *chunk_man, s64 x, s64 z) {
	if (!chunk_man)
		return NULL;

	for (int i=0;i<CHUNK_DATA_COUNT;i++) {
		if (chunk_man->chunks[i].x == x && chunk_man->chunks[i].z == z)
			return &chunk_man->chunks[i]; 
	}

	return NULL;
}

static int chunk_manager_find_compiled_idx(chunk_manager_t *chunk_man, s64 x, s64 z) {
	if (!chunk_man)
		return -1;

	for (int i=0;i<CHUNK_COMPILED_COUNT_PLANE;i++) {
		if (chunk_man->compiled_pos[i].x == x && chunk_man->compiled_pos[i].z == z)
			return i; 
	}

	return -1;
}

static int chunk_manager_find_data_idx(chunk_manager_t *chunk_man, s64 x, s64 z) {
	if (!chunk_man)
		return -1;

	for (int i=0;i<CHUNK_DATA_COUNT;i++) {
		if (chunk_man->chunks[i].x == x && chunk_man->chunks[i].z == z)
			return i; 
	}

	return -1;
}


static inline u8 chunk_manager_should_not_render(u8 idx) {
	return idx == BLOCK_AIR;
}

static inline u8 chunk_manager_is_transparent(u8 idx) {
	return idx == BLOCK_AIR;
}

static inline u8 chunk_manager_get_texid(u8 idx, u8 other_idx, enum facing face) {
	switch (idx) {
		case BLOCK_STONE:
			return 1;
		case BLOCK_GRASS:
			return face == FC_UP ? 0 : face == FC_DOWN ? 2 : 3;
		case BLOCK_DIRT:
			return 2;
		default:
			return 14;
	}
	return 14;
}

static int chunk_manager_compile(chunk_manager_t *chunk_man, int chunk_comp_idx, int budget) {
	if (!chunk_man)
		return budget;

	s64 cx = chunk_man->compiled_pos[chunk_comp_idx/CHUNK_COMPILED_YCOUNT].x;
	s64 cz = chunk_man->compiled_pos[chunk_comp_idx/CHUNK_COMPILED_YCOUNT].z;
	int ycomp_offset = (chunk_comp_idx % CHUNK_COMPILED_YCOUNT);
	int yoff = ycomp_offset * CHUNK_HEIGHT;

	chunkdata_t *cdat = chunk_manager_find_data(chunk_man, cx, cz);

	if (!chunkdata_ready(cdat))
		return budget;

	chunkdata_t *cdat_xp = chunk_manager_find_data(chunk_man, cx+CHUNK_WIDTH, cz);
	chunkdata_t *cdat_xn = chunk_manager_find_data(chunk_man, cx-CHUNK_WIDTH, cz);
	chunkdata_t *cdat_zp = chunk_manager_find_data(chunk_man, cx, cz+CHUNK_DEPTH);
	chunkdata_t *cdat_zn = chunk_manager_find_data(chunk_man, cx, cz-CHUNK_DEPTH);

	//printf("(%lld, %lld) -> %p %p %p %p\n", x, z, cdat_xp, cdat_xn, cdat_zp, cdat_zn);

	if (!chunkdata_ready(cdat_xp) || !chunkdata_ready(cdat_xn) ||
		!chunkdata_ready(cdat_zp) || !chunkdata_ready(cdat_zn))
		return budget;

	u32 vsize = chunk_man->compiled_chunks[chunk_comp_idx].size;

	u16 progress;
	for (progress = chunk_man->compiled_chunks[chunk_comp_idx].progress; progress < CHUNK_DEPTH*CHUNK_WIDTH; progress++) {
		if (budget <= 0)
			break;
		u8 x = progress/CHUNK_DEPTH;
		u8 z = progress%CHUNK_DEPTH;
		for (u8 y=0; y<CHUNK_HEIGHT; y++) {
			u8 ry = yoff+y;
			u8 cblock = cdat->data[x][z][ry];
			if (chunk_manager_should_not_render(cblock))
				continue;

			u8 cblock_yp = ry == WORLD_HEIGHT-1 ? BLOCK_AIR                           : cdat->data[x][z][ry+1];
			u8 cblock_yn = ry == 0              ? BLOCK_BEDROCK                       : cdat->data[x][z][ry-1];
			u8 cblock_xp = x  == CHUNK_WIDTH-1  ? cdat_xp->data[0][z][ry]             : cdat->data[x+1][z][ry];
			u8 cblock_xn = x  == 0              ? cdat_xn->data[CHUNK_WIDTH-1][z][ry] : cdat->data[x-1][z][ry];
			u8 cblock_zp = z  == CHUNK_DEPTH-1  ? cdat_zp->data[x][0][ry]             : cdat->data[x][z+1][ry];
			u8 cblock_zn = z  == 0              ? cdat_zn->data[x][CHUNK_DEPTH-1][ry] : cdat->data[x][z-1][ry];

			qvert_t pos;
			pos.x = x<<4;
			pos.y = y<<4;
			pos.z = z<<4;
			pos.w = 0;

			/*if (cblock == BLOCK_GRASS) {
				printf("[%d] (%d;%d(%d);%d) cblock_xp=%d, cblock_xn=%d, cblock_yp=%d, cblock_yn=%d, cblock_zp=%d, cblock_zn=%d, \n", chunk_comp_idx, x, y, ry, z, cblock_xp, cblock_xn, cblock_yp, cblock_yn, cblock_zp, cblock_zn);
			}*/

			if (chunk_manager_is_transparent(cblock_yp))
				vsize += qdisp_put_dir(&chunk_man->compiled_chunks[chunk_comp_idx].verts[vsize], pos, FC_UP, chunk_manager_get_texid(cblock, cblock_yp, FC_UP));
			if (chunk_manager_is_transparent(cblock_yn))
				vsize += qdisp_put_dir(&chunk_man->compiled_chunks[chunk_comp_idx].verts[vsize], pos, FC_DOWN, chunk_manager_get_texid(cblock, cblock_yn, FC_DOWN));

			if (chunk_manager_is_transparent(cblock_xp))
				vsize += qdisp_put_dir(&chunk_man->compiled_chunks[chunk_comp_idx].verts[vsize], pos, FC_RIGHT, chunk_manager_get_texid(cblock, cblock_xp, FC_RIGHT));
			if (chunk_manager_is_transparent(cblock_xn))
				vsize += qdisp_put_dir(&chunk_man->compiled_chunks[chunk_comp_idx].verts[vsize], pos, FC_LEFT, chunk_manager_get_texid(cblock, cblock_xn, FC_LEFT));

			if (chunk_manager_is_transparent(cblock_zn))
				vsize += qdisp_put_dir(&chunk_man->compiled_chunks[chunk_comp_idx].verts[vsize], pos, FC_BACK, chunk_manager_get_texid(cblock, cblock_zn, FC_BACK));
			if (chunk_manager_is_transparent(cblock_zp))
				vsize += qdisp_put_dir(&chunk_man->compiled_chunks[chunk_comp_idx].verts[vsize], pos, FC_FRONT, chunk_manager_get_texid(cblock, cblock_zp, FC_FRONT));

			assert(vsize <= CHUNK_COMPILED_MAX_VERTS);
		}
		budget--;
	}

	chunk_man->compiled_chunks[chunk_comp_idx].size = vsize;
	chunk_man->compiled_chunks[chunk_comp_idx].progress = progress;

	if (chunk_manager_chunk_ready_inner(chunk_man, chunk_comp_idx)) {
		mesh_update(&chunk_man->meshes[chunk_comp_idx], chunk_man->compiled_chunks[chunk_comp_idx].size, (qvert_t*)&chunk_man->compiled_chunks[chunk_comp_idx]);
		chunk_man->meshes[chunk_comp_idx].pos[0] = cx;
		chunk_man->meshes[chunk_comp_idx].pos[1] = yoff;
		chunk_man->meshes[chunk_comp_idx].pos[2] = cz;
		printf("%d %d %d %d\n", vsize, cx, yoff, cz);
	}

	return budget;
}

void chunk_manager_init(chunk_manager_t *chunk_man, texbuffer_t *texture) {
	if (!chunk_man)
		return;

	memset(chunk_man, 0, sizeof(chunk_manager_t));

	// Generate all the chunks
	printf("Init chunks\n");
	for (int i=0;i<CHUNK_DATA_COUNT;i++) {
		s64 x = ((i%(CHUNK_VIEWDIST+2)) - ((CHUNK_VIEWDIST+2)/2)) * CHUNK_WIDTH;
		s64 z = ((i/(CHUNK_VIEWDIST+2)) - ((CHUNK_VIEWDIST+2)/2)) * CHUNK_DEPTH;
		printf("D%d (%lld,%lld)\n", i, x, z);
		chunkdata_init(&chunk_man->chunks[i], x, z);
		if (i < CHUNK_COMPILED_COUNT_PLANE) {
			x = ((i%(CHUNK_VIEWDIST)) - ((CHUNK_VIEWDIST)/2)) * CHUNK_WIDTH;
			z = ((i/(CHUNK_VIEWDIST)) - ((CHUNK_VIEWDIST)/2)) * CHUNK_DEPTH;
			printf("C%d (%lld,%lld)\n", i, x, z);
			chunk_man->compiled_pos[i].x = x;
			chunk_man->compiled_pos[i].z = z;
		}
	}

	// Init meshes
	for (int i=0;i<CHUNK_COMPILED_COUNT;i++) {
		mesh_init(&chunk_man->meshes[i], 0, NULL);
		mesh_set_quad_prim(&chunk_man->meshes[i], texture);
	}
}

int chunk_manager_work(chunk_manager_t *chunk_man, int budget) {
	for (int i=0; i<CHUNK_DATA_COUNT; i++) {
		if (chunkdata_ready(&chunk_man->chunks[i]))
			continue;

		budget = chunkdata_generate(&chunk_man->chunks[i], budget);

		if (budget == 0)
			return 0;
	}

	for (int i=0; i<CHUNK_COMPILED_COUNT; i++) {
		if (chunk_manager_chunk_ready_inner(chunk_man, i))
			continue;

		budget = chunk_manager_compile(chunk_man, i, budget);

		if (budget == 0)
			return 0;
	}

	return budget;
}

void chunk_manager_update_pos(chunk_manager_t *chunk_man, s64 x, s64 z) {
	if (!chunk_man)
		return;
	return;
	x = (x/CHUNK_WIDTH)*CHUNK_WIDTH;
	z = (z/CHUNK_DEPTH)*CHUNK_DEPTH;

	if (x == chunk_man->old_x && z == chunk_man->old_z)
		return;

	chunk_man->old_x = x;
	chunk_man->old_z = z;

	u16 comp_needed[CHUNK_COMPILED_COUNT];
	u16 data_needed[CHUNK_DATA_COUNT];

	for (int i=0;i<CHUNK_DATA_COUNT;i++) {
		data_needed[i] = 0;
		if (i < CHUNK_COMPILED_COUNT_PLANE)
			comp_needed[i] = 0;
	}

	for (int i=0;i<CHUNK_DATA_COUNT;i++) {
		x = chunk_man->old_x + ((i%(CHUNK_VIEWDIST+2)) - ((CHUNK_VIEWDIST+2)/2)) * CHUNK_WIDTH;
		z = chunk_man->old_z + ((i/(CHUNK_VIEWDIST+2)) - ((CHUNK_VIEWDIST+2)/2)) * CHUNK_DEPTH;

		int cdat_idx = chunk_manager_find_data_idx(chunk_man, x, z);
		if (cdat_idx != -1)
			data_needed[cdat_idx] = 1; 

		if (i < CHUNK_COMPILED_COUNT_PLANE) {
			x = chunk_man->old_x + ((i%(CHUNK_VIEWDIST)) - ((CHUNK_VIEWDIST)/2)) * CHUNK_WIDTH;
			z = chunk_man->old_z + ((i/(CHUNK_VIEWDIST)) - ((CHUNK_VIEWDIST)/2)) * CHUNK_DEPTH;

			int ccomp_idx = chunk_manager_find_compiled_idx(chunk_man, x, z);
			if (ccomp_idx != -1)
				comp_needed[ccomp_idx] = 1;
		}
	}

	u16 cmp_idx = 0;
	u16 data_idx = 0;

	for (int i=0;i<CHUNK_DATA_COUNT;i++) {
		x = chunk_man->old_x + ((i%(CHUNK_VIEWDIST+2)) - ((CHUNK_VIEWDIST+2)/2)) * CHUNK_WIDTH;
		z = chunk_man->old_z + ((i/(CHUNK_VIEWDIST+2)) - ((CHUNK_VIEWDIST+2)/2)) * CHUNK_DEPTH;

		int cdat_idx = chunk_manager_find_data_idx(chunk_man, x, z);
		if (cdat_idx == -1) {
			for (;data_idx<CHUNK_DATA_COUNT;data_idx++) {
				if (!data_needed[data_idx])
					break;
			}

			//printf("D%d (%lld,%lld) %d\n", i, x, z, data_idx);
			chunkdata_init(&chunk_man->chunks[data_idx], x, z);
			data_idx++;
		}

		if (i < CHUNK_COMPILED_COUNT_PLANE) {
			x = chunk_man->old_x + ((i%(CHUNK_VIEWDIST)) - ((CHUNK_VIEWDIST)/2)) * CHUNK_WIDTH;
			z = chunk_man->old_z + ((i/(CHUNK_VIEWDIST)) - ((CHUNK_VIEWDIST)/2)) * CHUNK_DEPTH;

			int ccomp_idx = chunk_manager_find_compiled_idx(chunk_man, x, z);
			if (ccomp_idx == -1) {
				for (;cmp_idx<CHUNK_DATA_COUNT;cmp_idx++) {
					if (!comp_needed[cmp_idx])
						break;
				}

				//printf("C%d (%lld,%lld) %d\n", i, x, z, cmp_idx);
				chunk_man->compiled_pos[cmp_idx].x = x;
				chunk_man->compiled_pos[cmp_idx].z = z;
				for (int j=0;j<CHUNK_COMPILED_YCOUNT;j++) {
					int cidx = (cmp_idx*CHUNK_COMPILED_YCOUNT)+j;
					chunk_man->compiled_chunks[cidx].size = 0;
					chunk_man->compiled_chunks[cidx].progress = 0;
					mesh_update(&chunk_man->meshes[cidx], 0, NULL);
				}
				cmp_idx++;
			}
		}
	}
}