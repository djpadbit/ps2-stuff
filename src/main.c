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
//mesh_t cube_mesh;
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

static const VECTOR chunk_extends = {CHUNK_WIDTH, CHUNK_DEPTH, CHUNK_HEIGHT, 0.0f};

#define CHUNK_QUEUE_SIZE (CHUNK_COMPILED_COUNT_PLANE * (CHUNK_COMPILED_YCOUNT+2) + 1)

struct chunkinfo {
	s64 x, z;
	u16 idx;
	s16 y;
	enum facing from;
	u8 faces_seen;
};

#define FACE_SEEN(faces, face) (((faces) >> (face))&1)
#define FACE_SEE(face) (1 << (face))

static struct chunkinfo __attribute__((aligned(128))) chunk_queue[CHUNK_QUEUE_SIZE];
static u8 __attribute__((aligned(128))) chunk_visited[CHUNK_COMPILED_COUNT_PLANE];

#define CHUNK_TRAVERSED(x,y,z) (((chunk_visited[x/CHUNK_WIDTH + z/CHUNK_DEPTH] >> y)&1))
#define CHUNK_TRAVERSE(x,y,z) (chunk_visited[x/CHUNK_WIDTH + z/CHUNK_DEPTH] |= 1 << y)

static u16 queue_widx;
static u16 queue_ridx;

void add_chunk(s64 x, s16 y, s64 z, u16 idx, enum facing from, u8 faces_seen) {
	u16 new_idx = ((queue_widx+1)%CHUNK_QUEUE_SIZE);
	assert(new_idx != queue_ridx);
	chunk_queue[queue_widx].x = x;
	chunk_queue[queue_widx].z = z;
	chunk_queue[queue_widx].idx = idx;
	chunk_queue[queue_widx].y = y;
	chunk_queue[queue_widx].from = from;
	chunk_queue[queue_widx].faces_seen = faces_seen;
	queue_widx = new_idx;
}

int pop_chunk(struct chunkinfo* info) {
	if (queue_ridx == queue_widx)
		return -1;
	//memccpy(info, &chunk_queue[queue_ridx++], sizeof(struct chunkinfo), 1);
	info->x = chunk_queue[queue_ridx].x;
	info->z = chunk_queue[queue_ridx].z;
	info->idx = chunk_queue[queue_ridx].idx;
	info->y = chunk_queue[queue_ridx].y;
	info->from = chunk_queue[queue_ridx].from;
	info->faces_seen = chunk_queue[queue_ridx].faces_seen;
	queue_ridx++;
	return 0;
}

u16 queue_size() {
	if (queue_ridx == queue_widx)
		return 0;
	if (queue_ridx < queue_widx)
		return queue_widx - queue_ridx;
	return queue_widx + CHUNK_QUEUE_SIZE - queue_ridx;
}

void render(renderer_t *rend, texbuffer_t *texbuff, input_manager_t *inp, chunk_manager_t *chunks)
{
	/*int cube_cnt = 1000;
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
	printf("memory size: %d\n", cube_cnt*4*6*sizeof(qvert_t));*/

	// Camera look angles
	float verticalAngle = 0.0f;
	float horizontalAngle = M_PIf;
	VECTOR direction, right, up;
	direction[3] = 0.0f;
	right[1] = 0.0f;
	right[3] = 0.0f;

	printf("Start of render loop\n");

	clock_t last_time = clock();
	// The main loop...
	for (;;) {
		clock_t start_time = clock();
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
			//printf("btn: %x, ljoy: (%x,%x), rjoy: (%x,%x)\n", inp->buttons.btns, inp->buttons.ljoy_h,inp->buttons.ljoy_v, inp->buttons.rjoy_h,inp->buttons.rjoy_v);
		}

		// Moved the camera, so update the matrix
		renderer_update_matrices(rend);
		renderer_clear(rend);

		// Update position for the chunk mananger
		u16 base_compidx = chunk_manager_update_pos(chunks, rend->camera_pos[0], rend->camera_pos[2]);
		clock_t start_render = clock();

		int cntFrust = 0;
		int cntnFrsut = 0;
		// Draw all the visible chunks
		/*for (int i=0;i<CHUNK_COMPILED_COUNT;i++) {
			// Not compiled or empty
			if (chunk_manager_chunk_empty(&chunk_man, i))
				continue;

			u8 in_frust = renderer_check_box_frustum(rend, chunks->meshes[i].pos, (float*)chunk_extends);
			//chunks->meshes[i].color.val = in_frust ? 0x80008000 : 0x80000080;
			if (in_frust)
				cntFrust++;
			else {
				cntnFrsut++;
				continue;
			}

			mesh_draw(&chunks->meshes[i], rend);
		}*/

		queue_widx = 0;
		queue_ridx = 0;
		memset(chunk_visited, 0, sizeof(chunk_visited));

		s64 chunkx = (((s64)rend->camera_pos[0])/CHUNK_WIDTH) * CHUNK_WIDTH;
		s16 chunkyoff = (((s64)rend->camera_pos[1])/CHUNK_HEIGHT);
		if (chunkyoff > CHUNK_COMPILED_YCOUNT)
			chunkyoff = CHUNK_COMPILED_YCOUNT;
		if (chunkyoff < -1)
			chunkyoff = -1;
		s16 chunky = chunkyoff * CHUNK_HEIGHT;
		s64 chunkz = (((s64)rend->camera_pos[2])/CHUNK_DEPTH) * CHUNK_DEPTH;


		add_chunk(chunkx, chunkyoff, chunkz, base_compidx, FC_END, 0);

		struct chunkinfo info;
		VECTOR chunk_pos;
		chunk_pos[3] = 0.0f;

		while (queue_size()) {
			if (pop_chunk(&info) == -1)
				assert(0);

			chunk_pos[0] = info.x;
			chunk_pos[1] = info.y * CHUNK_HEIGHT;
			chunk_pos[2] = info.z;

			u16 comp_idx = info.idx + info.y;
			u16 vis = 0xFF;

			printf("(%lld,%d,%lld) - %d,%d,%d\n", info.x, info.y, info.z, info.idx, info.from, info.faces_seen);

			// Draw if we can & get visibility info
			if (info.y >= 0 && info.y < CHUNK_COMPILED_YCOUNT && !chunk_manager_chunk_empty(&chunk_man, comp_idx)) {
				vis = chunks->compiled_chunks[comp_idx].chunk_vis;
				mesh_draw(&chunks->meshes[comp_idx], rend);
			}

			for (enum facing to = 0;to<FC_END;to++) {
				if (to == info.from)
					continue;
				s64 newx = info.x + facing_dir[to][0] * CHUNK_WIDTH;
				s64 newz = info.z + facing_dir[to][2] * CHUNK_DEPTH;
				s16 newy = info.y + facing_dir[to][1];

				// Check if we're going backwards
				if (FACE_SEEN(info.faces_seen, facing_opposite[info.from]))
					continue;

				// We've already seen this one
				if (CHUNK_TRAVERSED(newx, newy, newz))
					continue;

				int cidx = chunk_manager_find_compiled_idx(&chunk_man, newx, newz);

				// Outside of the chunks we have
				if (newy < -1 || newy > CHUNK_COMPILED_YCOUNT || cidx == -1)
					continue;

				if (!chunk_manager_check_vis(vis, info.from, to))
					continue;

				u8 in_frust = renderer_check_box_frustum(rend, chunk_pos, (float*)chunk_extends);
				//chunks->meshes[i].color.val = in_frust ? 0x80008000 : 0x80000080;
				if (in_frust)
					cntFrust++;
				else {
					cntnFrsut++;
					continue;
				}
			}
		}

		clock_t render_end = clock();

		static const clock_t frametime_target = 16680;

		int budget = frametime_target - (render_end-start_time) - 1000; // Keep 1ms margin
		if (budget < 0)
			budget = 0;

		// Work a little bit on the chunks and stuff
		int start_budget = budget/100;
		budget = chunk_manager_work(chunks, start_budget);

		clock_t end_time = clock();

		long usperbudget = start_budget - budget;
		if (usperbudget != 0)
			usperbudget = (end_time-render_end) / usperbudget;

		printf("%ldus / %ldus (%ldus/%ldus/%ldus) (%.2f,%.2f,%.2f) (%.2f,%.2f,%.2f) %d/%d (%ldus/budget) %d/%d\n", end_time-start_time, start_time-last_time, start_render-start_time,
											render_end-start_render, end_time-render_end, rend->camera_pos[0], rend->camera_pos[1], rend->camera_pos[2],
											DEGS(rend->camera_rot[0]), DEGS(rend->camera_rot[1]), DEGS(rend->camera_rot[2]), budget, start_budget, usperbudget, cntFrust, cntnFrsut);

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

	// Upload VU1 quad code
	renderer_upload_vu1(&renderer, &VU1Draw3D_CodeStart, &VU1Draw3D_CodeEnd, 8, 496);

	SETVECTOR(renderer.camera_pos, 0.00f, WORLD_HEIGHT, 0.00f, 1.00f);
	SETVECTOR(renderer.camera_rot, 0.00f, 0.00f, M_PIf, 1.00f); // flip upside down...

	renderer_setup(&renderer, 640, 448);

	// Load the texture into vram.
	texbuffer_t texture;
	renderer_load_texture(&texture, 256, GS_PSM_32, terrain);

	// Init all the chunks
	chunk_manager_init(&chunk_man, &texture);

	render(&renderer, &texture, &input_man, &chunk_man);

	// Sleep
	SleepThread();

	// End program.
	return 0;
}
