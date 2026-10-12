#include "smolinput.h"

#ifndef NOLIBC
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

/* Recreate the pieces from <linux/uinput.h> because it pulls in <sys/time.h> */
struct mock_input_id {
	uint16_t bustype;
	uint16_t vendor;
	uint16_t product;
	uint16_t version;
};

struct mock_uinput_setup {
	struct mock_input_id id;
	char name[80];
	uint32_t ff_effects_max;
};

#define MOCK_UI_DEV_CREATE	_IO('U', 1)
#define MOCK_UI_DEV_DESTROY	_IO('U', 2)
#define MOCK_UI_DEV_SETUP	_IOW('U', 3, struct mock_uinput_setup)
#define MOCK_UI_SET_EVBIT	_IOW('U', 100, int)
#define MOCK_UI_SET_KEYBIT	_IOW('U', 101, int)

#define MOCK_BUS_VIRTUAL	0x06

#ifndef MOCK_UINPUT_PATH
#define MOCK_UINPUT_PATH	"/dev/uinput"
#endif

/* All of the keys we claim to have */
static const uint16_t mock_keys[] = {
	KEY_UP,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_A,
	KEY_B,
	KEY_C,
	KEY_D,
	KEY_E,
	KEY_F,
	KEY_G,
	KEY_H,
	KEY_I,
	KEY_J,
	KEY_K,
	KEY_L,
	KEY_M,
	KEY_N,
	KEY_O,
	KEY_P,
	KEY_Q,
	KEY_R,
	KEY_S,
	KEY_T,
	KEY_U,
	KEY_V,
	KEY_W,
	KEY_X,
	KEY_Y,
	KEY_Z,
	KEY_0,
	KEY_1,
	KEY_2,
	KEY_3,
	KEY_4,
	KEY_5,
	KEY_6,
	KEY_7,
	KEY_8,
	KEY_9,
	KEY_ENTER,
	KEY_ESC,
	KEY_MENU,
	KEY_POWER,
	KEY_SPACE,
	KEY_TAB,
	KEY_BACKSPACE,
	KEY_LEFTSHIFT,
};

static void mock_closefd(int *fd)
{
	if (*fd >= 0)
		close(*fd);

	*fd = -1;
}

#define __mock_cleanup_fd __attribute__((cleanup(mock_closefd)))

static void mock_destroydevice(int *fd)
{
	if (*fd >= 0)
		ioctl(*fd, MOCK_UI_DEV_DESTROY, 0);

	mock_closefd(fd);
}

#define __mock_cleanup_device __attribute__((cleanup(mock_destroydevice)))

static int mock_create(const char *name, int *fd)
{
	struct mock_uinput_setup setup = {
		.id.bustype = MOCK_BUS_VIRTUAL,
	};
	unsigned int i;

	*fd = open(MOCK_UINPUT_PATH, O_WRONLY | O_CLOEXEC);
	if (*fd < 0) {
		/* This is usually the node is owned by root or CONFIG_UINPUT=N */
		fprintf(stderr, "cannot open %s: %d\n", MOCK_UINPUT_PATH, errno);
		return -1;
	}

	if (ioctl(*fd, MOCK_UI_SET_EVBIT, EV_KEY) < 0) {
		fprintf(stderr, "set evbit ev_key failed\n");
		return -1;
	}

	for (i = 0; i < SMOLINPUT_ARRAYSZ(mock_keys); i++) {
		if (ioctl(*fd, MOCK_UI_SET_KEYBIT, (int) mock_keys[i]) < 0) {
			fprintf(stderr, "set keybit failed for %u\n", mock_keys[i]);
			return -1;
		}
	}

	snprintf(setup.name, sizeof(setup.name), "%s", name);

	if (ioctl(*fd, MOCK_UI_DEV_SETUP, &setup) < 0) {
		fprintf(stderr, "setup failed\n");
		return -1;
	}

	if (ioctl(*fd, MOCK_UI_DEV_CREATE, 0) < 0) {
		fprintf(stderr, "couldn't created device\n");
		return -1;
	}

	return 0;
}

int main(int argc, char **argv, char **envp)
{
	const char *name = "smolinput-mock";
	int __mock_cleanup_device fd = -1;

	(void) argc;
	(void) argv;
	(void) envp;

	if (mock_create(name, &fd))
		return 1;

	return 0;
}
