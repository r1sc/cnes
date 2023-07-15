#include <Windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <cnes.h>

JOYINFOEX joyInfo = { 
	.dwSize = sizeof(JOYINFOEX), 
	.dwFlags = JOY_RETURNBUTTONS | JOY_RETURNPOV
};

DWORD last_dwPOV[2] = { 0xFFFF, 0xFFFF };
DWORD last_dwButtons[2] = { 0,0 };

void poll_joystick(uint8_t joystick_id) {
	joyGetPosEx((UINT)joystick_id, &joyInfo);

	DWORD dwPOV = joyInfo.dwPOV;

	if (last_dwPOV[joystick_id] != dwPOV) {
		last_dwPOV[joystick_id] = dwPOV;

		bool up_down = dwPOV == 31500 || dwPOV == 0 || dwPOV == 4500;
		bool right_down = dwPOV == 4500 || dwPOV == 9000 || dwPOV == 13500;
		bool down_down = dwPOV == 13500 || dwPOV == 18000 || dwPOV == 22500;
		bool left_down = dwPOV == 22500 || dwPOV == 27000 || dwPOV == 31500;

		if (up_down) {
			buttons_down[joystick_id] |= 1 << 4;
		} else {
			buttons_down[joystick_id] &= ~(1 << 4);
		}

		if (down_down) {
			buttons_down[joystick_id] |= 1 << 5;
		} else {
			buttons_down[joystick_id] &= ~(1 << 5);
		}

		if (left_down) {
			buttons_down[joystick_id] |= 1 << 6;
		} else {
			buttons_down[joystick_id] &= ~(1 << 6);
		}

		if (right_down) {
			buttons_down[joystick_id] |= 1 << 7;
		} else {
			buttons_down[joystick_id] &= ~(1 << 7);
		}
	}

	// B = 4
	// A = 1
	// Select = 64
	// Start == 128
	DWORD dwButtons = joyInfo.dwButtons;
	if (dwButtons != last_dwButtons[joystick_id]) {
		last_dwButtons[joystick_id] = dwButtons;

		if (dwButtons & 1) {
			buttons_down[joystick_id] |= (1 << 0);
		} else {
			buttons_down[joystick_id] &= ~(1 << 0);
		}

		if (dwButtons & 4) {
			buttons_down[joystick_id] |= (1 << 1);
		} else {
			buttons_down[joystick_id] &= ~(1 << 1);
		}

		if (dwButtons & 64) {
			buttons_down[joystick_id] |= (1 << 2);
		} else {
			buttons_down[joystick_id] &= ~(1 << 2);
		}

		if (dwButtons & 128) {
			buttons_down[joystick_id] |= (1 << 3);
		} else {
			buttons_down[joystick_id] &= ~(1 << 3);
		}
	}
}