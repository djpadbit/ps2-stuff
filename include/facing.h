#ifndef __FACING_H__
#define __FACING_H__

#include <tamtypes.h>
#include <math3d.h>

enum facing : u8 {
	FC_RIGHT = 0,
	FC_LEFT = 1,
	FC_UP = 2,
	FC_DOWN = 3,
	FC_FRONT = 4,
	FC_BACK = 5,
	FC_END = 6
};

const extern VECTOR facing_dir[FC_END];
const extern enum facing facing_opposite[FC_END];
/*static inline void facing_dir(enum facing face, VECTOR vec) {
	vec[3] = 0.0f;
	switch (face) {
		case FC_RIGHT:
			vec[0] = 1.0f;
			vec[1] = vec[2] = 0.0f;
			return;
		case FC_LEFT:
			vec[0] = -1.0f;
			vec[1] = vec[2] = 0.0f;
			return;
		case FC_UP:
			vec[1] = 1.0f;
			vec[0] = vec[2] = 0.0f;
			return;
		case FC_DOWN:
			vec[1] = -1.0f;
			vec[0] = vec[2] = 0.0f;
			return;
		case FC_FRONT:
			vec[2] = 1.0f;
			vec[0] = vec[1] = 0.0f;
			return;
		case FC_BACK:
			vec[2] = -1.0f;
			vec[0] = vec[1] = 0.0f;
			return;
		default:
			break;
	}
	return;
}*/
/*
static inline enum facing facing_opposite(enum facing face) {
	switch (face) {
		case FC_RIGHT:
			return FC_LEFT;
		case FC_LEFT:
			return FC_RIGHT;
		case FC_UP:
			return FC_DOWN;
		case FC_DOWN:
			return FC_UP;
		case FC_FRONT:
			return FC_BACK;
		case FC_BACK:
			return FC_FRONT;
		default:
			break;
	}
	return FC_END;
}*/

#endif