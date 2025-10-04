/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (c) 2020 h4570 Sandro Sobczyński <sandro.sobczynski@gmail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/

#include <math3d.h>
#include <mesh_quad.h>

const int cube_vertex_count = 24;
const int cube_sts_count = 4;

#if 1
#define CUBE_VERT_COORD (1<<3)

const mesh_qvert_t cube_vertices[24] = {
    {{ CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, 0 | (1 << (2+8))}}, // right 0
    {{ CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, 1 | (1 << (2+8))}},
    {{ CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, 2 | (0 << (2+8))}},
    {{ CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, 3 | (0 << (2+8))}},
    {{-CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, 0 | (1 << (2+8))}}, // left 4
    {{-CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, 1 | (1 << (2+8))}},
    {{-CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, 2 | (0 << (2+8))}},
    {{-CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, 3 | (0 << (2+8))}},
    {{-CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, 0 | (1 << (2+8))}}, // up 8
    {{ CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, 1 | (1 << (2+8))}},
    {{-CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, 2 | (0 << (2+8))}},
    {{ CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, 3 | (0 << (2+8))}},
    {{-CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, 0 | (1 << (2+8))}}, // down 12
    {{ CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, 1 | (1 << (2+8))}},
    {{-CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, 2 | (0 << (2+8))}},
    {{ CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, 3 | (0 << (2+8))}},
    {{-CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, 0 | (1 << (2+8))}}, // front 16
    {{ CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, 1 | (1 << (2+8))}},
    {{-CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, 2 | (0 << (2+8))}},
    {{ CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, 3 | (0 << (2+8))}},
    {{-CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, 0 | (1 << (2+8))}}, // back 20
    {{ CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, 1 | (1 << (2+8))}},
    {{-CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, 2 | (0 << (2+8))}},
    {{ CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, 3 | (0 << (2+8))}}};

#else
#define CUBE_VERT_COORD 10.0f

// Calculate a floating point number from a integer
// Doesn't handle a full exponent
#define MANTISSA_CALC(val, idx) ( ((float)(((val)>>(23-idx))&1)) / ((float)(1<<idx)) )
#define FULL_MANTISSA(val) ( ((((val)>>23)&0xFF) == 0 ? 0.0f : 1.0f) + (MANTISSA_CALC((val), 1) + MANTISSA_CALC((val), 2) + MANTISSA_CALC((val), 3) + MANTISSA_CALC((val), 4) + MANTISSA_CALC((val), 5) + MANTISSA_CALC((val), 6) + MANTISSA_CALC((val), 7) + MANTISSA_CALC((val), 8) + MANTISSA_CALC((val), 9) + MANTISSA_CALC((val), 10) + MANTISSA_CALC((val), 11) + MANTISSA_CALC((val), 12) + MANTISSA_CALC((val), 13) + MANTISSA_CALC((val), 14) + MANTISSA_CALC((val), 15) + MANTISSA_CALC((val), 16) + MANTISSA_CALC((val), 17) + MANTISSA_CALC((val), 18) + MANTISSA_CALC((val), 19) + MANTISSA_CALC((val), 20) + MANTISSA_CALC((val), 21) + MANTISSA_CALC((val), 22) + MANTISSA_CALC((val), 23)))
#define SIGNBIT_CALC(val) ( (float)(-1*((((val)>>31)&1)*2 - 1)) )
#define EXP_CALC(val) ( (((val)>>23)&0xFF) == 0 ? (1.0f/(float)(((__int128)1)<<126)) : (((val)>>23)&0xFF) < 127 ? (1.0f/(float)(((__int128)1) << ( 127 - (((val)>>23)&0xFF)))) : (float)(((__int128)1) << ( (((val)>>23)&0xFF) - 127)) )
#define INT_TO_FLOAT(val) ( SIGNBIT_CALC((val)) * EXP_CALC((val)) * FULL_MANTISSA((val)) )

const VECTOR cube_vertices[24] = {
    { CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(0 | (1 << (2+8)))}, // right 0
    { CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(1 | (1 << (2+8)))},
    { CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(2 | (0 << (2+8)))},
    { CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(3 | (0 << (2+8)))},
    {-CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(0 | (1 << (2+8)))}, // left 4
    {-CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(1 | (1 << (2+8)))},
    {-CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(2 | (0 << (2+8)))},
    {-CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(3 | (0 << (2+8)))},
    {-CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(0 | (1 << (2+8)))}, // up 8
    { CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(1 | (1 << (2+8)))},
    {-CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(2 | (0 << (2+8)))},
    { CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(3 | (0 << (2+8)))},
    {-CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(0 | (1 << (2+8)))}, // down 12
    { CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(1 | (1 << (2+8)))},
    {-CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(2 | (0 << (2+8)))},
    { CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(3 | (0 << (2+8)))},
    {-CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(0 | (1 << (2+8)))}, // front 16
    { CUBE_VERT_COORD,  CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(1 | (1 << (2+8)))},
    {-CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(2 | (0 << (2+8)))},
    { CUBE_VERT_COORD, -CUBE_VERT_COORD,  CUBE_VERT_COORD, INT_TO_FLOAT(3 | (0 << (2+8)))},
    {-CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(0 | (1 << (2+8)))}, // back 20
    { CUBE_VERT_COORD,  CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(1 | (1 << (2+8)))},
    {-CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(2 | (0 << (2+8)))},
    { CUBE_VERT_COORD, -CUBE_VERT_COORD, -CUBE_VERT_COORD, INT_TO_FLOAT(3 | (0 << (2+8)))}};
#endif


#define CUBE_ST_COORD_ST (0.0f)
#define CUBE_ST_COORD_ED (1.0f/16.0f)

/** Texture coordinates */
const VECTOR cube_sts[4] = {
    {CUBE_ST_COORD_ST, CUBE_ST_COORD_ST, 1.0f, 0.0f}, // Top    right
    {CUBE_ST_COORD_ED, CUBE_ST_COORD_ST, 1.0f, 0.0f}, // Bottom right
    {CUBE_ST_COORD_ST, CUBE_ST_COORD_ED, 1.0f, 0.0f}, // Top    left
    {CUBE_ST_COORD_ED, CUBE_ST_COORD_ED, 1.0f, 0.0f}};// Bottom left