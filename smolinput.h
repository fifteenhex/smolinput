#ifndef __SMOLINPUT_H_
#define __SMOLINPUT_H_

#include <linux/ioctl.h>
#include <linux/input-event-codes.h>

#define SMOLINPUT_NAMESZ	64
#define SMOLINPUT_MAXDEVICES	32

#ifndef SMOLINPUT_DEVICEPATH
#define SMOLINPUT_DEVICEPATH	"/dev/input/event"
#endif

/* linux/input.h can't be used with nolibc right now */
#define SMOLINPUT_EVIOCGNAME(_len)	_IOC(_IOC_READ, 'E', 0x06, _len)
#define SMOLINPUT_EVIOCGKEY(_len)	_IOC(_IOC_READ, 'E', 0x18, _len)
#define SMOLINPUT_EVIOCGBIT(_ev, _len)	_IOC(_IOC_READ, 'E', 0x20 + (_ev), _len)
#define SMOLINPUT_EVIOCGRAB		_IOW('E', 0x90, int)

#define SMOLINPUT_BITSPERLONG		(8 * sizeof(unsigned long))
#define SMOLINPUT_LONGSFORBITS(_n)	(((_n) + SMOLINPUT_BITSPERLONG - 1) / SMOLINPUT_BITSPERLONG)

struct smolinput_event {
	unsigned long sec;
	unsigned long usec;
	uint16_t type;
	uint16_t code;
	int32_t value;
};

enum smolinput_keystate {
	SMOLINPUT_RELEASED = 0,
	SMOLINPUT_PRESSED = 1,
	SMOLINPUT_REPEATED = 2,
};

struct smolinput_key {
	uint16_t code;
	enum smolinput_keystate state;
};

struct smolinput_device {
	unsigned int index;
	int score;
	char name[SMOLINPUT_NAMESZ];
};

struct smolinput_keyboard {
	int fd;
	unsigned int index;
	char name[SMOLINPUT_NAMESZ];
	unsigned long held[SMOLINPUT_LONGSFORBITS(KEY_CNT)];
};

static int smolinput_testbit(const unsigned long *bits, unsigned int bit)
{
	return (bits[bit / SMOLINPUT_BITSPERLONG] >>
		(bit % SMOLINPUT_BITSPERLONG)) & 1ul;
}

static void smolinput_setbit(unsigned long *bits, unsigned int bit, int to)
{
	unsigned long mask = 1ul << (bit % SMOLINPUT_BITSPERLONG);

	if (to)
		bits[bit / SMOLINPUT_BITSPERLONG] |= mask;
	else
		bits[bit / SMOLINPUT_BITSPERLONG] &= ~mask;
}

static unsigned int smolinput_countbits(const unsigned long *bits,
					unsigned int first, unsigned int last)
{
	unsigned int bit, count = 0;

	for (bit = first; bit <= last; bit++)
		count += smolinput_testbit(bits, bit) ? 1 : 0;

	return count;
}

static int smolinput_getbits(int fd, unsigned int ev, unsigned long *bits, size_t sz)
{
	int ret;

	memset(bits, 0, sz);

	ret = ioctl(fd, SMOLINPUT_EVIOCGBIT(ev, sz), bits);
	if (ret < 0)
		return ret;

	return 0;
}

static int smolinput_getname(int fd, char *name, size_t sz)
{
	int ret;

	memset(name, 0, sz);

	ret = ioctl(fd, SMOLINPUT_EVIOCGNAME(sz - 1), name);
	if (ret < 0)
		return ret;

	return 0;
}

/* Couldn't work out how to do this, asked claude, recheck! */
static int smolinput_score(const unsigned long *evbits, const unsigned long *keybits)
{
	unsigned int letters;
	int score;

	if (!smolinput_testbit(evbits, EV_KEY))
		return -1;

	letters = smolinput_countbits(keybits, KEY_Q, KEY_P) +
		  smolinput_countbits(keybits, KEY_A, KEY_L) +
		  smolinput_countbits(keybits, KEY_Z, KEY_M);

	if (letters < 20)
		return -1;

	score = (int)letters * 2;

	/* the ones you cannot do without */
	score += smolinput_testbit(keybits, KEY_ENTER) ? 2 : 0;
	score += smolinput_testbit(keybits, KEY_SPACE) ? 2 : 0;
	score += smolinput_testbit(keybits, KEY_ESC) ? 2 : 0;

	/* a real keyboard repeats */
	score += smolinput_testbit(evbits, EV_REP) ? 4 : 0;

	/* and does not move the mouse */
	score -= smolinput_testbit(evbits, EV_REL) ? 5 : 0;
	score -= smolinput_testbit(evbits, EV_ABS) ? 5 : 0;

	return score;
}

static int smolinput_scorefd(int fd)
{
	unsigned long evbits[SMOLINPUT_LONGSFORBITS(EV_CNT)];
	unsigned long keybits[SMOLINPUT_LONGSFORBITS(KEY_CNT)];

	if (smolinput_getbits(fd, 0, evbits, sizeof(evbits)))
		return -1;

	if (smolinput_getbits(fd, EV_KEY, keybits, sizeof(keybits)))
		return -1;

	return smolinput_score(evbits, keybits);
}

static int smolinput_devicepath(char *path, size_t sz, unsigned int index)
{
	return snprintf(path, sz, "%s%u", SMOLINPUT_DEVICEPATH, index);
}

static void smolinput_insert(struct smolinput_device *devices, unsigned int found,
			     const struct smolinput_device *device)
{
	unsigned int i;

	for (i = found; i > 0 && devices[i - 1].score < device->score; i--)
		devices[i] = devices[i - 1];

	devices[i] = *device;
}

static int smolinput_enumerate(struct smolinput_device *devices, unsigned int max)
{
	unsigned int index, found = 0;

	for (index = 0; index < SMOLINPUT_MAXDEVICES && found < max; index++) {
		char path[sizeof(SMOLINPUT_DEVICEPATH) + 12];
		struct smolinput_device device;
		int fd, score;

		smolinput_devicepath(path, sizeof(path), index);

		fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;

		score = smolinput_scorefd(fd);
		if (score >= 0) {
			device.index = index;
			device.score = score;
			smolinput_getname(fd, device.name, sizeof(device.name));

			smolinput_insert(devices, found, &device);
			found++;
		}

		close(fd);
	}

	return (int)found;
}

static void smolinput_close(struct smolinput_keyboard *keyboard)
{
	if (keyboard->fd >= 0)
		close(keyboard->fd);

	keyboard->fd = -1;
}

#define __smolinput_cleanup_keyboard __attribute__((cleanup(smolinput_close)))

static int smolinput_open(struct smolinput_keyboard *keyboard, const char *path)
{
	char found[sizeof(SMOLINPUT_DEVICEPATH) + 12];
	unsigned int index = 0;
	int fd;

	memset(keyboard, 0, sizeof(*keyboard));
	keyboard->fd = -1;

	if (!path) {
		struct smolinput_device devices[SMOLINPUT_MAXDEVICES];
		int n = smolinput_enumerate(devices, SMOLINPUT_MAXDEVICES);

		if (n <= 0)
			return -ENODEV;

		index = devices[0].index;
		smolinput_devicepath(found, sizeof(found), index);
		path = found;
	}

	fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (fd < 0)
		return -ENODEV;

	keyboard->fd = fd;
	keyboard->index = index;
	smolinput_getname(fd, keyboard->name, sizeof(keyboard->name));

	return 0;
}

static int smolinput_grab(struct smolinput_keyboard *keyboard, int grab)
{
	int ret;

	ret = ioctl(keyboard->fd, SMOLINPUT_EVIOCGRAB, grab ? 1 : 0);

	return ret;
}

static int smolinput_readkey(struct smolinput_keyboard *keyboard,
			     struct smolinput_key *key)
{
	struct smolinput_event event;
	ssize_t ret;

	for (;;) {
		ret = read(keyboard->fd, &event, sizeof(event));
		if (ret < 0)
			return errno == EAGAIN ? 0 : -errno;

		if (ret == 0)
			return 0;

		if (ret != (ssize_t)sizeof(event))
			return -EIO;

		if (event.type != EV_KEY)
			continue;

		key->code = event.code;
		key->state = (enum smolinput_keystate)event.value;

		if (event.code < KEY_CNT)
			smolinput_setbit(keyboard->held, event.code,
					 event.value != SMOLINPUT_RELEASED);

		return 1;
	}
}

/* 0 == non-blocking  */
static int smolinput_waitkey(struct smolinput_keyboard *keyboard,
			     struct smolinput_key *key, int timeout)
{
	struct pollfd fds = {
		.fd = keyboard->fd,
		.events = POLLIN,
	};
	int ret;

	ret = poll(&fds, 1, timeout);
	if (ret <= 0)
		return ret;

	return smolinput_readkey(keyboard, key);
}

static int smolinput_sync(struct smolinput_keyboard *keyboard)
{
	int ret;

	ret = ioctl(keyboard->fd, SMOLINPUT_EVIOCGKEY(sizeof(keyboard->held)),
		    keyboard->held);
	if (ret < 0)
		return ret;

	return 0;
}

static int smolinput_isdown(const struct smolinput_keyboard *keyboard, uint16_t code)
{
	if (code >= KEY_CNT)
		return 0;

	return smolinput_testbit(keyboard->held, code);
}

#endif /* __SMOLINPUT_H_ */
