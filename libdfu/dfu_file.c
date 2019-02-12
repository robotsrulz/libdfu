/*
 * Load or store DFU files including suffix and prefix
 *
 * Copyright 2014 Tormod Volden <debian.tormod@gmail.com>
 * Copyright 2012 Stefan Schmidt <stefan@datenfreihafen.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>

#include "dfu_file.h"

#define DFU_SUFFIX_LENGTH 16
#define LMDFU_PREFIX_LENGTH 8
#define LPCDFU_PREFIX_LENGTH 16
#define PROGRESS_BAR_WIDTH 25
#define STDIN_CHUNK_SIZE 65536

void dfu_progress_bar(const char *desc, unsigned long long curr,
		unsigned long long max)
{
#if 0
	static char buf[PROGRESS_BAR_WIDTH + 1];
	static unsigned long long last_progress = -1;
	static time_t last_time;
	time_t curr_time = time(NULL);
	unsigned long long progress;
	unsigned long long x;

	/* check for not known maximum */
	if (max < curr)
		max = curr + 1;
	/* make none out of none give zero */
	if (max == 0 && curr == 0)
		max = 1;

	/* compute completion */
	progress = (PROGRESS_BAR_WIDTH * curr) / max;
	if (progress > PROGRESS_BAR_WIDTH)
		progress = PROGRESS_BAR_WIDTH;
	if (progress == last_progress &&
	    curr_time == last_time)
		return;
	last_progress = progress;
	last_time = curr_time;

	for (x = 0; x != PROGRESS_BAR_WIDTH; x++) {
		if (x < progress)
			buf[x] = '=';
		else
			buf[x] = ' ';
	}
	buf[x] = 0;

	printf("\r%s\t[%s] %3lld%% %12lld bytes", desc, buf,
	    (100ULL * curr) / max, curr);

	if (progress == PROGRESS_BAR_WIDTH)
		printf("\n%s done.\n", desc);
#endif
}

void *dfu_malloc(size_t size)
{
	void *ptr = malloc(size);
//	if (ptr == NULL)
//		errx(EX_SOFTWARE, "Cannot allocate memory of size %d bytes", (int)size);
	return (ptr);
}

#if 0
BOOL dfu_load_file(CString filename, struct dfu_file *file)
{
	BOOL retVal = FALSE;

	file->size = 0;
	if (file->firmware)
	{
		free(file->firmware);
		file->firmware = NULL;
	}

	HANDLE firmwareFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ,
		NULL, //&SecurityAttributes,  //no SECURITY_ATTRIBUTES structure
		OPEN_EXISTING,  //No special create flags
		0, // No special attributes
		NULL); // No template file

	if (firmwareFile != INVALID_HANDLE_VALUE)
	{
		off_t fsize = GetFileSize(firmwareFile, NULL);
		DWORD NumberOfBytesRead;

		if (INVALID_FILE_SIZE != fsize) file->size = fsize;
		file->firmware = (uint8_t *)dfu_malloc(file->size);

		if (ReadFile(firmwareFile, file->firmware, file->size,
			&NumberOfBytesRead, NULL))
		{
			retVal = TRUE;
		}
		CloseHandle(firmwareFile);
	}

	return retVal;
}
#endif
