#include <blocks.h>
#include <chunk_data.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int chunkdata_init(chunkdata_t *data, s64 x, s64 z) {
	if (!data)
		return -1;
	data->xoffset = 0;
	data->x = x;
	data->z = z;
	memset(data, BLOCK_AIR, sizeof(data->data));
	return 0;
}

int chunkdata_ready(chunkdata_t *data) {
	return data ? data->xoffset == 16 : 0;
}

int chunkdata_generate(chunkdata_t *data, int budget) {
	if (!data || chunkdata_ready(data))
		return budget;

	for (;data->xoffset<CHUNK_WIDTH;data->xoffset++) {
		if (budget == 0)
			return 0;

		for (u8 z=0;z<CHUNK_DEPTH;z++) {
			float height = ((float)WORLD_HEIGHT)/2.0f + (((float)WORLD_HEIGHT)/16.0f) * (cosf(((float)data->x + data->xoffset) / 16.0f) * cosf(((float)data->z + z) / 16.0f));
			int height_blk = height > 0 ? height : 1;

			for (u8 y=0;y<height_blk;y++)
				data->data[data->xoffset][z][y] = BLOCK_DIRT;
			data->data[data->xoffset][z][height_blk] = BLOCK_GRASS;
		}

		budget--;
	}

	return budget;
}