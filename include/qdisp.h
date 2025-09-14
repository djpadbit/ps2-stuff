#ifndef __QDISP_H__
#define __QDISP_H__

#include <facing.h>
#include <math3d.h>

typedef union {
	struct __attribute__((packed)) {
		s16 x,y,z,w;
	};
	s16 vals[4];
	u64 val;
} qvert_t;

u32 qdisp_put(qvert_t *verts, qvert_t position, qvert_t points[4], u8 texId);
u32 qdisp_put_dir(qvert_t *verts, qvert_t position, enum facing face, u8 texId);

#endif