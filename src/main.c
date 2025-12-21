
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

#include <assert.h>
#include <math3d.h>
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
#include <time.h>

#include <facing.h>
#include <chunk_data.h>
#include <chunk_manager.h>
#include <input_manager.h>
#include <renderer.h>
#include <mesh.h>
#include <mesh_quad.h>

#define SETVECTOR(v,x,y,z,w) ((v)[0]=x,(v)[1]=y,(v)[2]=z,(v)[3]=w)

renderer_t renderer;
input_manager_t input_man;
chunk_manager_t chunk_man;

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

// NTSC
//#define FRAMETIME_TARGET 16680
// PAL
#define FRAMETIME_TARGET 20000
#define US_PER_BUDGET 100
#define BUDGET_US_MARGIN 1000

static const VECTOR chunk_extends = {CHUNK_WIDTH, CHUNK_DEPTH, CHUNK_HEIGHT, 0.0f};

void render(renderer_t *rend, texture_t *texture, input_manager_t *inp, chunk_manager_t *chunks)
{
	// Camera look angles
	float verticalAngle = rend->camera_rot[0];
	float horizontalAngle = rend->camera_rot[1];
	VECTOR direction, right, up;
	direction[3] = 0.0f;
	right[1] = 0.0f;
	right[3] = 0.0f;

	// We only have one type of mesh to render so we prepare only once
	mesh_quad_prepare_renderer(rend, &chunks->chunk_mesh_type);

	printf("Start of render loop\n");

	clock_t last_time = clock();
	// The main loop...
	for (;;) {
		clock_t start_time = clock();

		// Only apply inputs if we managed to read the info
		if (inputman_read(inp) != -1) {
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

			//printf("(%.2f,%.2f) (%.2f,%.2f)\n", ljoyh, ljoyv, rjoyh, rjoyv);
			//printf("ok: %x, mode: %d, btn: %x, ljoy: (%x,%x), rjoy: (%x,%x)\n", inp->buttons.ok, inp->buttons.mode, inp->buttons.btns, inp->buttons.ljoy_h,inp->buttons.ljoy_v, inp->buttons.rjoy_h,inp->buttons.rjoy_v);
		}

		// Moved the camera, so update the matrix
		renderer_update_matrices(rend);
		renderer_clear(rend);

		// Update position for the chunk mananger
		chunk_manager_update_pos(chunks, rend->camera_pos[0], rend->camera_pos[2]);
		clock_t start_render = clock();

		u16 cntFrust = 0;
		u16 cntnFrsut = 0;
		u32 triCnt = 0;
		// Draw all the visible chunks
		for (int i=0;i<CHUNK_COMPILED_COUNT;i++) {
			// Not compiled or empty
			if (chunk_manager_chunk_empty(chunks, i))
				continue;

			u8 in_frust = renderer_check_box_frustum(rend, chunks->meshes[i].pos, (float*)chunk_extends);
			//chunks->meshes[i].color.val = in_frust ? 0x80008000 : 0x80000080;
			if (in_frust)
				cntFrust++;
			else {
				cntnFrsut++;
				continue;
			}

			mesh_quad_draw(&chunks->meshes[i], rend);
			triCnt += chunks->meshes[i].vert_count / 2;
		}

		clock_t render_end = clock();

		int budget = FRAMETIME_TARGET - (render_end-start_time) - BUDGET_US_MARGIN; // Keep margin
		// Never fully stop working
		if (budget <= 0)
			budget = US_PER_BUDGET;

		// Work a little bit on the chunks and stuff
		int start_budget = budget/US_PER_BUDGET;
		budget = chunk_manager_work(chunks, start_budget);

		clock_t end_time = clock();

		long usperbudget = start_budget - budget;
		if (usperbudget != 0)
			usperbudget = (end_time-render_end) / usperbudget;

		printf("%s%ldus / %ldus (%ldus/%ldus/%ldus) (%.2f,%.2f,%.2f) (%.2f,%.2f,%.2f) %d/%d (%ldus/budget) %d/%d %dt\n", end_time-start_time > FRAMETIME_TARGET ? "!!OVERLOAD!! " : "",
											end_time-start_time, start_time-last_time, start_render-start_time, render_end-start_render, end_time-render_end,
											rend->camera_pos[0], rend->camera_pos[1], rend->camera_pos[2],
											DEGS(rend->camera_rot[0]), DEGS(rend->camera_rot[1]), DEGS(rend->camera_rot[2]),
											budget, start_budget, usperbudget, cntFrust, cntnFrsut, triCnt);

		last_time = start_time;

		graph_wait_vsync();
	}
}

int main(int argc, char *argv[])
{
	// Init DMA channels.
	dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
	dma_channel_initialize(DMA_CHANNEL_VIF1, NULL, 0);
	//dma_channel_initialize(DMA_CHANNEL_VIF0, NULL, 0);
	dma_channel_fast_waits(DMA_CHANNEL_GIF);
	dma_channel_fast_waits(DMA_CHANNEL_VIF1);
	//dma_channel_fast_waits(DMA_CHANNEL_VIF0);

	// Init the SIF RPC
	sceSifInitRpc(0);

	// Load Input related modules
	inputman_load_iop();

	// Init the input mananger
	inputman_init(&input_man, 0, 0);

	// Init the renderer (VIF Packets & clear packet allocations)
	renderer_init(&renderer);

	SETVECTOR(renderer.camera_pos, CHUNK_WIDTH/2.0f, 240.0f, CHUNK_DEPTH/2.0f, 1.00f);
	SETVECTOR(renderer.camera_rot, M_PIf/2.0f, M_PIf, M_PIf, 1.00f); // flip upside down... (PI in z)

	renderer_setup(&renderer, 640, 512);

	// Init texture
	texture_t texture;
	renderer_init_texture(&texture, 256, GS_PSM_24, terrain);

	// Init all the chunks
	chunk_manager_init(&chunk_man, &texture);

	// Main Loop
	render(&renderer, &texture, &input_man, &chunk_man);

	// Sleep
	SleepThread();

	// End program.
	return 0;
}
