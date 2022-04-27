//
// Copyright(C) 2020 Alex Mayfield
// Copyright(C) 2022 Fabian Greffrath
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// Converts text files to a format usable by Windows Notepad.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "win_fopen.h"

#define FAIL(a) do{fputs(a,stderr);\
	if(src)fclose(src);\
	if(dest)fclose(dest);\
	if(src_stream)free(src_stream);\
	if(dest_stream)free(dest_stream);\
	return EXIT_FAILURE;}while(0)

static const char *const bom = "\xef\xbb\xbf";

int main (int argc, char **argv)
{
#ifdef _WIN32
	FILE *src = NULL, *dest = NULL;
	char *src_stream = NULL, *dest_stream = NULL;
	size_t i, j, src_size, dest_size;

	if (argc < 3)
	{
		FAIL("Usage: win-textfile <src> <dest>");
	}

	src = fopen(argv[1], "r");
	if (!src)
	{
		FAIL("Failed to open source file");
	}

	fseek(src, 0, SEEK_END);
	src_size = ftell(src);
	fseek(src, 0, SEEK_SET);

	src_stream = malloc(sizeof(*dest_stream) * src_size);
	if (!src_stream)
	{
		FAIL("Failed to allocate memory for source buffer");
	}

	if (fread(src_stream, 1, src_size, src) != src_size)
	{
		FAIL("Failed to read from source file");
	}

	fclose(src);

	// allocate 3 bytes more for BOM
	dest_size = src_size + 3;
	for (i = 0; i < src_size; i++)
	{
		if (src_stream[i] == '\n')
			dest_size++;
	}

	dest_stream = malloc(sizeof(*dest_stream) * dest_size);
	if (!dest_stream)
	{
		FAIL("Failed to allocate memory for destination buffer");
	}

	j = 0;

	// check for BOM and if it doesn't exist, add it
	if (memcmp(src_stream, bom, 3) != 0)
	{
		memcpy(dest_stream, bom, 3);
		j = 3;
	}

	for (i = 0; i < src_size; i++)
	{
		if (src_stream[i] == '\r')
		{
			continue;
		}
		else if (src_stream[i] == '\n')
		{
			dest_stream[j++] = '\r';
			dest_stream[j++] = '\n';
		}
		else
		{
			dest_stream[j++] = src_stream[i];
		}
	}

	free(src_stream);

	dest = fopen(argv[2], "w");
	if (!dest)
	{
		FAIL("Failed to open destination file");
	}

	if (fwrite(dest_stream, 1, j, dest) != j)
	{
		FAIL("Failed to write to destination file");
	}

	free(dest_stream);
	fclose(dest);
#endif
	return EXIT_SUCCESS;
}
