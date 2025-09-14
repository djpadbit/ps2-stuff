/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (c) 2020 h4570 Sandro Sobczyński <sandro.sobczynski@gmail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
# VU1 and libpacket2 showcase.
*/

#include "chunk_data.h"
#include "input_manager.h"
#include "math3d.h"
#include <math.h>
#include <stdio.h>
#include <kernel.h>
#include <malloc.h>
#include <string.h>
#include <tamtypes.h>
#include <gs_psm.h>
#include <dma.h>
#include <packet2.h>
#include <packet2_utils.h>
#include <packet2_extras.h>
#include <graph.h>
#include <draw.h>
#include <resources.h>
#include <sifrpc.h>

#include <renderer.h>
#include <mesh.h>
#include <qdisp.h>

#define SETVECTOR(v,x,y,z,w) ((v)[0]=x,(v)[1]=y,(v)[2]=z,(v)[3]=w)

/** 
 * Data of VU1 micro program (draw_3D.vcl/vsm). 
 * How we can use it: 
 * 1. Upload program to VU1. 
 * 2. Send calculated local_screen matrix once per mesh (3D object) 
 * 3. Set buffers size. (double-buffering described below) 
 * 4. Send packet with: lod, clut, tex buffer, scale vector, rgba, verts and sts. 
 * What this program is doing? 
 * 1. Load local_screen. 
 * 2. Zero clipping flag. 
 * 3. Set current buffer start address from TOP register (xtop command) 
 *      To use pararelism, we set two buffers in the VU1. It means, that when 
 *      VU1 is working with one verts packet, we can load second one into another buffer. 
 *      xtop command is automatically switching buffers. I think that AAA games used 
 *      quad buffers (TOP+TOPS) which can give best performance and no VIF_FLUSH should be needed. 
 * 4. Load rest of data. 
 * 5. Prepare GIF tag. 
 * 6. For every vertex: transform, clip, scale, perspective divide. 
 * 7. Send it to GS via XGKICK command. 
 */
extern u32 VU1Draw3D_CodeStart __attribute__((section(".vudata")));
extern u32 VU1Draw3D_CodeEnd __attribute__((section(".vudata")));

renderer_t renderer;
mesh_t cube_mesh;
input_manager_t input_man;

#define CHUNK_VIEWDIST 4
// Account for border chunks
chunkdata_t chunks[(CHUNK_VIEWDIST+1)*(CHUNK_VIEWDIST+1)];

static inline float remap_stick(u8 input) {
	if (input > 127-10 && input <= 127+10)
		return 0.0f;

	float val = ((float)(input-127.0f));

	if (input > 0x7F)
		return val/128.0f;
	return val/127.0f;
}

#define DEGS(val) ((val)*(180.0f/3.14f))
#define M_PIf		3.14159265358979323846f

void render(renderer_t *rend, texbuffer_t *texbuff, input_manager_t *inp)
{
	int cube_cnt = 1000;
	mesh_create(&cube_mesh, 4*6*cube_cnt);

	mesh_set_quad_prim(&cube_mesh, texbuff);

	u32 vert_idx = 0;
	qvert_t pos = {{0,0,0,0}};
	for (int i = 0; i < cube_cnt; i++) {
		pos.x = (i % 32)*20;
		pos.y = (i / 32)*20;
		for (enum facing face = 0; face < FC_END; face++)
			vert_idx += qdisp_put_dir(cube_mesh.vertices + vert_idx, pos, face, face == FC_UP ? 0 : face == FC_DOWN ? 2 : 3);
	}
	printf("Final vert count: %d\n", vert_idx);
	printf("memory size: %d\n", cube_cnt*4*6*sizeof(qvert_t));

	// Camera look angles
	float verticalAngle = 0.0f;
	float horizontalAngle = M_PIf;
	VECTOR direction, right, up;
	direction[3] = 0.0f;
	right[1] = 0.0f;
	right[3] = 0.0f;

	// The main loop...
	for (;;) {
		// Spin the cube a bit.
		/*cube_mesh.rot[0] += 0.008f;
		while (cube_mesh.rot[0] > 3.14f)
			cube_mesh.rot[0] -= 6.28f;

		cube_mesh.rot[1] += 0.012f;
		while (cube_mesh.rot[1] > 3.14f)
			cube_mesh.rot[1] -= 6.28f;

		rend->camera_pos[2] += .5F;
		rend->camera_rot[2] += 0.002f;

		if (rend->camera_pos[2] >= 400.0F) {
			rend->camera_pos[2] = 40.0F;
			rend->camera_rot[2] = 0.00f;
		}*/

		inputman_read(inp);

		float rjoyv = remap_stick(inp->buttons.rjoy_v);
		float rjoyh = remap_stick(inp->buttons.rjoy_h);

		float ljoyv = remap_stick(inp->buttons.ljoy_v);
		float ljoyh = remap_stick(inp->buttons.ljoy_h);

		float speed = 1.0f;
		if (!(inp->buttons.btns & PAD_R2))
			speed += 2.0f;
		if (!(inp->buttons.btns & PAD_L2))
			speed -= 0.5f;

		horizontalAngle -= (1.0f/60.0f) * speed * rjoyh;
		verticalAngle   += (1.0f/60.0f) * speed * rjoyv;

		while (horizontalAngle > M_PIf)
			horizontalAngle -= 2.0f*M_PIf;
		while (horizontalAngle < -M_PIf)
			horizontalAngle += 2.0f*M_PIf;

		while (verticalAngle > M_PIf)
			verticalAngle -= 2.0f*M_PIf;
		while (verticalAngle < -M_PIf)
			verticalAngle += 2.0f*M_PIf;

		direction[0] = cosf(verticalAngle) * sinf(-horizontalAngle);
		direction[1] = sinf(verticalAngle);
		direction[2] = cosf(verticalAngle) * cosf(-horizontalAngle);

		right[0] = sin(-horizontalAngle - (M_PIf/2.0f));
		right[2] = cos(-horizontalAngle - (M_PIf/2.0f));

		vector_cross_product(up, right, direction);

		rend->camera_rot[0] = verticalAngle;
		rend->camera_rot[1] = horizontalAngle;

		rend->camera_pos[0] += direction[0] * (ljoyv * speed) + right[0] * (ljoyh * speed);
		rend->camera_pos[1] += direction[1] * (ljoyv * speed) + right[1] * (ljoyh * speed);
		rend->camera_pos[2] += direction[2] * (ljoyv * speed) + right[2] * (ljoyh * speed);

		//printf("(%.2f,%.2f,%.2f) (%.2f,%.2f,%.2f)\n", rend->camera_pos[0], rend->camera_pos[1], rend->camera_pos[2],
		//									DEGS(rend->camera_rot[0]), DEGS(rend->camera_rot[1]), DEGS(rend->camera_rot[2]));

		//printf("(%.2f,%.2f) (%.2f,%.2f)\n", ljoyh, ljoyv, rjoyh, rjoyv);
		//printf("btn: %x, ljoy: (%x,%x), rjoy: (%x,%x)\n", inp->buttons.btns, inp->buttons.ljoy_h,inp->buttons.ljoy_v, inp->buttons.rjoy_h,inp->buttons.rjoy_v);

		// Moved the camera, so update the matrix
		renderer_update_matrices(rend);
		renderer_clear(rend);

		for (int i = 0; i < 1; i++) {
			cube_mesh.pos[0] = i * 40.0F;
			for (int j = 0; j < 5; j++) {
				cube_mesh.pos[1] = j * 40.0F;
				mesh_draw(&cube_mesh, rend);
			}
		}

		graph_wait_vsync();
	}
}

int main(int argc, char *argv[])
{
	// Init DMA channels.
	dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
	dma_channel_initialize(DMA_CHANNEL_VIF1, NULL, 0);
	dma_channel_fast_waits(DMA_CHANNEL_GIF);
	dma_channel_fast_waits(DMA_CHANNEL_VIF1);

	// Init the SIF RPC
	sceSifInitRpc(0);

	// Load Input related modules
	inputman_load_iop();

	// Init the input mananger
	inputman_init(&input_man, 0, 0);

	// Init the renderer (VIF Packets & clear packet allocations)
	renderer_init(&renderer);

	// Upload VU1 quad code
	renderer_upload_vu1(&renderer, &VU1Draw3D_CodeStart, &VU1Draw3D_CodeEnd, 8, 496);

	SETVECTOR(renderer.camera_pos, 0.00f, 0.00f, -40.00f, 1.00f);
	SETVECTOR(renderer.camera_rot, 0.00f, 0.00f, M_PIf, 1.00f); // flip upside down...

	renderer_setup(&renderer, 640, 512);

	// Load the texture into vram.
	texbuffer_t texture;
	renderer_load_texture(&texture, 256, GS_PSM_32, terrain);

	// Generate all the chunks
	printf("Generating chunks\n");
	for (int i=0;i<(CHUNK_VIEWDIST+1)*(CHUNK_VIEWDIST+1);i++) {
		s64 x = ((i%(CHUNK_VIEWDIST+1)) - ((CHUNK_VIEWDIST+1)/2)) * CHUNK_WIDTH;
		s64 z = ((i/(CHUNK_VIEWDIST+1)) - ((CHUNK_VIEWDIST+1)/2)) * CHUNK_DEPTH;
		printf("%d (%lld,%lld)\n", i, x, z);
		chunkdata_init(&chunks[i], x, z);
		chunkdata_generate(&chunks[i]);
	}

	render(&renderer, &texture, &input_man);

	// Sleep
	SleepThread();

	// End program.
	return 0;
}
