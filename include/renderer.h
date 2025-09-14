#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <packet2_types.h>
#include <math3d.h>
#include <draw_buffers.h>

typedef struct {
	packet2_t vif_packets[2];
	packet2_t clear_packet;
	MATRIX view_screen __attribute__((aligned(64)));
	MATRIX world_view __attribute__((aligned(64)));
	u8 curr_vif_packet;
	VECTOR camera_pos;
	VECTOR camera_rot;
	framebuffer_t fb;
	zbuffer_t zbuff;
	u32 vu1_dbf_base;
	u32 vu1_dbf_offset;
} renderer_t;

int renderer_init(renderer_t *rend);
int renderer_setup(renderer_t *rend, u16 width, u16 height);
int renderer_load_texture(texbuffer_t *texbuf, u16 width, u32 psm, u8 *data);
void renderer_clear(renderer_t *rend);
// After any camera rotation/translation
void renderer_update_matrices(renderer_t *rend);
packet2_t *renderer_get_vif_packet(renderer_t *rend);
void renderer_upload_vu1(renderer_t *rend, u32 *start, u32 *end, u16 dbf_base, u16 dbf_offset);

#endif