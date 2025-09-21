#include <facing.h>

const VECTOR facing_dir[FC_END] = {
	{1.0f, 0.0f, 0.0f, 0.0f},
	{-1.0f, 0.0f, 0.0f, 0.0f},
	{0.0f, 1.0f, 0.0f, 0.0f},
	{0.0f, -1.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 1.0f, 0.0f},
	{0.0f, 0.0f, -1.0f, 0.0f},
};

const enum facing facing_opposite[FC_END] = {
	FC_LEFT,
	FC_RIGHT,
	FC_DOWN,
	FC_UP,
	FC_BACK,
	FC_FRONT,
};