#include <windows.h>
#include <locale.h>

#include <libgrf/libgrf.h>
#include <curl/curl.h>
#include <mxml/mxml.h>

#include <7z/7zCrc.h>
#include <7z/7zFile.h>
#include <7z/7zVersion.h>
#include <7z/7zAlloc.h>
#include <7z/7zExtract.h>
#include <7z/7zIn.h>

#include "tehpatcher.h"
#include "network.h"
#include "options.h"
#include "dialog.h"

DWORD thread_id;

/*
 <?xml version="1.0"?>
 <patchlist>
	<patch number="1" grf="lumi.grf" action="append">00101_lumi_test.7z</patch>
	<patch number="2" action="append">00102_lumi_test2.7z</patch>
 </patchlist>
*/

BOOL
main_target_grf(void *grf_handle, WCHAR *name, BOOL is_dir, BYTE *buffer, size_t size)
{
	CHAR mb_name[512];
	WCHAR *ptr = name;

	if (grf_handle == NULL)
		return RES_FAIL;

	while (*ptr)
	{
		if (*ptr == L'/')
			*ptr = L'\\';
		ptr++;
	}

	WideCharToMultiByte(CP_UTF8, 0, name, -1, mb_name, sizeof(mb_name), NULL, NULL);

	if (grf_file_add(grf_handle, utf8_to_euc_kr(mb_name), buffer, size) == NULL)
	{
		WCHAR mes[256];

		swprintf_s(mes, sizeof(mes), L"grf_file_add() выполнена с ошибкой на файле: %s", name);
		dialog_set_status_w(mes);
		return RES_FAIL;
	}

	return RES_OK;
}

BOOL
main_fs_make_directory(WCHAR *name)
{
	WCHAR mes[256];

	if (CreateDirectoryW(name, NULL) == 0 &&
		GetLastError() != ERROR_ALREADY_EXISTS)
	{
		swprintf_s(mes, sizeof(mes), L"Невозможно создать: %s", name);
		dialog_set_status_w(mes);
		return RES_FAIL;
	}

	return RES_OK;
}

BOOL
main_target_fs(WCHAR *name, BOOL is_dir, BYTE *buffer, size_t size)
{
	HANDLE f;
	WCHAR mes[256];
	WCHAR *ptr = name;
	DWORD processed = 0;

	// making directories for each level
    for (; *ptr != 0; *ptr++)
	{
		if (*ptr == L'/')
		{
			*ptr = 0;
			if (main_fs_make_directory(name))
				return RES_FAIL;
			*ptr = L'/';
		}
	}

	// is that's just a dir
	if (is_dir)
	{
		if (main_fs_make_directory(name))
			return RES_FAIL;
		return RES_OK;
	}

	// open file for writing
	f = CreateFileW(name, GENERIC_WRITE, FILE_SHARE_READ, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == NULL)
	{
		swprintf_s(mes, sizeof(mes), L"Невозможно открыть файл: %s", name);
		dialog_set_status_w(mes);
		return RES_FAIL;
	}

	// write
	if (WriteFile(f, buffer, (DWORD)size, &processed, NULL) == FALSE)
	{
		swprintf_s(mes, sizeof(mes), L"Невозможно записать в файл: %s", name);
		dialog_set_status_w(mes);
		return RES_FAIL;
	}

	// close file
	CloseHandle(f);
	return RES_OK;
}

typedef struct
{
	ISeekInStream s;
	void *buf;
	void *ptr;
	size_t size;
} CMemInStream;

SRes
lzma_memory_read(void *p, void *buf, size_t *size)
{
	size_t originalSize = *size;
	CMemInStream *s = (CMemInStream *)p;

	if (originalSize == 0)
		return SZ_OK;

	*size = 0;
	memcpy(buf, s->ptr, originalSize);
	buf = (void *)((BYTE *)buf + originalSize);
	s->ptr = (void *)((BYTE *)s->ptr + originalSize);
	*size += originalSize;
	return SZ_OK;
}

SRes
lzma_memory_seek(void *p, Int64 *pos, ESzSeek origin)
{
	long rpos = (long)*pos;
	CMemInStream *s = (CMemInStream *)p;

	switch (origin)
	{
		case SZ_SEEK_SET:
			s->ptr = (BYTE *)s->buf + rpos;
			break;

		case SZ_SEEK_CUR:
			s->ptr = (BYTE *)s->ptr + rpos;
			break;

		case SZ_SEEK_END:
			s->ptr = (BYTE *)s->buf + s->size - rpos;
			break;

		default:
			return ERROR_INVALID_PARAMETER;
	}

	*pos = (BYTE *)s->ptr - (BYTE *)s->buf;
	return SZ_OK;
}

BOOL
main_unpack(CHAR *patch_file, struct MemoryStruct *patch, CHAR *grf)
{
	CHAR mes[256];

	CMemInStream memory_stream;
	CLookToRead lookStream;
	CSzArEx db;
	SRes res;
	ISzAlloc allocImp;
	ISzAlloc allocTempImp;

	memory_stream.buf = patch->memory;
	memory_stream.ptr = memory_stream.buf;
	memory_stream.size = patch->size;

	memory_stream.s.Read = &lzma_memory_read;
	memory_stream.s.Seek = &lzma_memory_seek;

	sprintf_s(mes, sizeof(mes), "Распаковка: %s...", patch_file);
	dialog_set_status(mes);

	LookToRead_CreateVTable(&lookStream, False);
	lookStream.realStream = &memory_stream.s;
	LookToRead_Init(&lookStream);

	allocImp.Alloc = SzAlloc;
	allocImp.Free = SzFree;

	allocTempImp.Alloc = SzAllocTemp;
	allocTempImp.Free = SzFreeTemp;

	SzArEx_Init(&db);
	res = SzArEx_Open(&db, &lookStream.s, &allocImp, &allocTempImp);
	if (res == SZ_OK)
	{
		UInt32 i;
		UInt32 blockIndex = 0xFFFFFFFF;
		Byte *outBuffer = 0;
		size_t outBufferSize = 0;
		void *grf_handle;

		// if we are working with grf we should
		// open grf-file first
		if (grf != NULL)
		{
			grf_handle = grf_load(grf, TRUE);
			if (grf_handle == NULL)
				grf_handle = grf_new(grf, TRUE);
			if (grf_handle == NULL)
			{
				SzArEx_Free(&db, &allocImp);
				sprintf_s(mes, sizeof(mes), "Невозможно ни открыть, ни создать GRF-файл: %s", grf);
				dialog_set_status(mes);
				return RES_FAIL;
			}
		}

		for (i = 0; i < db.db.NumFiles; i++)
		{
			BOOL res;
			size_t offset;
			size_t outSizeProcessed;
			CSzFileItem *f = db.db.Files + i;

			if (!f->IsDir)
			{
				res = SzAr_Extract(&db, &lookStream.s, i,
					&blockIndex, &outBuffer, &outBufferSize,
					&offset, &outSizeProcessed,
					&allocImp, &allocTempImp);

				if (res != SZ_OK)
					break;
			}

			res = (grf != NULL)
				? main_target_grf(grf_handle, (WCHAR *)f->Name, f->IsDir, outBuffer + offset, outSizeProcessed)
				: main_target_fs((WCHAR *)f->Name, f->IsDir, outBuffer + offset, outSizeProcessed);

			if (res)
			{
				IAlloc_Free(&allocImp, outBuffer);
				SzArEx_Free(&db, &allocImp);
				return RES_FAIL;
			}
		}

		// if we're working with grf
		// close it
		if (grf != NULL)
			grf_free(grf_handle);

		IAlloc_Free(&allocImp, outBuffer);
	}

	SzArEx_Free(&db, &allocImp);

	if (res != SZ_OK)
	{
		if (res == SZ_ERROR_UNSUPPORTED)
			sprintf_s(mes, sizeof(mes), "Ошибка распаковки %s: Архив не поддерживается", patch_file);
		else if (res == SZ_ERROR_MEM)
			sprintf_s(mes, sizeof(mes),	"Ошибка распаковки %s: Невозможно зарезервировать память", patch_file);
		else if (res == SZ_ERROR_CRC)
			sprintf_s(mes, sizeof(mes),	"Ошибка распаковки %s: ошибка CRC", patch_file);
		else
			sprintf_s(mes, sizeof(mes),	"Ошибка распаковки %s: ошибка #%d", patch_file, res);
		dialog_set_status(mes);
		return RES_FAIL;
	}

	return RES_OK;
}

BOOL
main_perform_patch(CHAR *patch_file, CHAR *action, CHAR *grf, CHAR *target)
{
	CHAR url[256];
	struct MemoryStruct patch;

	if (patch_file == NULL)
		return RES_FAIL;

	sprintf_s(url, sizeof(url),	"%s%s", options.base_url, patch_file);
	if (network_perform(url, patch_file, &patch, 0))
	{
		if (patch.memory != NULL)
			free(patch.memory);
		return RES_FAIL;
	}

	if (main_unpack(patch_file, &patch, grf))
	{
		free(patch.memory);
		return RES_FAIL;
	}

	free(patch.memory);

	return RES_OK;
}

BOOL
main_parse_patches_list(CHAR *list)
{
	mxml_node_t *tree, *node;

	if (list == NULL)
		return RES_FAIL;

	tree = mxmlLoadString(NULL, list, MXML_TEXT_CALLBACK);
	if (tree == NULL)
	{
		dialog_set_status("main_parse_patches_list: mxmlLoadString() выполнена с ошибкой");
		return RES_FAIL;
	}

    for (node = mxmlFindElement(tree, tree, "patch", NULL, NULL, MXML_DESCEND);
         node != NULL;
         node = mxmlFindElement(node, tree, "patch", NULL, NULL, MXML_DESCEND))
    {
		CHAR *number, *grf, *action, *patch_file, *target;

		number = (CHAR *)mxmlElementGetAttr(node, "number");
		grf = (CHAR *)mxmlElementGetAttr(node, "grf");
		action = (CHAR *)mxmlElementGetAttr(node, "action");
		target = (CHAR *)mxmlElementGetAttr(node, "target");

		if (number &&
			node->child &&
			node->child->type == MXML_TEXT)
		{
			if (atoi(number) <= options.last_patch)
				continue;

			patch_file = node->child->value.text.string;

			if (main_perform_patch(patch_file, action, grf, target))
				return RES_FAIL;

			options.last_patch = atoi(number);
			options_save_last_patch();
		}
    }

	return RES_OK;
}

BOOL
main_parse_motd(CHAR *motd)
{
	mxml_node_t *tree, *node;
	INT day, month, year;
	CHAR *text = NULL;

	if (motd == NULL)
		return RES_FAIL;

	tree = mxmlLoadString(NULL, motd, MXML_OPAQUE_CALLBACK);
	if (tree == NULL)
	{
		dialog_set_status("main_parse_motd: mxmlLoadString() выполнена с ошибкой");
		return RES_FAIL;
	}

    for (node = mxmlFindElement(tree, tree, "entry", NULL, NULL, MXML_DESCEND);
         node != NULL;
         node = mxmlFindElement(node, tree, "entry", NULL, NULL, MXML_DESCEND))
    {
		day = atoi((CHAR *)mxmlElementGetAttr(node, "day"));
		month = atoi((CHAR *)mxmlElementGetAttr(node, "month"));
		year = atoi((CHAR *)mxmlElementGetAttr(node, "year"));
		text = node->child->value.opaque;
    }

	if (text != NULL)
		dialog_set_motd(text, day, month, year);

	return RES_OK;
}

DWORD WINAPI
main_routine(LPVOID lpParam)
{
	CHAR url[256];
	struct MemoryStruct list, motd;

	// load motd
	sprintf_s(url, sizeof(url),	"%s%s", options.base_url, options.motd_file);
	if (!network_perform(url, options.motd_file, &motd, 0))
	{
		((CHAR *)motd.memory)[motd.size] = '\0';

		// parsing motd
		main_parse_motd((CHAR *)motd.memory);  // ignoring errors
		free(motd.memory);
	}

	// fetching patches list
	sprintf_s(url, sizeof(url),	"%s%s", options.base_url, options.patch_list);
	if (network_perform(url, options.patch_list, &list, 0))
	{
		free(list.memory);
		dialog_enable_start();
		return RES_FAIL;
	}

	((CHAR *)list.memory)[list.size] = '\0';

	// parsing the list
	if (main_parse_patches_list((CHAR *)list.memory))
	{
		free(list.memory);
		dialog_enable_start();
		return RES_FAIL;
	}
	
	dialog_set_status("Процесс завершен. Нажмите кнопку Старт, чтобы запустить игру.");

	free(list.memory);
	dialog_enable_start();
	return RES_OK;
}

void
main_start_routine()
{
	CreateThread(NULL, 0, main_routine, NULL, 0, &thread_id);
}

int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hInstNULL, LPSTR lpszCmdLine, int nCmdShow)
{
	setlocale(LC_ALL, "Russian");

	CrcGenerateTable();

	options_load();
	dialog_main(hInstance, NULL);
	return 0;
}
