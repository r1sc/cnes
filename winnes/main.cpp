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
#include "joystick.h"

#include <crtdbg.h>
#include <memory>

bool running = true;
static HGLRC ourOpenGLRenderingContext;

bool joystick_enabled = false;

static std::unique_ptr<Window> window = nullptr;

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
		MessageBox(window->hwnd, infoLog, "Shader compile error", MB_ICONERROR);
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
		MessageBox(window->hwnd, infoLog, "Shader program link error", MB_ICONERROR);
		exit(1);
	}

	return program;
}

GLuint load_shader_program_from_disk(const char* path) {
	FILE* f;
	if (fopen_s(&f, path, "rb")) {
		MessageBox(window->hwnd, "Failed to load shader", "Error", 0);
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
	sample_out += ((sample - sample_out) >> 4);

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
static HANDLE ines_loading_mutex;

DWORD WINAPI render_thread(void* param) {
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
		PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
		24,                   // Colordepth of the framebuffer.
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		0,                   // Number of bits for the depthbuffer
		0,                    // Number of bits for the stencilbuffer
		0,                    // Number of Aux buffers in the framebuffer.
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	window_dc = GetDC(window->hwnd);

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

	timeBeginPeriod(1);

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER last;
	LARGE_INTEGER now;
	LONGLONG accum = 0;
	LONGLONG dt_cps = frequency.QuadPart / 60;
	LONGLONG one_second_cps = frequency.QuadPart;
	LONGLONG one_ms_cps = frequency.QuadPart / 1000;
	LONGLONG secondacc = 0;

	int frame_counter = 0;
	int num_frames = 0;

	waveout_initialize(SAMPLE_RATE, BUFFER_LEN);

	QueryPerformanceCounter(&last);
	QueryPerformanceCounter(&now);

	while (running) {
		QueryPerformanceCounter(&now);
		LONGLONG delta = now.QuadPart - last.QuadPart;
		last = now;

		WaitForSingleObject(ines_loading_mutex, INFINITE);

		if (delta >= one_second_cps) {
			delta = dt_cps;
		}
		accum += delta;
		secondacc += delta;

		if (secondacc >= one_second_cps) {
			int fps = frame_counter;
			char title[128];
			sprintf_s(title, 128, "WinNES - (%d screen fps and %d NES frames / sec)\0", fps, num_frames);
			SetWindowText(window->hwnd, title);
			frame_counter = 0;
			num_frames = 0;
			secondacc -= one_second_cps;
		}

		if (accum >= dt_cps) {
			poll_xinput_joy(0);
			poll_xinput_joy(1);

			while (accum >= dt_cps) {
				tick_frame();
				num_frames++;
				accum -= dt_cps;
			}

			frame_counter++;
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGB, GL_UNSIGNED_BYTE, framebuffer);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
			SwapBuffers(window_dc);

			if (needs_resize) {
				glViewport(new_width / 2 - new_size / 2, new_height / 2 - new_size / 2, new_size, new_size);
				glClear(GL_COLOR_BUFFER_BIT);
				SwapBuffers(window_dc);
				glClear(GL_COLOR_BUFFER_BIT);
				SwapBuffers(window_dc);

				needs_resize = false;
			}
		}
		ReleaseMutex(ines_loading_mutex);

		Sleep(1);

	}

	timeEndPeriod(1);

	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(ourOpenGLRenderingContext);

	waveout_free();

	return 0;
}

char* loaded_data = NULL;
char loaded_path[256] = { 0 };

void main_load_state() {
	if (loaded_data == NULL) return;

	char state_path[256];
	sprintf_s(state_path, sizeof(state_path), "%s.sav", loaded_path);

	FILE* f;
	fopen_s(&f, state_path, "rb");
	if (f) {
		load_state((void*)f, (stream_reader)fread);
		fclose(f);
	}
}

void main_save_state() {
	if (loaded_data == NULL) return;

	char state_path[256];
	sprintf_s(state_path, sizeof(state_path), "%s.sav", loaded_path);

	FILE* f;
	fopen_s(&f, state_path, "wb");
	if (f) {
		save_state((void*)f, (stream_writer)fwrite);
		fclose(f);
	}
}

void free_ines_file() {
	if (loaded_data) {
		free(loaded_data);
		loaded_data = NULL;
	}
}

void load_ines_from_file(const char* path) {
	WaitForSingleObject(ines_loading_mutex, INFINITE);

	strcpy_s(loaded_path, sizeof(loaded_path), path);

	free_ines_file();

	FILE* f;
	fopen_s(&f, path, "rb");
	if (!f) {
		MessageBox(NULL, "Failed to read nes file", "Error", MB_ICONERROR);
		return;
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	loaded_data = (char*)malloc((size_t)size);
	if (!loaded_data) exit(1);

	fread(loaded_data, size, 1, f);

	fclose(f);

	int result = load_ines(loaded_data);
	switch (result) {
		case CNES_LOAD_NO_ERR:
			break;
		case CNES_LOAD_MAPPER_NOT_SUPPORTED:
			MessageBox(NULL, "Mapper not supported!", "Error", MB_ICONERROR);
			[[fallthrough]];
		default:
			exit(1);
			break;
	}

	ReleaseMutex(ines_loading_mutex);
}

static std::vector<uint8_t> chr_ram;

extern "C" uint8_t* get_8k_chr_ram(uint8_t num_8k_chunks) {
	chr_ram.resize(8192 * (size_t)num_8k_chunks);
	return chr_ram.data();
}

int APIENTRY WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nShowCmd
) {
	ines_loading_mutex = CreateMutex(NULL, FALSE, NULL);
	load_ines_from_file("roms/smb3.nes");
	
	//create_window();
	window = std::make_unique<Window>(reset_machine, main_load_state, main_save_state);

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

	free_ines_file();

	return 0;
}