#ifndef __INPUT_MANAGER_H__
#define __INPUT_MANAGER_H__

#include <tamtypes.h>
#include <libpad.h>

typedef struct {
	int port, slot;
	int initialized;
	u8 *pad_data;
	struct padButtonStatus buttons;
} input_manager_t;

// Required before initializing instances of inputman
int inputman_load_iop();

int inputman_init(input_manager_t *man, int port, int slot);
int inputman_read(input_manager_t *man);


#endif