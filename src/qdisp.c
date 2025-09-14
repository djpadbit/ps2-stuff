#include <qdisp.h>

#include <resources.h>

u32 qdisp_put(qvert_t *verts, qvert_t position, qvert_t points[4], u8 texId) {
	for (int i=0;i<4;i++) {
		verts[i].vals[0] = points[i].vals[0] + position.vals[0];
		verts[i].vals[1] = points[i].vals[1] + position.vals[1];
		verts[i].vals[2] = points[i].vals[2] + position.vals[2];

		u8 texX = (texId % 16) & 0xF;
		u8 texY = (texId / 16) & 0xF;
		verts[i].w = points[i].w | ((texX | (texY << 4)) << 2);

		/*u32 *flags_in = (u32*)&points[i].vals[3];
		u32 *flags_out = (u32*)&verts[i].vals[3];

		// Add texture offset to flags
		u8 texX = (texId % 16) & 0xF;
		u8 texY = (texId / 16) & 0xF;
		u32 flags = (*flags_in) | ((texX | (texY << 4)) << 2);
		// Write flags
		(*flags_out) = flags;*/
	}
	return 4;
}

u32 qdisp_put_dir(qvert_t *verts, qvert_t position, enum facing face, u8 texId) {
	if (face < 0 || face >= 6)
		face = 0;
	return qdisp_put(verts, position, ((qvert_t*)cube_vertices)+(face*4), texId);
}