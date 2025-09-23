#include "qdisp.h"
#include <renderer.h>
#include <mesh.h>

#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <draw.h>
#include <packet2.h>
#include <packet2_utils.h>
#include <dma.h>

#include <resources.h>

#include <stdio.h>
#include <time.h>

int mesh_create(mesh_t *mesh, u32 count) {
	if (!mesh)
		return -1;

	memset(mesh, 0, sizeof(mesh_t));

	printf("sizeof(mesh_t) = %d\n", sizeof(mesh_t));

	mesh->vertices = memalign(128, sizeof(qvert_t) * count);
	if (!mesh->vertices)
		return -1;

	mesh->vert_count = count;

	return 0;
}

void mesh_init(mesh_t *mesh, u32 count, qvert_t *verts) {
	if (!mesh)
		return;

	memset(mesh, 0, sizeof(mesh_t));

	mesh_update(mesh, count, verts);
	
}

void mesh_update(mesh_t *mesh, u32 count, qvert_t *verts) {
	if (!mesh)
		return;
	mesh->vert_count = count;
	mesh->vertices = verts;
}

void mesh_set_quad_prim(mesh_t *mesh, texbuffer_t *tbuff) {
	if (!mesh)
		return;

	mesh->lod.calculation = LOD_USE_K;
	mesh->lod.max_level = 0;
	mesh->lod.mag_filter = LOD_MAG_NEAREST;
	mesh->lod.min_filter = LOD_MIN_NEAREST;
	mesh->lod.l = 0;
	mesh->lod.k = 0;

	mesh->clut.storage_mode = CLUT_STORAGE_MODE1;
	mesh->clut.start = 0;
	mesh->clut.psm = 0;
	mesh->clut.load_method = CLUT_NO_LOAD;
	mesh->clut.address = 0;

	// Define the triangle primitive we want to use.
	mesh->prim.type = PRIM_TRIANGLE_STRIP;
	mesh->prim.shading = PRIM_SHADE_GOURAUD;
	mesh->prim.mapping = DRAW_ENABLE;
	mesh->prim.fogging = DRAW_DISABLE;
	mesh->prim.blending = DRAW_ENABLE;
	mesh->prim.antialiasing = DRAW_DISABLE;
	mesh->prim.mapping_type = PRIM_MAP_ST;
	mesh->prim.colorfix = PRIM_UNFIXED;

	mesh->texture = tbuff;
	// Add ST base at start of VU mem
	mesh->vu1_extra_data = (VECTOR*)&cube_sts;
	mesh->vu1_extra_data_size = 4;
	// we need to send quad by quad
	mesh->vu1_min_vert_send = 4;

	// Set color
	for (int i=0;i<4;i++)
		mesh->color.vals[i] = 128;
}

void mesh_draw(mesh_t *mesh, renderer_t *rend) {
	if (!mesh || !rend)
		return;

	clock_t start = clock();

	MATRIX local_world, local_screen;
	create_local_world(local_world, mesh->pos, mesh->rot);
	create_local_screen(local_screen, local_world, rend->world_view, rend->view_screen);

	packet2_t *packet = renderer_get_vif_packet(rend);
	packet2_reset(packet, 0);

	// Add extra data at start of vu mem if needed
	if (mesh->vu1_extra_data != NULL && mesh->vu1_extra_data_size != 0)
		packet2_utils_vu_add_unpack_data(packet, 0, mesh->vu1_extra_data, mesh->vu1_extra_data_size, 0);

	u32 vif_added_qw = 0;   // zero because now we will use TOP register (double buffer)
							// we don't wan't to unpack at 8 + beggining of buffer, but at
							// the beggining of the buffer

	packet2_utils_vu_open_unpack(packet, vif_added_qw, 1);
	// Add matrix at the start of TOP
	for (int i=0;i<16;i++)
		packet2_add_float(packet, local_screen[i]);

	packet2_add_float(packet, 2048.0F);						// GS screen scale
	packet2_add_float(packet, 2048.0F);						// GS screen scale
	packet2_add_float(packet, ((float)0xFFFFFF) / 32.0F);	// GS screen scale
	packet2_add_u32(packet, mesh->vert_count);				// Number of verts

	packet2_utils_gif_add_set(packet, 1);

	packet2_utils_gs_add_lod(packet, &mesh->lod);

	packet2_utils_gs_add_texbuff_clut(packet, mesh->texture, &mesh->clut);
	
	for (u8 j = 0; j < 4; j++)
		packet2_add_u32(packet, mesh->color.vals[j]);
	vif_added_qw += packet2_utils_vu_close_unpack(packet);

	u32 vert_multiple = mesh->vu1_min_vert_send < 1 ? 1 : mesh->vu1_min_vert_send;

	u32 vert_drawn = 0;

	clock_t base = clock();

	// While we didn't send everything, append a new batch of data
	while (vert_drawn != mesh->vert_count) {
		// The qwords we have to work with to fit vertex data + GS data after processing
		// Keep a multiple of vertex to allow for some slack in the VCL loop unrolling
		u32 max_qwords = rend->vu1_dbf_offset - vif_added_qw - 4 - vert_multiple * (1 + 3); // -4 for the giftag we append right after
		// The verts we have left to draw
		u32 max_verts = mesh->vert_count - vert_drawn;

		u32 to_draw;
		// (to_draw + vert_multiple) because we are looking ahead one vertex mult to see if it fits
		// Vertex size is 1 for the input & 3 for the GS output (XYZ, RGB, STQ)
		for (to_draw = 0; (to_draw + vert_multiple) * (1 + 3) < max_qwords && to_draw < max_verts; to_draw += vert_multiple);

		// In case the count is not divisible by vert_multiple... (shouldn't happen)
		if (to_draw > max_verts)
			to_draw = max_verts;

		//printf("max_qwords=%d, calc_qwords=%d, to_draw=%d, vert_draw=%d, verts=%d\n", max_qwords, to_draw * (1 + 3), to_draw, vert_drawn, mesh->vert_count);

		// Add the GIF prim tag
		packet2_utils_vu_open_unpack(packet, vif_added_qw, 1);
		packet2_utils_gs_add_prim_giftag(packet, &mesh->prim, to_draw, DRAW_STQ2_REGLIST, 3, 0);
		vif_added_qw += packet2_utils_vu_close_unpack(packet);

		// Add vertices
		//packet2_utils_vu_add_unpack_data(packet, vif_added_qw, mesh->vertices + vert_drawn, to_draw, 1);
		packet2_chain_ref(packet, mesh->vertices + vert_drawn, to_draw/2, 0, 0, 0);
        packet2_vif_stcycl(packet, 0, 0x0101, 0);
        packet2_vif_open_unpack(packet, P2_UNPACK_V4_16, vif_added_qw, 1, 0, 0, 0);
        packet2_vif_close_unpack_manual(packet, to_draw);
		vif_added_qw += to_draw; // one VECTOR is size of qword

		// Start the program
		if (vert_drawn == 0) {
			packet2_utils_vu_add_start_program(packet, 0);
		} else { // Or continue it if we aren't at the start anymore
			packet2_chain_open_cnt(packet, 0, 0, 0);
			packet2_vif_flushe(packet, 0);
			packet2_vif_mscnt(packet, 0);
			packet2_chain_close_tag(packet);
		}
		vif_added_qw = 0;
		vert_drawn += to_draw;

		// If this gets triggered we already are over by a bit (+1 for end tag)
		assert(packet2_get_qw_count(packet)+1 <= RENDERER_VIF_PCKT_MAX_QWORDS);
	}

	packet2_utils_vu_add_end_tag(packet);

	clock_t finish = clock();

	//printf("Final qw count: %d, %d\n", packet2_get_qw_count(packet), mesh->vert_count);

	// Wait for the old vif packet to finish
	dma_channel_wait(DMA_CHANNEL_VIF1, 0);
	//dma_wait_fast();

	clock_t wait = clock();
	// Send the new one
	dma_channel_send_packet2(packet, DMA_CHANNEL_VIF1, 0);

	clock_t send = clock();

	//printf("%ldus (%ldus/%ldus/%ldus/%ldus) %d, %d\n", send-start, base-start, finish-base, wait-finish, send-wait,  packet2_get_qw_count(packet), mesh->vert_count);
}