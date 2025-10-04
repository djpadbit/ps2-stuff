#ifndef __PACKET2_EXTRAS__
#define __PACKET2_EXTRAS__

#include <packet2_types.h>

// Either allocates the memory if base & next are null or inits the struct with the pointers given
packet2_t *packet2_init(packet2_t* packet2, qword_t *base, qword_t *next, u16 qwords, enum Packet2Type type, enum Packet2Mode mode, u8 tte);

// Resets the packet & sets the mode + tte
packet2_t *packet2_reset_cfg(packet2_t *packet2, enum Packet2Mode mode, u8 tte, u8 clear_mem);

#endif