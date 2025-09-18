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
	rend->fb.height = 512;
	rend->fb.mask = 0;
	rend->fb.psm = GS_PSM_32;
	rend->fb.address = graph_vram_allocate(width, height, rend->fb.psm, GRAPH_ALIGN_PAGE);

	// Enable the zbuffer.
	rend->zbuff.enable = DRAW_ENABLE;
	rend->zbuff.mask = 0;
	rend->zbuff.method = ZTEST_METHOD_GREATER_EQUAL;
	rend->zbuff.zsm = GS_ZBUF_32;
	rend->zbuff.address = graph_vram_allocate(width, height, rend->zbuff.zsm, GRAPH_ALIGN_PAGE);

	printf("Vram used frame: %d\n", graph_vram_size(rend->fb.width, rend->fb.height, rend->fb.psm, GRAPH_ALIGN_PAGE));
	printf("Vram used zbuff: %d\n", graph_vram_size(rend->fb.width, rend->fb.height, rend->zbuff.zsm, GRAPH_ALIGN_PAGE));

	// Set a default interlaced video mode with flicker filter.
	graph_set_mode(GRAPH_MODE_INTERLACED, GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_ENABLE);

	// Set the screen up
	graph_set_screen(0, 0, width, height);

	// Set black background
	graph_set_bgcolor(0,0,0);

	graph_set_framebuffer_filtered(rend->fb.address, rend->fb.width, rend->fb.psm, 0, 0);

	graph_enable_output();

	// Create the view_screen matrix.
	create_view_screen(rend->view_screen, graph_aspect_ratio(), -3.00f, 3.00f, -3.00f, 3.00f, 1.00f, 2000.00f);

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
	packet2_update(packet2, draw_primitive_xyoffset(packet2->next, 0, (2048 - width/2), (2048 - height/2)));

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
	texbuf->info.function = TEXTURE_FUNCTION_DECAL;
	printf("Vram used textu: %d\n", graph_vram_size(texbuf->width, texbuf->width, texbuf->psm, GRAPH_ALIGN_BLOCK));

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

void renderer_update_matrices(renderer_t *rend) {
	create_world_view(rend->world_view, rend->camera_pos, rend->camera_rot);
}

packet2_t *renderer_get_vif_packet(renderer_t *rend) {
	packet2_t *packet = &rend->vif_packets[rend->curr_vif_packet];
	rend->curr_vif_packet = (rend->curr_vif_packet + 1)%2;
	return packet;
}