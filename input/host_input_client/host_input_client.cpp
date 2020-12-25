#include <stdio.h>
#include <iostream>
#include <getopt.h>
#include "DirectInput.h"
#include <string>

using namespace std;
int debug = 0x2;
void test_button_1(DirectInputReceiver *device)
{
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BUTTON_1\n", __func__, __LINE__);
	device->onJoystickMessage("k 288 1\n"); // BUTTON_1 Down
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 288 0\n"); // BUTTON_1 Up
	device->onJoystickMessage("c\n");
}

void test_button_mode(DirectInputReceiver *device)
{
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BUTTON_MODE\n", __func__, __LINE__);
	device->onJoystickMessage("k 316 1\n"); // BUTTON_MODE Down
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 316 0\n"); // BUTTON_MODE Up
	device->onJoystickMessage("c\n");
}

void test_joystick(DirectInputReceiver *device)
{
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_TL\n", __func__, __LINE__);
	device->onJoystickMessage("k 310 1\n"); // BTN_TL Down
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 310 0\n"); // BTN_TL Up
	device->onJoystickMessage("c\n");

	usleep(200 * 1000);

	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_TR\n", __func__, __LINE__);
	device->onJoystickMessage("k 311 1\n"); // BTN_TR Down
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 311 0\n"); // BTN_TR Up
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_TL2\n", __func__, __LINE__);
	device->onJoystickMessage("k 312 1\n"); // BTN_TL2 Down
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 62 0\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 62 50\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 62 100\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 62 150\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 62 200\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 62 255\n");
	device->onJoystickMessage("c\n");

	usleep(200 * 1000);

	device->onJoystickMessage("a 62 200\n");
	device->onJoystickMessage("c\n");
	usleep(40000);

	device->onJoystickMessage("a 62 150\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 62 100\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 62 50\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 62 0\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("k 312 0\n"); // BTN_TL2 Up
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_TR2\n", __func__, __LINE__);
	device->onJoystickMessage("k 313 1\n"); // BTN_TR2 Down
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 63 0\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 63 50\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 63 100\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 63 150\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 63 200\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 63 255\n");
	device->onJoystickMessage("c\n");

	usleep(200 * 1000);

	device->onJoystickMessage("a 63 200\n");
	device->onJoystickMessage("c\n");
	usleep(40000);

	device->onJoystickMessage("a 63 150\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 63 100\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 63 50\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("a 63 0\n");
	device->onJoystickMessage("c\n");
	usleep(40000);
	device->onJoystickMessage("k 313 0\n"); // BTN_TR2 Up
	device->onJoystickMessage("c\n");

	usleep(200 * 1000);

	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_SELECT\n", __func__, __LINE__);
	device->onJoystickMessage("k 314 1\n"); // BTN_SELECT Down
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 314 0\n"); // BTN_SELECT Up
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_START\n", __func__, __LINE__);
	device->onJoystickMessage("k 315 1\n"); // BTN_START Down
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 315 0\n"); // BTN_START Up
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_THUMBL\n", __func__, __LINE__);
	device->onJoystickMessage("k 317 1\n"); // BTN_THUMBL Down
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 317 0\n"); // BTN_THUMBL Up
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_THUMBR\n", __func__, __LINE__);
	device->onJoystickMessage("k 318 1\n"); // BTN_THUMBR Down
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 318 0\n"); // BTN_THUMBR Up
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// BTN_Y
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_Y\n", __func__, __LINE__);
	device->onJoystickMessage("k 308 1\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 308 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// BTN_X
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_X\n", __func__, __LINE__);
	device->onJoystickMessage("k 307 1\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 307 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// BTN_A
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_A\n", __func__, __LINE__);
	device->onJoystickMessage("k 304 1\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 304 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// BTN_B
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d BTN_B\n", __func__, __LINE__);
	device->onJoystickMessage("k 305 1\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("k 305 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Directional pad ABS_HAT0Y North
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Directional pad ABS_HAT0Y North\n", __func__, __LINE__);
	device->onJoystickMessage("a 17 -1\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 17 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Directional pad ABS_HAT0Y South
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Directional pad ABS_HAT0Y South\n", __func__, __LINE__);
	device->onJoystickMessage("a 17 1\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 17 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Directional pad ABS_HAT0X West
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Directional pad ABS_HAT0X West\n", __func__, __LINE__);
	device->onJoystickMessage("a 16 -1\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 16 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Directional pad ABS_HAT0X East
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Directional pad ABS_HAT0X East\n", __func__, __LINE__);
	device->onJoystickMessage("a 16 1\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 16 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Right Stick West
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Right Stick West\n", __func__, __LINE__);
	device->onJoystickMessage("a 2 0\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 -31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 -63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 -95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 -127\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 2 -95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 -63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 -31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Right Stick East
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Right Stick East\n", __func__, __LINE__);
	device->onJoystickMessage("a 2 0\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 127\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 2 95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 2 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Right Stick North
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Right Stick North\n", __func__, __LINE__);
	device->onJoystickMessage("a 5 0\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 -31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 -63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 -95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 -127\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 5 -95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 -63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 -31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Right Stick South
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Right Stick South\n", __func__, __LINE__);
	device->onJoystickMessage("a 5 0\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 127\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 5 95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 5 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Left Stick West
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Left Stick West\n", __func__, __LINE__);
	device->onJoystickMessage("a 0 0\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 -31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 -63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 -95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 -127\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 0 -95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 -63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 -31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Left Stick East
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Left Stick East\n", __func__, __LINE__);
	device->onJoystickMessage("a 0 0\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 127\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 0 95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 0 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Left Stick North
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Left Stick North\n", __func__, __LINE__);
	device->onJoystickMessage("a 1 0\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 -31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 -63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 -95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 -127\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 1 -95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 -63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 -31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);

	// Left Stick South
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Left Stick South\n", __func__, __LINE__);
	device->onJoystickMessage("a 1 0\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 127\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
	device->onJoystickMessage("a 1 95\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 63\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 31\n");
	device->onJoystickMessage("c\n");
	usleep(10000);
	device->onJoystickMessage("a 1 0\n");
	device->onJoystickMessage("c\n");
	usleep(200 * 1000);
}

void test_touch(DirectInputReceiver *device)
{
	// Touch Event
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d Touch Event\n", __func__, __LINE__);
	device->onInputMessage("d 0 710 500 200\n");
	device->onInputMessage("c\n");
	usleep(10000);

	device->onInputMessage("m 0 710 480 200\n");
	device->onInputMessage("c\n");
	usleep(10000);

	device->onInputMessage("m 0 710 460 200\n");
	device->onInputMessage("c\n");
	usleep(10000);

	device->onInputMessage("m 0 710 440 200\n");
	device->onInputMessage("c\n");
	usleep(10000);

	device->onInputMessage("m 0 710 420 200\n");
	device->onInputMessage("c\n");
	usleep(10000);

	device->onInputMessage("m 0 710 400 200\n");
	device->onInputMessage("c\n");
	usleep(10000);

	device->onInputMessage("m 0 710 380 200\n");
	device->onInputMessage("c\n");
	usleep(10000);

	device->onInputMessage("m 0 710 360 200\n");
	device->onInputMessage("c\n");
	usleep(10000);

	device->onInputMessage("m 0 710 340 200\n");
	device->onInputMessage("c\n");
	usleep(10000);

	device->onInputMessage("u 0\n");
	device->onInputMessage("c\n");
}

void test_enable_joystick(DirectInputReceiver *device)
{
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d enable joystick down\n", __func__, __LINE__);
	device->onJoystickMessage("k 631 1\n");
	device->onJoystickMessage("c\n");
	usleep(2000);
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d enable joystick up\n", __func__, __LINE__);
	device->onJoystickMessage("k 631 0\n");
	device->onJoystickMessage("c\n");
}

void test_disable_joystick(DirectInputReceiver *device)
{
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d disable joystick down\n", __func__, __LINE__);
	device->onJoystickMessage("k 632 1\n"); // BTN_TL Down
	device->onJoystickMessage("c\n");
	usleep(2000);
	if ((debug & 0x1) > 0)
		printf("\n\n\n\t%s:%d disable joystick up\n", __func__, __LINE__);
	device->onJoystickMessage("k 632 0\n"); // BTN_TL Down
	device->onJoystickMessage("c\n");
}

char *const short_options = "c:hi:n:";
struct option long_options[] = {
	{"cmd", 0, NULL, 'c'},
	{"help", 1, NULL, 'h'},
	{"instance", 1, NULL, 'i'},
	{"input", 1, NULL, 'n'},
	{0, 0, 0, 0},
};

int main(int argc, char *argv[])
{
	int c;
	int index = 0;
	char *p_opt_arg = NULL;
	int cmd = 0;
	int instance = 0;
	int input = 0;

	while ((c = getopt_long(argc, argv, short_options, long_options, &index)) != -1)
	{
		switch (c)
		{
		case 'c':
			p_opt_arg = optarg;
			cmd = atoi(p_opt_arg);
			if ((debug & 0x2) > 0)
				printf("cmd: %d\n", cmd);
			break;
		case 'i':
			p_opt_arg = optarg;
			instance = atoi(p_opt_arg);
			if ((debug & 0x2) > 0)
				printf("instance: %d\n", instance);
			break;
		case 'n':
			p_opt_arg = optarg;
			input = atoi(p_opt_arg);
			if ((debug & 0x2) > 0)
				printf("input: %d\n", input);
			break;
		case 'h':
			if ((debug & 0x2) > 0)
				printf("%s\n"
					   "\t-c, --cmd cmd\n"
					   "\t      1: Test BUTTON_1. \n"
					   "\t      2: Test BUTTON_MODE. \n"
					   "\t      4: Test Joystick. \n"
					   "\t      8: Test touchscreen. \n"
					   "\t      16: Test enable Joystick. \n"
					   "\t      32: Test disable Joystick. \n"
					   "\t-i, --instance instance ID\n",
					   "\t-n, --iNput joystick ID\n",
					   "\t-h, --help help\n",
					   argv[0]);
			break;
		default:
			if ((debug & 0x2) > 0)
				printf("Nock: c = %c, index =%d \n", c, index);
		}
	}
	DirectInputReceiver *device = new DirectInputReceiver(instance, input);
	if ((debug & 0x1) > 0)
		printf("\t%s:%d Remote input test:\n", __func__, __LINE__);

	if ((cmd & 0x1) > 0)
		test_button_1(device);

	if ((cmd & 0x2) > 0)
		test_button_mode(device);

	if ((cmd & 0x4) > 0)
		test_joystick(device);

	if ((cmd & 0x8) > 0)
		test_touch(device);

	if ((cmd & 0x10) > 0)
		test_enable_joystick(device);

	if ((cmd & 0x20) > 0)
		test_disable_joystick(device);

	return 0;
}
