#include <input_manager.h>

#include <kernel.h>
#include <malloc.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <libpad.h>
#include <string.h>

int inputman_load_iop() {
	int ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
	if (ret < 0) {
		printf("sifLoadModule sio failed: %d\n", ret);
		return -1;
	}

	ret = SifLoadModule("rom0:PADMAN", 0, NULL);
	if (ret < 0) {
		printf("sifLoadModule pad failed: %d\n", ret);
		return -1;
	}

	printf("Loaded IOP modules\n");

	// Init the pad mananger
	if (padInit(0) != 1)
		return -1;

	printf("Initialized the pad mananger\n");
	return 0;
}

static inline int inputman_is_rdy(int state) {
	return (state == PAD_STATE_STABLE) || (state == PAD_STATE_FINDCTP1);
}

static int inputman_setup_pad(input_manager_t *man, int port, int slot) {
	// How many different modes can this device operate in?
	// i.e. get # entrys in the modetable
	int modes = padInfoMode(port, slot, PAD_MODETABLE, -1);

	// If modes == 0, this is not a Dual shock controller
	// (it has no actuator engines)
	if (modes == 0) {
		printf("This is a digital controller?\n");
		return -1;
	}

	printf("It is currently using mode %d\n", padInfoMode(port, slot, PAD_MODECURID, 0));

	// Verify that the controller has a DUAL SHOCK mode
	int i = 0;
	do {
		if (padInfoMode(port, slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
			break;
		i++;
	} while (i < modes);

	if (i >= modes) {
		printf("This is no Dual Shock controller\n");
		return -1;
	}

	// If ExId != 0x0 => This controller has actuator engines
	// This check should always pass if the Dual Shock test above passed
	int ret = padInfoMode(port, slot, PAD_MODECUREXID, 0);
	if (ret == 0) {
		printf("This is no Dual Shock controller??\n");
		return -1;
	}

	printf("Enabling dual shock functions\n");

	// When using MMODE_LOCK, user cant change mode with Select button
	if (padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK) != 1)
		return -1;

	man->initialized = 1;

	return inputman_is_rdy(padGetState(man->port, man->slot)) ? 0 : -1;
}

static int inputman_padrdy(input_manager_t *man, int wait) {
	char stateString[16];
	int state = padGetState(man->port, man->slot);
	int ret = 0;
	if (state == PAD_STATE_DISCONN) {
		man->initialized = 0;
		ret = -1;
	}

	if ((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1))
		ret = -1;

	if (wait) {
		int lastState = -1;
		while ((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1)) {
			if (state != lastState) {
				padStateInt2String(state, stateString);
				printf("Please wait, pad(%d,%d) is in state %s\n",
						   man->port, man->slot, stateString);
			}
			lastState = state;
			state = padGetState(man->port, man->slot);
		}

		// Were the pad ever 'out of sync'?
		if (lastState != -1) {
			printf("Pad OK!\n");
		}
	}

	if (ret == 0 && !man->initialized)
		ret = inputman_setup_pad(man, man->port, man->slot);

	return ret;
}

int inputman_init(input_manager_t *man, int port, int slot) {
	if (!man)
		return -1;

	memset(man, 0, sizeof(input_manager_t));

	man->pad_data = memalign(64, 256);
	if (!man->pad_data)
		return -1;

	man->port = port;
	man->slot = slot;
	man->initialized = 0;

	if (!padPortOpen(port, slot, man->pad_data)) {
		free(man->pad_data);
		return -1;
	}

	// This will init the pad if present & optionally get data
	inputman_read(man);

	return 0;
}

int inputman_read(input_manager_t *man) {
	if (!man)
		return -1;

	// Check if pad is alive, it will try to bring up the pad if dead
	if (inputman_padrdy(man, 0) < 0)
		return -1;
	if (padRead(man->port, man->slot, &man->buttons) == 0)
		return -1;

	return 0;
}

