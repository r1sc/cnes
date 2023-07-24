#include <Windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cnes.h>

#include "window.h"
#include "waveout.h"
#include "glstuff/glad.h"
#include "glstuff/wglext.h"
#include <crtdbg.h>

bool running = true;
static HGLRC ourOpenGLRenderingContext;
extern void poll_joystick(uint8_t joystick_id);

GLuint load_shader(const char* shader_src, GLenum kind) {

	const GLchar* strings[] = {
		"#version 300 es\n\0",
		kind == GL_VERTEX_SHADER ? "#define VS\n\0" : "#define FS\n\0",
		shader_src
	};

	GLuint shader = glCreateShader(kind);
	glShaderSource(shader, 3, strings, NULL);

	glCompileShader(shader);

	GLint compile_status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (!compile_status) {
		GLchar infoLog[256];
		GLsizei actualLength;
		glGetShaderInfoLog(shader, 256, &actualLength, infoLog);
		MessageBox(hwnd, infoLog, "Shader compile error", MB_ICONERROR);
		exit(1);
	}

	return shader;
}

GLuint link_program(const char* src) {
	GLuint program = glCreateProgram();
	glAttachShader(program, load_shader(src, GL_VERTEX_SHADER));
	glAttachShader(program, load_shader(src, GL_FRAGMENT_SHADER));

	glLinkProgram(program);

	GLint link_status;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (!link_status) {
		GLchar infoLog[256];
		GLsizei actualLength;
		glGetProgramInfoLog(program, 256, &actualLength, infoLog);
		MessageBox(hwnd, infoLog, "Shader program link error", MB_ICONERROR);
		exit(1);
	}

	return program;
}

GLuint load_shader_program_from_disk(const char* path) {
	FILE* f;
	if (fopen_s(&f, path, "rb")) {
		MessageBox(hwnd, "Failed to load shader", "Error", 0);
		exit(1);
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* src = (char*)malloc(size + 1);
	ZeroMemory(src, size + 1);
	fread(src, size, 1, f);
	fclose(f);

	GLuint program = link_program(src);
	free(src);

	return program;
}

// AUDIO STUFF
//
#define SAMPLE_RATE 15720
#define BUFFER_LEN 262
static int16_t* buffer = NULL;
static size_t buffer_pos = 0;
static int16_t sample_out = 0;
static int last_scanline = -2; // Start out of range

void write_audio_sample(int scanline, int16_t sample) {
	// Super crude 1 pole IIR filter
	sample_out += ((sample - sample_out) >> 6);	

	if (scanline == last_scanline) return;
	last_scanline = scanline;

	if (buffer == NULL) {
		buffer = waveout_get_current_buffer();
		buffer_pos = 0;
	}
	if (buffer != NULL) {
		buffer[buffer_pos++] = sample_out;

		if (buffer_pos == BUFFER_LEN) {
			waveout_queue_buffer();
			buffer = NULL;
		}
	}
}


static HDC window_dc;

DWORD WINAPI render_thread(void* param) {
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

	HGLRC fakeContext = wglCreateContext(window_dc);
	wglMakeCurrent(window_dc, fakeContext);

	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

	wglSwapIntervalEXT(0);
	int attribs[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
		WGL_CONTEXT_MINOR_VERSION_ARB, 2,
		WGL_CONTEXT_FLAGS_ARB, 0,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0
	};
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
	ourOpenGLRenderingContext = wglCreateContextAttribsARB(window_dc, NULL, attribs);

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(fakeContext);
	wglMakeCurrent(window_dc, ourOpenGLRenderingContext);
	wglSwapIntervalEXT(0);

	gladLoadGL();

	GLuint program = load_shader_program_from_disk("shaders/crt.glsl");
	glUseProgram(program);

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 240, 0, GL_RGB, GL_UNSIGNED_BYTE, framebuffer);

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// x y u v
	int vertex_data[] = {
		-1, 1, 0, 0,
		1, 1, 1, 0,
		1, -1, 1, 1,
		-1, -1, 0, 1
	};

	GLuint vertex_buffer;
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);

	unsigned short indices[] = {
		0, 1, 2,
		0, 2, 3
	};

	GLuint index_buffer;
	glGenBuffers(1, &index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_INT, false, 4 * 4, (GLvoid*)0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_INT, false, 4 * 4, (GLvoid*)8);

	double last = (double)GetTickCount64();
	double  now = last;
	double  accum = 0;
	double  dt = 1000.0 / 60.0;

	int frame_counter = 0;
	int fps = 0;
	double secondacc = 0;
	int num_frames = 0;

	waveout_initialize(SAMPLE_RATE, BUFFER_LEN);

	while (running) {
		now = (double)GetTickCount64();
		double delta = now - last;
		if (delta > 1000) {
			delta = dt;
		}
		last = now;
		accum += delta;
		secondacc += delta;

		if (secondacc >= 1000) {
			fps = frame_counter;
			char title[128];
			sprintf(title, "WinNES - (%d fps = %d frames / sec)\0", fps, num_frames);
			SetWindowText(hwnd, title);
			frame_counter = 0;
			num_frames = 0;
			secondacc -= 1000;
		}

		bool needs_rerender = false;
		if (accum >= dt) {
			//poll_joystick(0);
			needs_rerender = true;


			while (accum >= dt) {
				tick_frame();
				num_frames++;
				accum -= dt;
			}
		} else {
			Sleep(1);
		}

		if (needs_rerender) {
			frame_counter++;
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGB, GL_UNSIGNED_BYTE, framebuffer);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
			SwapBuffers(window_dc);
			glFinish();


			if (needs_resize) {
				glViewport(new_width / 2 - new_size / 2, new_height / 2 - new_size / 2, new_size, new_size);
				glClear(GL_COLOR_BUFFER_BIT);
				SwapBuffers(window_dc);
				glClear(GL_COLOR_BUFFER_BIT);
				SwapBuffers(window_dc);

				needs_resize = false;
			}
		}
	}

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(ourOpenGLRenderingContext);

	waveout_free();

	return 0;
}

int APIENTRY WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nShowCmd
) {
	load_ines("roms/bubble.nes");
	create_window();

	HANDLE threadId = CreateThread(NULL, 0, render_thread, NULL, 0, NULL);
	if (!threadId) {
		MessageBox(NULL, "Failed to create render thread", "Error", 0);
		return 1;
	}

	MSG msg;
	while (running) {
		GetMessage(&msg, NULL, 0, 0);

		if (msg.message == WM_QUIT) {
			running = false;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);

	}

	WaitForSingleObject(threadId, INFINITE);

	free_ines();

	return 0;
}