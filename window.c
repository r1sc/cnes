#include <Windows.h>
#include "fake6502.h"
#include "window.h"

HWND hwnd;
HDC window_dc;

unsigned int keymap[8] = { 'X', 'Z', 'A', 'S', VK_UP, VK_DOWN, VK_LEFT,  VK_RIGHT };
uint8_t keysdown;

HMENU menu;

extern void load_ines(char* path);
extern void reset_machine();

bool needs_resize;
int new_size;
unsigned int new_width;
unsigned int new_height;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
		{
			menu = CreateMenu();

			HMENU fileMenu = CreateMenu();
			AppendMenu(menu, MF_STRING | MF_POPUP, (UINT)fileMenu, "File");
			AppendMenu(fileMenu, MF_STRING, 2, "&Open...");
			
			HMENU emuMenu = CreateMenu();
			AppendMenu(menu, MF_STRING | MF_POPUP, (UINT)emuMenu, "Emulation");
			AppendMenu(emuMenu, MF_STRING, 3, "Reset");

			SetMenu(hWnd, menu);
			DrawMenuBar(hWnd);
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
			for (size_t i = 0; i < 8; i++) {
				if (keymap[i] == wParam) {
					keysdown |= (1 << i);
				}
			}
		}
		break;
		case WM_KEYUP:
		{
			for (size_t i = 0; i < 8; i++) {
				if (keymap[i] == wParam) {
					keysdown &= ~(1 << i);
				}
			}
		}
		break;
		case WM_COMMAND:
		{
			WORD which = LOWORD(wParam);
			if (which == 2) {
				char szFile[100] = { 0 };
				OPENFILENAME o = { 0 };
				o.lStructSize = sizeof(OPENFILENAME);
				o.hwndOwner = hWnd;
				o.lpstrFilter = "NES Roms (*.nes)\0*.nes\0";
				o.lpstrFile = szFile;
				o.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
				o.nMaxFile = sizeof(szFile);
				if (GetOpenFileName(&o)) {
					load_ines(szFile);
				}
			} else if (which == 3) {				
				reset_machine();
			}
		}
		break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

void create_window() {
	HINSTANCE hInstance = GetModuleHandleA(NULL);
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	wc.lpszClassName = "cnes";
	wc.style = CS_OWNDC;
	if (!RegisterClass(&wc)) {
		exit(1);
		return;
	}

	RECT rect = { 0, 0, 512, 512 };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, true);
	LONG width = rect.right - rect.left;
	LONG height = rect.bottom - rect.top;

	hwnd = CreateWindow(
		wc.lpszClassName, 
		"cnes", 
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
		GetSystemMetrics(SM_CXSCREEN) / 2 - width / 2, 
		GetSystemMetrics(SM_CYSCREEN) / 2 - height / 2, 
		width, height, 0, 0, hInstance, 0);

	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
		PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
		32,                   // Colordepth of the framebuffer.
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		24,                   // Number of bits for the depthbuffer
		8,                    // Number of bits for the stencilbuffer
		0,                    // Number of Aux buffers in the framebuffer.
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	window_dc = GetDC(hwnd);

	int  letWindowsChooseThisPixelFormat;
	letWindowsChooseThisPixelFormat = ChoosePixelFormat(window_dc, &pfd);
	SetPixelFormat(window_dc, letWindowsChooseThisPixelFormat, &pfd);
}
