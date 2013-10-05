#pragma once
#include <curl/curl.h>

struct MemoryStruct {
	char *memory;
	size_t size;
	long http_response_code;
	long offset;
	char *filename;
};

CURLcode network_perform(CHAR *url, CHAR *filename,
	struct MemoryStruct *target, long start_from);

#define is_http_ok(__c) (__c >= 200 && __c <= 206)
