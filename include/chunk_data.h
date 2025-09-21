#ifndef __CHUNK_DATA_H__
#define __CHUNK_DATA_H__

#include <tamtypes.h>

#define WORLD_HEIGHT 128

#define CHUNK_WIDTH 16
#define CHUNK_DEPTH 16
#define CHUNK_HEIGHT 16

#define CHUNK_CLIP_POS(x) (((x)>>4)<<4)
#define CHUNK_POS(x) ((x)>>4)

typedef struct {
	u32 data[CHUNK_WIDTH][CHUNK_DEPTH][WORLD_HEIGHT];
	s64 x,z;
	u8 xoffset;
} chunkdata_t;

int chunkdata_init(chunkdata_t *data, s64 x, s64 z);
int chunkdata_ready(chunkdata_t *data);
// Returns the remaining budget, budget can be -1 for U32_MAX max ops
int chunkdata_generate(chunkdata_t *data, int budget);

#endif