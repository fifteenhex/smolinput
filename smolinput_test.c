#include "smolinput.h"

static void listdevices(void)
{
	struct smolinput_device devices[SMOLINPUT_MAXDEVICES];
	int n, i;

	n = smolinput_enumerate(devices, SMOLINPUT_MAXDEVICES);
	if (n <= 0) {
		printf("no keyboards found\n");
		return;
	}

	printf("%d keyboard%s, best first:\n", n, n == 1 ? "" : "s");

	for (i = 0; i < n; i++)
		printf("  %s%u  score %3d  %s\n", SMOLINPUT_DEVICEPATH,
		       devices[i].index, devices[i].score, devices[i].name);
}

int main(int argc, char **argv, char **envp)
{
	struct smolinput_keyboard keyboard;
	struct smolinput_key key;
	int ret;

	listdevices();

	ret = smolinput_open(&keyboard, argc > 1 ? argv[1] : NULL);
	if (ret) {
		printf("could not open a keyboard\n");
		return 1;
	}

	printf("reading %s%u, %s. escape to stop.\n", SMOLINPUT_DEVICEPATH,
	       keyboard.index, keyboard.name);

	if (smolinput_sync(&keyboard))
		printf("could not ask what is held down\n");
	else if (smolinput_isdown(&keyboard, KEY_LEFTSHIFT))
		printf("shift is being held right now\n");

	for (;;) {
		ret = smolinput_waitkey(&keyboard, &key, -1);
		if (ret < 0) {
			printf("stopped: %d\n", ret);
			break;
		}

		if (ret == 0)
			continue;

		printf("key %u %s%s\n", key.code,
		       key.state == SMOLINPUT_RELEASED ? "up" :
		       key.state == SMOLINPUT_PRESSED ? "down" : "repeat",
		       smolinput_isdown(&keyboard, KEY_LEFTSHIFT) ? " (with shift)" : "");

		if (key.code == KEY_ESC && key.state == SMOLINPUT_RELEASED)
			break;
	}

	smolinput_close(&keyboard);

	return 0;
}
