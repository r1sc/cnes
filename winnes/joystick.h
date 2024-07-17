#ifndef _JOYSTICK_H_
#define _JOYSTICK_H_

#include <stdint.h>
#include <Windows.h>

void poll_joystick(uint8_t joystick_id);
void poll_xinput_joy(DWORD joystick_id);

#endif