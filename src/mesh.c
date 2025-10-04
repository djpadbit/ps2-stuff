#include <renderer.h>
#include <mesh.h>

#include <assert.h>
#include <string.h>

void mesh_init(mesh_t *mesh, mesh_type_t *type, u32 count, void *verts) {
	if (!mesh)
		return;

	memset(mesh, 0, sizeof(mesh_t));
	mesh->type = type;

	mesh_update(mesh, count, verts);
}

void mesh_update(mesh_t *mesh, u32 count, void *verts) {
	if (!mesh)
		return;
	mesh->vert_count = count;
	mesh->vertices = verts;
}

void mesh_update_matrix(mesh_t *mesh) {
	if (!mesh)
		return;
	create_local_world(mesh->local_world, mesh->pos, mesh->rot);
}