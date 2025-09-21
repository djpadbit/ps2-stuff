#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <packet2_types.h>
#include <math3d.h>
#include <draw_buffers.h>

#define RENDERER_VIF_PCKT_MAX_QWORDS 1024

#define RENDERER_DEFAULT_FOV 160.0f
#define RENDERER_DEFAULT_FAR 2000.0f
#define RENDERER_DEFAULT_NEAR 1.0f

typedef struct {
	packet2_t vif_packets[2];
	packet2_t clear_packet;
	MATRIX view_screen __attribute__((aligned(64)));
	MATRIX world_view __attribute__((aligned(64)));
	VECTOR camera_pos;
	VECTOR camera_rot;
	VECTOR frustum[6];
	framebuffer_t fb;
	zbuffer_t zbuff;
	u32 vu1_dbf_base;
	u32 vu1_dbf_offset;
	u8 curr_vif_packet;
} renderer_t;

int renderer_init(renderer_t *rend);
int renderer_setup(renderer_t *rend, u16 width, u16 height);
int renderer_load_texture(texbuffer_t *texbuf, u16 width, u32 psm, u8 *data);
void renderer_clear(renderer_t *rend);
// After any camera rotation/translation
void renderer_update_matrices(renderer_t *rend);
// return 1 if in frustum
u8 renderer_check_box_frustum(renderer_t *rend, VECTOR min, VECTOR size);
// if any value < 0 => uses default values
void renderer_set_perspective(renderer_t *rend, float vfov, float near, float far);
packet2_t *renderer_get_vif_packet(renderer_t *rend);
void renderer_upload_vu1(renderer_t *rend, u32 *start, u32 *end, u16 dbf_base, u16 dbf_offset);

#endif