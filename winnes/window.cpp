#include <Windows.h>
#include "window.h"
#include "resource.h"
#include <cnes.h>

//HWND hwnd;

unsigned int keymap[16] = { 'S', 'A', 'Q', 'W', VK_UP, VK_DOWN, VK_LEFT,  VK_RIGHT, 'L', 'K', 'I', 'O', 'T', 'G', 'F', 'H' };


HMENU menu;

bool needs_resize;
int new_size;
unsigned int new_width;
unsigned int new_height;

#define COMMAND_OPEN 1
#define COMMAND_RESET 2
#define COMMAND_ABOUT 3
#define COMMAND_SAVE_STATE 4
#define COMMAND_RESTORE_STATE 5
#define COMMAND_TOGGLE_JOY 6

extern void load_ines_from_file(const char* path);

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	Window* self = (Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (message) {
		case WM_CREATE:
		{
			menu = CreateMenu();

			HMENU fileMenu = CreateMenu();
			AppendMenu(menu, MF_STRING | MF_POPUP, (UINT_PTR)fileMenu, "File");
			AppendMenu(fileMenu, MF_STRING, COMMAND_OPEN, "&Open...");

			HMENU optionsMenu = CreateMenu();
			AppendMenu(menu, MF_STRING | MF_POPUP, (UINT_PTR)optionsMenu, "Options");
			AppendMenu(optionsMenu, MF_STRING | MF_CHECKED, COMMAND_TOGGLE_JOY, "Joystick");

			HMENU emuMenu = CreateMenu();
			AppendMenu(menu, MF_STRING | MF_POPUP, (UINT_PTR)emuMenu, "Emulation");
			AppendMenu(emuMenu, MF_STRING, COMMAND_RESET, "Reset");
			AppendMenu(emuMenu, MF_STRING, COMMAND_SAVE_STATE, "Save state");
			AppendMenu(emuMenu, MF_STRING, COMMAND_RESTORE_STATE, "Restore state");

			//EnableMenuItem(emuMenu, COMMAND_RESTORE_STATE, has_save_state ? MF_ENABLED : MF_GRAYED);


			HMENU helpMenu = CreateMenu();
			AppendMenu(menu, MF_STRING | MF_POPUP, (UINT_PTR)helpMenu, "Help");
			AppendMenu(helpMenu, MF_STRING, COMMAND_ABOUT, "About...");

			SetMenu(hWnd, menu);
			DrawMenuBar(hWnd);
		}
		break;
		case WM_COMMAND:
		{
			WORD which = LOWORD(wParam);
			switch (which) {
				case COMMAND_OPEN:
				{
					char szFile[100] = { 0 };
					OPENFILENAME openFile = { 0 };
					openFile.lStructSize = sizeof(OPENFILENAME);
					openFile.hwndOwner = hWnd;
					openFile.lpstrFilter = "NES Roms (*.nes)\0*.nes\0";
					openFile.lpstrFile = szFile;
					openFile.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
					openFile.nMaxFile = sizeof(szFile);
					if (GetOpenFileName(&openFile)) {
						load_ines_from_file(szFile);
						//EnableMenuItem(emuMenu, COMMAND_RESTORE_STATE, has_save_state ? MF_ENABLED : MF_GRAYED);
					}
				}
				break;
				case COMMAND_RESET:
					//main_reset();
					self->onReset();
					break;
				case COMMAND_ABOUT:
					MessageBox(hWnd, "WinNES 0.1 by r1sc 2023\n\nSupported mappers:\nNROM\nUNROM\nMMC1", "About WinNES", MB_ICONINFORMATION);
					break;
				case COMMAND_SAVE_STATE:
					self->onSaveState();
					//main_save_state();
					break;
				case COMMAND_RESTORE_STATE:
					self->onLoadState();
					//main_load_state();
					break;
				case COMMAND_TOGGLE_JOY:

					break;
			}
		}
		break;
		case WM_CLOSE:
		{
			PostQuitMessage(0);
		}
		break;
		case WM_SIZE:
		{
			new_width = LOWORD(lParam);
			new_height = HIWORD(lParam);

			new_size = new_width;
			if (new_width > new_height) {
				new_size = new_height;
			}
			needs_resize = true;
		}
		break;
		case WM_KEYDOWN:
		{
			for (size_t i = 0; i < 16; i++) {
				if (keymap[i] == wParam) {
					uint8_t controller_id = (i & 8) == 8;
					buttons_down[controller_id] |= (1 << (i & 7));
				}
			}
		}
		break;
		case WM_KEYUP:
		{
			for (size_t i = 0; i < 16; i++) {
				if (keymap[i] == wParam) {
					uint8_t controller_id = (i & 8) == 8;
					buttons_down[controller_id] &= ~(1 << (i & 7));
				}
			}
		}
		break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

//void create_window() {
//	HINSTANCE hInstance = GetModuleHandleA(NULL);
//	WNDCLASS wc = { 0 };
//	wc.lpfnWndProc = WndProc;
//	wc.hInstance = hInstance;
//	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
//	wc.lpszClassName = "WinNES";
//	wc.style = CS_OWNDC;
//	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
//	if (!RegisterClass(&wc)) {
//		exit(1);
//		return;
//	}
//
//	RECT rect = { 0, 0, 512, 512 };
//	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, true);
//	LONG width = rect.right - rect.left;
//	LONG height = rect.bottom - rect.top;
//
//	hwnd = CreateWindow(
//		wc.lpszClassName,
//		"WinNES",
//		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
//		GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2,
//		GetSystemMetrics(SM_CYSCREEN) / 2 - height / 2,
//		width, height, 0, 0, hInstance, 0);
//}


Window::Window(std::function<void()> onReset,
	std::function<void()> onLoadState,
	std::function<void()> onSaveState)
	: onReset(onReset),
	onLoadState(onLoadState),
	onSaveState(onSaveState) {
	HINSTANCE hInstance = GetModuleHandleA(NULL);
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	wc.lpszClassName = "WinNES";
	wc.style = CS_OWNDC;
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	if (!RegisterClass(&wc)) {
		throw "Cannot register class";
	}

	RECT rect = { 0, 0, 512, 512 };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, true);
	LONG width = rect.right - rect.left;
	LONG height = rect.bottom - rect.top;

	hwnd = CreateWindow(
		wc.lpszClassName,
		"WinNES",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2,
		GetSystemMetrics(SM_CYSCREEN) / 2 - height / 2,
		width, height, 0, 0, hInstance, 0);

	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
}