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

#endif