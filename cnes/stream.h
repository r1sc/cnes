#ifndef _STREAM_H_
#define _STREAM_H_

typedef void(*stream_writer)(const void* data, size_t element_size, size_t element_count, void* stream);
typedef void(*stream_reader)(void* dest, size_t element_size, size_t element_count, void* stream);

#endif