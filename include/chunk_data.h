#ifndef __CHUNK_DATA_H__
#define __CHUNK_DATA_H__

#include <tamtypes.h>

#define WORLD_HEIGHT 128

#define CHUNK_WIDTH 16
#define CHUNK_DEPTH 16
#define CHUNK_HEIGHT 16

typedef struct {
	u32 data[CHUNK_WIDTH][CHUNK_DEPTH][WORLD_HEIGHT];
	s64 x,z;
	u8 ready;
} chunkdata_t;

int chunkdata_init(chunkdata_t *data, s64 x, s64 z);
int chunkdata_ready(chunkdata_t *data);
int chunkdata_generate(chunkdata_t *data);

#endif