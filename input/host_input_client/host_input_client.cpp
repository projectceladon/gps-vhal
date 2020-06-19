#include <stdio.h>
#include <iostream>
#include "DirectInput.h"
#include <string>

using namespace std;
void test_button_1(DirectInputReceiver *device)
{
	printf("\n\n\n\t%s:%d BUTTON_1\n", __func__, __LINE__);
	device->onJoystickMessage("k 288 1\n"); // BUTTON_1 Down
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 288 0\n"); // BUTTON_1 Up
	device->onJoystickMessage("c\n");
}

void test_button_mode(DirectInputReceiver *device)
{
	printf("\n\n\n\t%s:%d BUTTON_MODE\n", __func__, __LINE__);
	device->onJoystickMessage("k 316 1\n"); // BUTTON_MODE Down
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 316 0\n"); // BUTTON_MODE Up
	device->onJoystickMessage("c\n");
}

void test_joystick(DirectInputReceiver *device)
{
	printf("\n\n\n\t%s:%d BTN_TL\n", __func__, __LINE__);
	device->onJoystickMessage("k 310 1\n"); // BTN_TL Down
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 310 0\n"); // BTN_TL Up
	device->onJoystickMessage("c\n");

	usleep(1 * 1000 * 1000);

	printf("\n\n\n\t%s:%d BTN_TR\n", __func__, __LINE__);
	device->onJoystickMessage("k 311 1\n"); // BTN_TR Down
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 311 0\n"); // BTN_TR Up
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

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

	usleep(1 * 1000 * 1000);

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
	usleep(1 * 1000 * 1000);

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

	usleep(1 * 1000 * 1000);

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

	usleep(1 * 1000 * 1000);

	printf("\n\n\n\t%s:%d BTN_SELECT\n", __func__, __LINE__);
	device->onJoystickMessage("k 314 1\n"); // BTN_SELECT Down
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 314 0\n"); // BTN_SELECT Up
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	printf("\n\n\n\t%s:%d BTN_START\n", __func__, __LINE__);
	device->onJoystickMessage("k 315 1\n"); // BTN_START Down
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 315 0\n"); // BTN_START Up
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	printf("\n\n\n\t%s:%d BTN_THUMBL\n", __func__, __LINE__);
	device->onJoystickMessage("k 317 1\n"); // BTN_THUMBL Down
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 317 0\n"); // BTN_THUMBL Up
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	printf("\n\n\n\t%s:%d BTN_THUMBR\n", __func__, __LINE__);
	device->onJoystickMessage("k 318 1\n"); // BTN_THUMBR Down
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 318 0\n"); // BTN_THUMBR Up
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	// BTN_Y
	printf("\n\n\n\t%s:%d BTN_Y\n", __func__, __LINE__);
	device->onJoystickMessage("k 308 1\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 308 0\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	// BTN_X
	printf("\n\n\n\t%s:%d BTN_X\n", __func__, __LINE__);
	device->onJoystickMessage("k 307 1\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 307 0\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	// BTN_A
	printf("\n\n\n\t%s:%d BTN_A\n", __func__, __LINE__);
	device->onJoystickMessage("k 304 1\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 304 0\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	// BTN_B
	printf("\n\n\n\t%s:%d BTN_B\n", __func__, __LINE__);
	device->onJoystickMessage("k 305 1\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("k 305 0\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	// Directional pad ABS_HAT0Y North
	printf("\n\n\n\t%s:%d Directional pad ABS_HAT0Y North\n", __func__, __LINE__);
	device->onJoystickMessage("a 17 -1\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("a 17 0\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	// Directional pad ABS_HAT0Y South
	printf("\n\n\n\t%s:%d Directional pad ABS_HAT0Y South\n", __func__, __LINE__);
	device->onJoystickMessage("a 17 1\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("a 17 0\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	// Directional pad ABS_HAT0X West
	printf("\n\n\n\t%s:%d Directional pad ABS_HAT0X West\n", __func__, __LINE__);
	device->onJoystickMessage("a 16 -1\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("a 16 0\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	// Directional pad ABS_HAT0X East
	printf("\n\n\n\t%s:%d Directional pad ABS_HAT0X East\n", __func__, __LINE__);
	device->onJoystickMessage("a 16 1\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);
	device->onJoystickMessage("a 16 0\n");
	device->onJoystickMessage("c\n");
	usleep(1 * 1000 * 1000);

	// Right Stick West
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
	usleep(1 * 1000 * 1000);
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
	usleep(1 * 1000 * 1000);

	// Right Stick East
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
	usleep(1 * 1000 * 1000);
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
	usleep(1 * 1000 * 1000);

	// Right Stick North
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
	usleep(1 * 1000 * 1000);
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
	usleep(1 * 1000 * 1000);

	// Right Stick South
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
	usleep(1 * 1000 * 1000);
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
	usleep(1 * 1000 * 1000);

	// Left Stick West
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
	usleep(1 * 1000 * 1000);
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
	usleep(1 * 1000 * 1000);

	// Left Stick East
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
	usleep(1 * 1000 * 1000);
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
	usleep(1 * 1000 * 1000);

	// Left Stick North
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
	usleep(1 * 1000 * 1000);
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
	usleep(1 * 1000 * 1000);

	// Left Stick South
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
	usleep(1 * 1000 * 1000);
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
	usleep(1 * 1000 * 1000);
}

void test_touch(DirectInputReceiver *device)
{
	// Touch Event
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

int main()
{
	DirectInputReceiver *device = new DirectInputReceiver(0);
	int select = 0;
	printf("\t%s:%d Remote input test:\n", __func__, __LINE__);

	do
	{
		printf("\n\n\n\t%s:%d \n"
			   "\t      1: Test BUTTON_1. \n"
			   "\t      2: Test BUTTON_MODE. \n"
			   "\t      3. Test Joystick. \n"
			   "\t      4. Test touchscreen. \n"
			   "\t      Other number. Quit. \n"
			   "\t      Please select:",
			   __func__, __LINE__);
		cin >> select;
		switch (select)
		{
		case 1:
			test_button_1(device);
			break;
		case 2:
			test_button_mode(device);
			break;
		case 3:
			test_joystick(device);
			break;
		case 4:
			test_touch(device);
			break;
		default:
			printf("\t%s:%d Quit\n", __func__, __LINE__);
			break;
		}
	} while (select >= 1 && select <= 4);
	return 0;
}
