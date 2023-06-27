#include <Windows.h>
#include <gl/GL.h>

#include "window.h"

HWND hwnd;
HDC window_dc;
static HGLRC ourOpenGLRenderingContext;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
		{
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

			window_dc = GetDC(hWnd);

			int  letWindowsChooseThisPixelFormat;
			letWindowsChooseThisPixelFormat = ChoosePixelFormat(window_dc, &pfd);
			SetPixelFormat(window_dc, letWindowsChooseThisPixelFormat, &pfd);

			ourOpenGLRenderingContext = wglCreateContext(window_dc);
			wglMakeCurrent(window_dc, ourOpenGLRenderingContext);
		}
		break;
		case WM_CLOSE:
		{
			wglDeleteContext(ourOpenGLRenderingContext);
			PostQuitMessage(0);
		}
		break;
		case WM_SIZE:
		{
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);
			glViewport(0, 0, width, height);
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
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);

	hwnd = CreateWindow(wc.lpszClassName, "cnes", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, rect.right - rect.left, rect.bottom - rect.top, 0, 0, hInstance, 0);
}
