#include <assert.h>
#include <math.h>
#include <math3d.h>
#include <renderer.h>

#include <string.h>
#include <packet2.h>
#include <packet2_utils.h>
#include <packet2_extras.h>
#include <graph.h>
#include <gs_psm.h>
#include <draw.h>
#include <dma.h>

#include <stdio.h>

int renderer_init(renderer_t *rend) {
	if (!rend)
		return -1;

	memset(rend, 0, sizeof(renderer_t));

	// Initialize vif packets
	for (int i=0;i<2;i++) {
		if (!packet2_init(&rend->vif_packets[i], NULL, NULL, RENDERER_VIF_PCKT_MAX_QWORDS, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1))
			return -1;
	}
	// Init clear packet
	if (!packet2_init(&rend->clear_packet, NULL, NULL, 35, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0))
		return -1;

	return 0;
}

int renderer_setup(renderer_t *rend, u16 width, u16 height) {
	if (!rend)
		return -1;

	// Define a 32-bit framebuffer.
	rend->fb.width = width;
	rend->fb.height = height;
	rend->fb.mask = 0;
	rend->fb.psm = GS_PSM_16S;
	rend->fb.address = graph_vram_allocate(width, height, rend->fb.psm, GRAPH_ALIGN_PAGE);

	// Enable the zbuffer.
	rend->zbuff.enable = DRAW_ENABLE;
	rend->zbuff.mask = 0;
	rend->zbuff.method = ZTEST_METHOD_GREATER_EQUAL;
	rend->zbuff.zsm = GS_ZBUF_24;
	rend->zbuff.address = graph_vram_allocate(width, height, rend->zbuff.zsm, GRAPH_ALIGN_PAGE);

	printf("Vram used frame: %d (%d)\n", graph_vram_size(rend->fb.width, rend->fb.height, rend->fb.psm, GRAPH_ALIGN_PAGE), rend->fb.address);
	printf("Vram used zbuff: %d (%d)\n", graph_vram_size(rend->fb.width, rend->fb.height, rend->zbuff.zsm, GRAPH_ALIGN_PAGE), rend->zbuff.address);

	assert(((int)rend->fb.address) >= 0 && ((int)rend->zbuff.address) >= 0);

	// Set a default interlaced video mode with flicker filter.
	graph_set_mode(GRAPH_MODE_INTERLACED, GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_ENABLE);

	// Set the screen up
	graph_set_screen(0, 0, width, height);

	// Set black background
	graph_set_bgcolor(0x80,0x00,0x00);

	// Non flicker filter uses RC 2
	//graph_set_framebuffer(1, rend->fb.address, rend->fb.width, rend->fb.psm, 0, 0);
	graph_set_framebuffer_filtered(rend->fb.address, rend->fb.width, rend->fb.psm, 0, 0);

	graph_enable_output();
	// Filter
	//graph_set_output(GRAPH_ENABLE,GRAPH_ENABLE,GRAPH_VALUE_ALPHA,GRAPH_RC1_ALPHA,GRAPH_BLEND_RC2,0x70);
	// Non filter
	//graph_set_output(GRAPH_DISABLE,GRAPH_ENABLE,GRAPH_VALUE_RC1,GRAPH_RC2_ALPHA,GRAPH_BLEND_RC2,0x80);

	// Create the view_screen matrix.
	renderer_set_perspective(rend, RENDERER_DEFAULT_FOV, RENDERER_DEFAULT_NEAR, RENDERER_DEFAULT_FAR);

	// Create clear packet
	// Clear framebuffer but don't update zbuffer.
	packet2_update(&rend->clear_packet, draw_disable_tests(rend->clear_packet.next, 0, &rend->zbuff));
	packet2_update(&rend->clear_packet, draw_clear(rend->clear_packet.next, 0, 2048.0f - width/2.0f, 2048.0f - height/2.0f, width, height, 0x40, 0x40, 0x40));
	packet2_update(&rend->clear_packet, draw_enable_tests(rend->clear_packet.next, 0, &rend->zbuff));
	packet2_update(&rend->clear_packet, draw_finish(rend->clear_packet.next));

	// Setup the drawing env.
	packet2_t *packet2 = packet2_create(20, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);

	// This will setup a default drawing environment.
	packet2_update(packet2, draw_setup_environment(packet2->next, 0, &rend->fb, &rend->zbuff));

	// Now reset the primitive origin to 2048-width/2,2048-height/2.
	packet2_update(packet2, draw_primitive_xyoffset(packet2->next, 0, (2048 - width/2.0f), (2048 - height/2.0f)));

	// Finish setting up the environment.
	packet2_update(packet2, draw_finish(packet2->next));

	// Now send the packet, no need to wait since it's the first.
	dma_channel_send_packet2(packet2, DMA_CHANNEL_GIF, 1);
	dma_wait_fast();

	packet2_free(packet2);

	return 0;
}

void renderer_upload_vu1(renderer_t *rend, u32 *start, u32 *end, u16 dbf_base, u16 dbf_offset) {
	if (!rend || !start || !end)
		return;

	rend->vu1_dbf_base = dbf_base;
	rend->vu1_dbf_offset = dbf_offset;

	u32 packet_size = packet2_utils_get_packet_size_for_program(start, end) + 2; // + 1 for end tag + 1 for dbf
	packet2_t *packet2 = packet2_create(packet_size, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1);

	printf("size prog: %d %d\n", packet_size, end-start);

	// VU1 Microprogram
	packet2_vif_add_micro_program(packet2, 0, start, end);
	// Double buffer settings
	packet2_utils_vu_add_double_buffer(packet2, dbf_base, dbf_offset);
	// End tag
	packet2_utils_vu_add_end_tag(packet2);
	
	dma_channel_send_packet2(packet2, DMA_CHANNEL_VIF1, 1);
	dma_channel_wait(DMA_CHANNEL_VIF1, 0);
	
	packet2_free(packet2);
}

int renderer_load_texture(texbuffer_t *texbuf, u16 width, u32 psm, u8 *data) {
	if (!texbuf || !data)
		return -1;

	// Allocate some vram for the texture buffer
	texbuf->width = 256;
	texbuf->psm = psm;
	texbuf->address = graph_vram_allocate(width, width, psm, GRAPH_ALIGN_BLOCK);
	texbuf->info.width = draw_log2(width);
	texbuf->info.height = draw_log2(width);
	texbuf->info.components = psm == GS_PSM_32 ? TEXTURE_COMPONENTS_RGBA : TEXTURE_COMPONENTS_RGB;
	texbuf->info.function = TEXTURE_FUNCTION_MODULATE;
	printf("Vram used textu: %d (%d)\n", graph_vram_size(texbuf->width, texbuf->width, texbuf->psm, GRAPH_ALIGN_BLOCK), texbuf->address);

	assert(((int)texbuf->address) >= 0);

	// Send the texture to the GS
	packet2_t *packet2 = packet2_create(50, P2_TYPE_NORMAL, P2_MODE_CHAIN, 0);

	packet2_update(packet2, draw_texture_transfer(packet2->next, data, texbuf->width, texbuf->width, texbuf->psm, texbuf->address, texbuf->width));
	packet2_update(packet2, draw_texture_flush(packet2->next));

	dma_channel_send_packet2(packet2, DMA_CHANNEL_GIF, 1);
	dma_wait_fast();

	packet2_free(packet2);

	return 0;
}

/** Send packet which will clear our screen. */
void renderer_clear(renderer_t *rend) {
	// Now send our current dma chain.
	dma_wait_fast();
	dma_channel_send_packet2(&rend->clear_packet, DMA_CHANNEL_GIF, 1);

	// Wait for clear to finish drawing
	draw_wait_finish();
}

void renderer_set_perspective(renderer_t *rend, float vfov, float near, float far) {
	float f = 1.0f / tanf((vfov/2.0f) * M_PI/180.0f);

	float c = (far + near) / (far - near);
	float d = (2.0f * far * near) / (far - near);

	rend->view_screen[ 0] = f/graph_aspect_ratio(); rend->view_screen[ 4] = 0.0f;  rend->view_screen[ 8] = 0.0f;  rend->view_screen[12] = 0.0f;
	rend->view_screen[ 1] = 0.0f                  ; rend->view_screen[ 5] = f;     rend->view_screen[ 9] = 0.0f;  rend->view_screen[13] = 0.0f;
	rend->view_screen[ 2] = 0.0f                  ; rend->view_screen[ 6] = 0.0f;  rend->view_screen[10] = c;     rend->view_screen[14] = d;
	rend->view_screen[ 3] = 0.0f                  ; rend->view_screen[ 7] = 0.0f;  rend->view_screen[11] = -1.0f; rend->view_screen[15] = 0.0f;

	/*printf("%.8f %.8f %.8f %.8f\n%.8f %.8f %.8f %.8f\n%.8f %.8f %.8f %.8f\n%.8f %.8f %.8f %.8f\n------------\n", rend->view_screen[0], rend->view_screen[1], rend->view_screen[2], rend->view_screen[3],
		rend->view_screen[4], rend->view_screen[5], rend->view_screen[6], rend->view_screen[7],rend->view_screen[8], rend->view_screen[9], rend->view_screen[10], rend->view_screen[11],
		rend->view_screen[12], rend->view_screen[13], rend->view_screen[14], rend->view_screen[15]);*/

	/*float top = near * tanf((vfov/2.0f) * (M_PI/180.0f));
	float bottom = -top;
	float right = top;
	float left = -right;

	create_view_screen(rend->view_screen, graph_aspect_ratio(), left, right, bottom, top, near, far);*/
	//create_view_screen(rend->view_screen, graph_aspect_ratio(), -3.00f, 3.00f, -3.00f, 3.00f, 1.00f, 2000.00f);

#define MAT_IDX(col,row) (row*4+col)
	for (int i = 4; i--; ) { rend->frustum[5][i] = rend->view_screen[MAT_IDX(i,3)] + rend->view_screen[MAT_IDX(i,0)]; }
	for (int i = 4; i--; ) { rend->frustum[4][i] = rend->view_screen[MAT_IDX(i,3)] - rend->view_screen[MAT_IDX(i,0)]; }
	for (int i = 4; i--; ) { rend->frustum[2][i] = rend->view_screen[MAT_IDX(i,3)] + rend->view_screen[MAT_IDX(i,1)]; }
	for (int i = 4; i--; ) { rend->frustum[3][i] = rend->view_screen[MAT_IDX(i,3)] - rend->view_screen[MAT_IDX(i,1)]; }
	for (int i = 4; i--; ) { rend->frustum[0][i] = rend->view_screen[MAT_IDX(i,3)] + rend->view_screen[MAT_IDX(i,2)]; }
	for (int i = 4; i--; ) { rend->frustum[1][i] = rend->view_screen[MAT_IDX(i,3)] - rend->view_screen[MAT_IDX(i,2)]; }
#undef MAT_IDX

	for (int i=0;i<6;i++) {
		float invlen = 1.0f / sqrtf(rend->frustum[i][0] * rend->frustum[i][0] + rend->frustum[i][1] * rend->frustum[i][1] + rend->frustum[i][2] * rend->frustum[i][2]);
		printf("%d - %f\n", i, invlen);
		rend->frustum[i][0] *= invlen;
		rend->frustum[i][1] *= invlen;
		rend->frustum[i][2] *= invlen;
		rend->frustum[i][3] *= invlen;
	}

}

void renderer_update_matrices(renderer_t *rend) {
	create_world_view(rend->world_view, rend->camera_pos, rend->camera_rot);
	matrix_multiply(rend->local_screen, rend->world_view, rend->view_screen);
}


static inline void vector_scale(VECTOR out, VECTOR in1, float scale) {
	out[0] = in1[0]*scale;
	out[1] = in1[1]*scale;
	out[2] = in1[2]*scale;
	out[3] = in1[3]*scale;
}

static inline float vector_dot3(VECTOR v1, VECTOR v2) {
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

static inline float vector_dot3f(VECTOR v1, float x, float y, float z) {
	return v1[0] * x + v1[1] * y + v1[2] * z;
}


// 12.183ms all seen, 12.563ms all hidden, 512chunks
/*static u8 renderer_check_box_plane(VECTOR plane, VECTOR center, VECTOR extents) {
	// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
	const float r = extents[0] * fabsf(plane[0]) +
			extents[1] * fabsf(plane[1]) + extents[2] * fabsf(plane[2]);

	return -r <= ((plane[0] * center[0] + plane[1] * center[1] + plane[2] * center[2]) - plane[3]);
}

u8 renderer_check_box_frustum(renderer_t *rend, VECTOR min, VECTOR size) {
	VECTOR max;
	VECTOR newmin;
	vector_add(max, min, size);
	vector_apply(newmin, min, rend->world_view);
	vector_apply(max, max, rend->world_view);

	VECTOR center;
	vector_add(center, max, newmin);
	vector_scale(center, center, 0.5f);

	VECTOR extents;
	vector_scale(extents, center, -1.0f);
	vector_add(extents, extents, max);

	return  !(renderer_check_box_plane(rend->frustum[0], center, extents) &&
			renderer_check_box_plane(rend->frustum[1], center, extents) &&
			renderer_check_box_plane(rend->frustum[2], center, extents) &&
			renderer_check_box_plane(rend->frustum[3], center, extents) &&
			renderer_check_box_plane(rend->frustum[4], center, extents) &&
			renderer_check_box_plane(rend->frustum[5], center, extents));

}*/
// 12.065ms all seen, 12.270ms no seen 512chunks
/*
static u8 renderer_check_box_plane(VECTOR plane, VECTOR min, VECTOR max) {
	return !(vector_dot3f(plane, min[0], min[1], min[2]) < 0.0 &&
			 vector_dot3f(plane, max[0], min[1], min[2]) < 0.0 &&
			 vector_dot3f(plane, min[0], max[1], min[2]) < 0.0 &&
			 vector_dot3f(plane, max[0], max[1], min[2]) < 0.0 &&
			 vector_dot3f(plane, min[0], min[1], max[2]) < 0.0 &&
			 vector_dot3f(plane, max[0], min[1], max[2]) < 0.0 &&
			 vector_dot3f(plane, min[0], max[1], max[2]) < 0.0 &&
			 vector_dot3f(plane, max[0], max[1], max[2]) < 0.0);
}

u8 renderer_check_box_frustum(renderer_t *rend, VECTOR min, VECTOR size) {
	VECTOR max;
	VECTOR newmin;
	vector_add(max, min, size);
	vector_apply(newmin, min, rend->world_view);
	vector_apply(max, max, rend->world_view);

	return !(renderer_check_box_plane(rend->frustum[0], newmin, max) &&
			 renderer_check_box_plane(rend->frustum[1], newmin, max) &&
			 renderer_check_box_plane(rend->frustum[2], newmin, max) &&
			 renderer_check_box_plane(rend->frustum[3], newmin, max) &&
			 renderer_check_box_plane(rend->frustum[4], newmin, max) &&
			 renderer_check_box_plane(rend->frustum[5], newmin, max));

}*/
// 12.025ms all seen 12.481ms all hidden 512 chunks

static inline u8 renderer_check_in_clip(MATRIX mat, float x, float y, float z) {
	VECTOR vec = {x,y,z,1.0f};
	vector_apply(vec, vec, mat);
	// https://bruop.github.io/frustum_culling/
	//printf("%.2f %.2f %.2f %.2f\n", vec[0], vec[1], vec[2], vec[3]);
	// Value i took from my ass, i don't know where it come from, it just work(tm)
	float val = vec[3]*0.25f;
	return 	 0  >= vec[2] && vec[2] >= -vec[3] &&
			val >= vec[0] && vec[0] >= -val &&
			val >= vec[1] && vec[1] >= -val;
}

u8 renderer_check_box_frustum(renderer_t *rend, VECTOR min, VECTOR size) {
	VECTOR max;
	vector_add(max, min, size);

	return  renderer_check_in_clip(rend->local_screen, min[0], min[1], min[2]) ||
			renderer_check_in_clip(rend->local_screen, max[0], min[1], min[2]) ||
			renderer_check_in_clip(rend->local_screen, min[0], max[1], min[2]) ||
			renderer_check_in_clip(rend->local_screen, max[0], max[1], min[2]) ||
			renderer_check_in_clip(rend->local_screen, min[0], min[1], max[2]) ||
			renderer_check_in_clip(rend->local_screen, max[0], min[1], max[2]) ||
			renderer_check_in_clip(rend->local_screen, min[0], max[1], max[2]) ||
			renderer_check_in_clip(rend->local_screen, max[0], max[1], max[2]);
}

packet2_t *renderer_get_vif_packet(renderer_t *rend) {
	packet2_t *packet = &rend->vif_packets[rend->curr_vif_packet];
	rend->curr_vif_packet = (rend->curr_vif_packet + 1)%2;
	return packet;
}