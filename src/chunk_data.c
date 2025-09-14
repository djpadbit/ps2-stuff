#include <chunk_data.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int chunkdata_init(chunkdata_t *data, s64 x, s64 z) {
	if (!data)
		return -1;
	data->ready = 0;
	data->x = x;
	data->z = z;
	memset(data, 0, sizeof(data->data));
	return 0;
}

int chunkdata_ready(chunkdata_t *data) {
	return data ? data->ready : 0;
}

int chunkdata_generate(chunkdata_t *data) {
	if (!data)
		return -1;

	for (u8 x=0;x<CHUNK_WIDTH;x++) {
		for (u8 z=0;z<CHUNK_DEPTH;z++) {
			float height = ((float)WORLD_HEIGHT)/2.0f + (((float)WORLD_HEIGHT)/16.0f) * (cosf(((float)data->x + x) / 16.0f) * cosf(((float)data->z + z) / 16.0f));
			int height_blk = height > 0 ? height : 1;

			memset(data->data[x][z], 3, height_blk-1);
			data->data[x][z][height_blk] = 2;
		}
	}
	data->ready = 1;

	return 0;
}