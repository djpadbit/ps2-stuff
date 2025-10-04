#include "packet2_types.h"
#include <kernel.h>
#include <malloc.h>
#include <packet2.h>
#include <packet2_utils.h>
#include <string.h>

#define P2_ALIGNMENT 64
#define P2_GET_PTYPE(PTR) ( (enum Packet2Type)(((u32)PTR)&(~0x0FFFFFFF)) )

packet2_t *packet2_init(packet2_t* packet2, qword_t *base, qword_t *next, u16 qwords, enum Packet2Type type, enum Packet2Mode mode, u8 tte)
{
	if (packet2 == NULL)
		return NULL;

	if (base == NULL) { // We need to allocate it
		// Dma buffer size should be a whole number of cache lines (64 bytes = 4 quads)
		assert(!((type == P2_TYPE_UNCACHED || type == P2_TYPE_UNCACHED_ACCL) && qwords & (4 - 1)));

		u32 byte_size = qwords << 4;

		if ((base = memalign(P2_ALIGNMENT, byte_size)) == NULL) {
			return NULL;
		}

		base = next = (qword_t *)((u32)base | type);

		memset(base, 0, byte_size);
	} else { // Already allocated
		type = P2_GET_PTYPE(base);

		// Dma buffer size should be a whole number of cache lines (64 bytes = 4 quads)
		assert(!((type == P2_TYPE_UNCACHED || type == P2_TYPE_UNCACHED_ACCL) && qwords & (4 - 1)));

		// Dma buffer should be aligned on a cache line (64-byte boundary)
		assert(!((type == P2_TYPE_UNCACHED || type == P2_TYPE_UNCACHED_ACCL) && (u32)base & (64 - 1)));
	}

	packet2->base = base;
	packet2->next = next;

	packet2->max_qwords_count = qwords;
	packet2->type = type;
	packet2->mode = mode;
	packet2->tte = tte;
	packet2->tag_opened_at = NULL;
	packet2->vif_code_opened_at = NULL;

	if (type == P2_TYPE_UNCACHED || type == P2_TYPE_UNCACHED_ACCL)
		FlushCache(0);

	return packet2;
}

packet2_t* packet2_reset_cfg(packet2_t *packet2, enum Packet2Mode mode, u8 tte, u8 clear_mem) {
	packet2->next = packet2->base;
	packet2->vif_code_opened_at = NULL;
	packet2->tag_opened_at = NULL;
	packet2->mode = mode;
	packet2->tte = tte;
	if (clear_mem)
		memset(packet2->base, 0, packet2->max_qwords_count << 4);

	return packet2;
}