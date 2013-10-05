#include <windows.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#include "network.h"
#include "dialog.h"
#include "tehpatcher.h"
#include "options.h"

void *
network_realloc(void *ptr, size_t size)
{
	return (ptr)
		? realloc(ptr, size)
		: malloc(size);
}

size_t
network_write_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)data;

	mem->memory = network_realloc(mem->memory, mem->size + realsize + 1);
	if (mem->memory)
	{
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;
	}

	return realsize;
}

int
network_progress_callback(void *target,
	double t, /* dltotal */ double d, /* dlnow */
	double ultotal, double ulnow)
{
	CHAR mes[128];
	struct MemoryStruct *obj = (struct MemoryStruct *)target;

	if (t > 0)
	{
		t += obj->offset;
		d += obj->offset;

		sprintf_s(mes, sizeof(mes), "Загрузка: %s (%.2f Mb, %d%%)...",
			obj->filename, t/1024./1024., (DWORD)(d * 100.0 / t));
		dialog_set_status(mes);
	}
	else
	{
		sprintf_s(mes, sizeof(mes), "Загрузка: %s...", obj->filename);
		dialog_set_status(mes);
	}

	dialog_set_progress((DWORD)(d * 100.0 / t));
	return 0;
}

BOOL
network_perform(CHAR *url, CHAR *filename,
	struct MemoryStruct *target, long resume_from)
{
	int s;
	CHAR mes[128];
	CURL *curl_handle;
	CURLcode res;

	sprintf_s(mes, sizeof(mes), "Запрос: %s...", filename);
	dialog_set_status(mes);

	curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);

	// enable proxy
	if (options.proxy_enabled)
	{
		curl_easy_setopt(curl_handle, CURLOPT_PROXY, options.proxy_address);
		curl_easy_setopt(curl_handle, CURLOPT_PROXYPORT, (LONG)options.proxy_port);
		curl_easy_setopt(curl_handle, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);

		if (options.proxy_auth_required)
		{
			curl_easy_setopt(curl_handle, CURLOPT_PROXYUSERNAME, options.proxy_auth_user);
			curl_easy_setopt(curl_handle, CURLOPT_PROXYPASSWORD, options.proxy_auth_password);
		}
	}

	target->filename = filename;

	// perform new download
	if (resume_from == 0)
	{
		target->memory = NULL;
		target->size = 0;
		target->offset = 0;
	}

	// resume after disconnect
	else
	{
		curl_easy_setopt(curl_handle, CURLOPT_RESUME_FROM, resume_from);
		target->offset = resume_from;
	}

	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, network_write_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)target);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, network_progress_callback);
	curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, (void *)target);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	res = curl_easy_perform(curl_handle);
	curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &target->http_response_code);
	curl_easy_cleanup(curl_handle);

	if (res == CURLE_OK && is_http_ok(target->http_response_code))
		return RES_OK;

	if (res == CURLE_PARTIAL_FILE)
	{
		// delay 10 seconds
		for (s=10; s>0; s--)
		{
			sprintf_s(mes, sizeof(mes),	"Не удается получить %s: %s [%d]",
				filename, curl_easy_strerror(res), s);
			dialog_set_status(mes);
			Sleep(1000);
		}

		// start downloading again
		return network_perform(url, filename,
			target, (long)target->size);
	}

	if (res != CURLE_OK)
		sprintf_s(mes, sizeof(mes),	"Не удается получить %s: %s",
			filename, curl_easy_strerror(res));
	else
		sprintf_s(mes, sizeof(mes),	"Не удается получить %s: HTTP/%d",
			filename, target->http_response_code);
	dialog_set_status(mes);
	return RES_FAIL;
}
