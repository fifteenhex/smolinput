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
#define MOCK_UI_GET_SYSNAME(_len) _IOC(_IOC_READ, 'U', 44, _len)

#define MOCK_BUS_VIRTUAL	0x06

#ifndef MOCK_UINPUT_PATH
#define MOCK_UINPUT_PATH	"/dev/uinput"
#endif

/* All of the keys we claim to have, with names */
static const struct {
	const char *name;
	uint16_t code;
} mock_keys[] = {
	{ "up", KEY_UP },
	{ "down", KEY_DOWN },
	{ "left", KEY_LEFT },
	{ "right", KEY_RIGHT },
	{ "a", KEY_A },
	{ "b", KEY_B },
	{ "c", KEY_C },
	{ "d", KEY_D },
	{ "e", KEY_E },
	{ "f", KEY_F },
	{ "g", KEY_G },
	{ "h", KEY_H },
	{ "i", KEY_I },
	{ "j", KEY_J },
	{ "k", KEY_K },
	{ "l", KEY_L },
	{ "m", KEY_M },
	{ "n", KEY_N },
	{ "o", KEY_O },
	{ "p", KEY_P },
	{ "q", KEY_Q },
	{ "r", KEY_R },
	{ "s", KEY_S },
	{ "t", KEY_T },
	{ "u", KEY_U },
	{ "v", KEY_V },
	{ "w", KEY_W },
	{ "x", KEY_X },
	{ "y", KEY_Y },
	{ "z", KEY_Z },
	{ "0", KEY_0 },
	{ "1", KEY_1 },
	{ "2", KEY_2 },
	{ "3", KEY_3 },
	{ "4", KEY_4 },
	{ "5", KEY_5 },
	{ "6", KEY_6 },
	{ "7", KEY_7 },
	{ "8", KEY_8 },
	{ "9", KEY_9 },
	{ "enter", KEY_ENTER },
	{ "start", KEY_ENTER },
	{ "esc", KEY_ESC },
	{ "select", KEY_ESC },
	{ "menu", KEY_MENU },
	{ "power", KEY_POWER },
	{ "space", KEY_SPACE },
	{ "tab", KEY_TAB },
	{ "backspace", KEY_BACKSPACE },
	{ "shift", KEY_LEFTSHIFT },
};

/* 0 == dunno */
static uint16_t mock_keycode(const char *what)
{
	unsigned int code;
	unsigned int i;

	for (i = 0; i < SMOLINPUT_ARRAYSZ(mock_keys); i++)
		if (!strcmp(what, mock_keys[i].name))
			return mock_keys[i].code;

	code = (unsigned int) atoi(what);

	return code && code < KEY_CNT ? (uint16_t) code : 0;
}

static void mock_sleep_ms(unsigned int ms)
{
	struct timespec ts = {
		.tv_sec = ms / 1000,
		.tv_nsec = (long) (ms % 1000) * 1000000,
	};

	if (ms)
		nanosleep(&ts, NULL);
}

static int mock_write_event(int fd, uint16_t type, uint16_t code, int32_t value)
{
	struct smolinput_event event = {
		.type = type,
		.code = code,
		.value = value,
	};

	if (write(fd, &event, sizeof(event)) != (ssize_t) sizeof(event))
		return -1;

	return 0;
}

/* Do a press, hold, release */
static int mock_tap(int fd, uint16_t code, unsigned int hold_ms)
{
	if (mock_write_event(fd, EV_KEY, code, 1) ||
	    mock_write_event(fd, EV_SYN, SYN_REPORT, 0))
		return -1;

	mock_sleep_ms(hold_ms);

	if (mock_write_event(fd, EV_KEY, code, 0) ||
	    mock_write_event(fd, EV_SYN, SYN_REPORT, 0))
		return -1;

	return 0;
}

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
		if (ioctl(*fd, MOCK_UI_SET_KEYBIT, (int) mock_keys[i].code) < 0) {
			fprintf(stderr, "set keybit failed for %u\n", mock_keys[i].code);
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

/* Work out the node for our device so you can pass it to another prog */
static void mock_what_node(int fd, const char *name)
{
	char path[sizeof(SMOLINPUT_DEVICEPATH) + 12];
	char sysname[32] = { 0 };
	unsigned int i;
	char dir[128];

	if (ioctl(fd, MOCK_UI_GET_SYSNAME(sizeof(sysname) - 1), sysname) < 0) {
		fprintf(stderr, "get sysname failed\n");
		return;
	}

	for (i = 0; i < SMOLINPUT_MAXDEVICES; i++) {
		snprintf(dir, sizeof(dir), SMOLINPUT_SYSFSPATH "%s/event%u", sysname, i);
		if (access(dir, F_OK))
			continue;

		smolinput_devicepath(path, sizeof(path), i);
		printf("%s is %s\n", name, path);
		return;
	}

	fprintf(stderr, "no event node under " SMOLINPUT_SYSFSPATH "%s for %s\n", sysname, name);
}

/* A key per line from stdin until EOF, for pressing buttons over a serial console */
static int mock_from_stdin(int fd, unsigned int hold_ms)
{
	unsigned int len = 0;
	char line[64];

	for (;;) {
		uint16_t code;
		ssize_t got;
		char c;

		got = read(0, &c, 1);
		if (got <= 0)
			return 0;

		if (c != '\n' && c != '\r') {
			if (len < sizeof(line) - 1)
				line[len++] = c;
			continue;
		}

		line[len] = 0;
		len = 0;

		if (!line[0])
			continue;

		/* This is a hack to allow exiting if your shell is a bit broken :) */
		if (!strcmp(line, "getmeouttahere"))
			return 0;

		code = mock_keycode(line);
		if (!code) {
			printf("not a key: %s\n", line);
			continue;
		}

		if (mock_tap(fd, code, hold_ms))
			return -1;

		printf("pressed %s (%u)\n", line, code);
	}
}

static void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [-n name] [-w ms] [-h ms] [-d ms] [-i] [key ...]\n", progname);
	fprintf(stderr, "\t-n  device name (default smolinput-mock)\n");
	fprintf(stderr, "\t-w  delay in ms between creating the device and starting input\n");
	fprintf(stderr, "\t-h  hold time in ms (default 60ms)\n");
	fprintf(stderr, "\t-d  delay between keys in ms (default 150ms)\n");
	fprintf(stderr, "\t-i  interactive mode, send a key per line on stdin, end with EOF or \"getmeouttahere\"\n");
	fprintf(stderr, "a key is a letter, a digit, a name (up down left right enter"
			" esc menu power space tab) or a keycode\n");
}

int main(int argc, char **argv, char **envp)
{
	const char *name = "smolinput-mock";
	int __mock_cleanup_device fd = -1;
	unsigned int wait_ms = 500;
	unsigned int hold_ms = 60;
	unsigned int gap_ms = 150;
	bool interactive = false;
	int i, opt;

	(void) envp;

	while ((opt = getopt(argc, argv, "n:w:h:d:i")) != -1) {
		switch (opt) {
		case 'n':
			name = optarg;
			break;
		case 'w':
			wait_ms = (unsigned int) atoi(optarg);
			break;
		case 'h':
			hold_ms = (unsigned int) atoi(optarg);
			break;
		case 'd':
			gap_ms = (unsigned int) atoi(optarg);
			break;
		case 'i':
			interactive = true;
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (!interactive && optind >= argc) {
		usage(argv[0]);
		return 1;
	}

	/* Make sure the keys are valid before doing anything */
	for (i = optind; i < argc; i++) {
		if (!mock_keycode(argv[i])) {
			fprintf(stderr, "not a key: %s\n", argv[i]);
			return 1;
		}
	}

	if (mock_create(name, &fd))
		return 1;

	mock_what_node(fd, name);
	mock_sleep_ms(wait_ms);

	for (i = optind; i < argc; i++) {
		uint16_t code = mock_keycode(argv[i]);

		if (mock_tap(fd, code, hold_ms)) {
			fprintf(stderr, "could not press %s\n", argv[i]);
			return 1;
		}

		printf("pressed %s (%u)\n", argv[i], code);

		if (i + 1 < argc)
			mock_sleep_ms(gap_ms);
	}

	if (interactive && mock_from_stdin(fd, hold_ms))
		return 1;

	return 0;
}
