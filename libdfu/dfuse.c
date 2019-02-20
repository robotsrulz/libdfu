/*
 * DfuSe specific functions
 * 
 * This implements the ST Microsystems DFU extensions (DfuSe)
 * as per the DfuSe 1.1a specification (ST documents AN3156, AN2606)
 * The DfuSe file format is described in ST document UM0391.
 *
 * Copyright 2010-2016 Tormod Volden <debian.tormod@gmail.com>
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

// #define _OFF_T_DEFINED


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "dfu.h"
#include "usb_dfu.h"
#include "dfu_file.h"
#include "dfuse.h"
#include "dfuse_mem.h"

#define DFU_TIMEOUT 5000

unsigned int last_erased_page = 1; /* non-aligned value, won't match */
struct memsegment *mem_layout;
unsigned int dfuse_address = 0;
unsigned int dfuse_address_present = 0;
static unsigned int dfuse_length = 0;
static int dfuse_force = 0;
static int dfuse_leave = 0;
static int dfuse_unprotect = 0;
static int dfuse_mass_erase = 0;
static int dfuse_will_reset = 0;

const char *lastError = 0;

unsigned int quad2uint(unsigned char *p)
{
	return (*p + (*(p + 1) << 8) + (*(p + 2) << 16) + (*(p + 3) << 24));
}

void dfuse_parse_options(const char *options)
{
	char *end;
	const char *endword;
	unsigned int number;

	/* address, possibly empty, must be first */
	if (*options != ':') {
		endword = strchr(options, ':');
		if (!endword)
			endword = options + strlen(options); /* GNU strchrnul */

		number = strtoul(options, &end, 0);
		if (end == endword) {
			dfuse_address = number;
			dfuse_address_present = 1;
		} else {
//			errx(EX_IOERR, "Invalid dfuse address: %s", options);
		}
		options = endword;
	}

	while (*options) {
		if (*options == ':') {
			options++;
			continue;
		}
		endword = strchr(options, ':');
		if (!endword)
			endword = options + strlen(options);

		if (!strncmp(options, "force", endword - options)) {
			dfuse_force++;
			options += 5;
			continue;
		}
		if (!strncmp(options, "leave", endword - options)) {
			dfuse_leave = 1;
			options += 5;
			continue;
		}
		if (!strncmp(options, "unprotect", endword - options)) {
			dfuse_unprotect = 1;
			options += 9;
			continue;
		}
		if (!strncmp(options, "mass-erase", endword - options)) {
			dfuse_mass_erase = 1;
			options += 10;
			continue;
		}
		if (!strncmp(options, "will-reset", endword - options)) {
			dfuse_will_reset = 1;
			options += 10;
			continue;
		}

		/* any valid number is interpreted as upload length */
		number = strtoul(options, &end, 0);
		if (end == endword) {
			dfuse_length = number;
		} else {
//			errx(EX_IOERR, "Invalid dfuse modifier: %s", options);
		}
		options = endword;
	}
}

/* DFU_UPLOAD request for DfuSe 1.1a */
int dfuse_upload(struct dfu_if *dif, const unsigned short length,
		 unsigned char *data, unsigned short transaction)
{
	int status;

	status = libusb_control_transfer(dif->dev_handle,
		 /* bmRequestType */	 LIBUSB_ENDPOINT_IN |
					 LIBUSB_REQUEST_TYPE_CLASS |
					 LIBUSB_RECIPIENT_INTERFACE,
		 /* bRequest      */	 DFU_UPLOAD,
		 /* wValue        */	 transaction,
		 /* wIndex        */	 dif->interface,
		 /* Data          */	 data,
		 /* wLength       */	 length,
					 DFU_TIMEOUT);
	if (status < 0) {
//		errx(EX_IOERR, "%s: libusb_control_msg returned %d",
//			__FUNCTION__, status);
	}
	return status;
}

/* DFU_DNLOAD request for DfuSe 1.1a */
int dfuse_download(struct dfu_if *dif, const unsigned short length,
		   unsigned char *data, unsigned short transaction)
{
	int status;

	status = libusb_control_transfer(dif->dev_handle,
		 /* bmRequestType */	 LIBUSB_ENDPOINT_OUT |
					 LIBUSB_REQUEST_TYPE_CLASS |
					 LIBUSB_RECIPIENT_INTERFACE,
		 /* bRequest      */	 DFU_DNLOAD,
		 /* wValue        */	 transaction,
		 /* wIndex        */	 dif->interface,
		 /* Data          */	 data,
		 /* wLength       */	 length,
					 DFU_TIMEOUT);
	if (status < 0) {
//		errx(EX_IOERR, "%s: libusb_control_transfer returned %d",
//			__FUNCTION__, status);
	}
	return status;
}

/* DfuSe only commands */
/* Leaves the device in dfuDNLOAD-IDLE state */
int dfuse_special_command(struct dfu_if *dif, unsigned int address,
			  enum dfuse_command command)
{
	const char* dfuse_command_name[] = { "SET_ADDRESS" , "ERASE_PAGE",
					     "MASS_ERASE", "READ_UNPROTECT"};
	unsigned char buf[5];
	int length;
	int ret;
	struct dfu_status dst;
	int firstpoll = 1;

	if (command == ERASE_PAGE) {
		struct memsegment *segment;
		int page_size;

		segment = find_segment(mem_layout, address);
		if (!segment || !(segment->memtype & DFUSE_ERASABLE)) {
//			errx(EX_IOERR, "Page at 0x%08x can not be erased",
//				address);
			lastError = "Page can not be erased";
		}
		page_size = segment->pagesize;
		buf[0] = 0x41;	/* Erase command */
		length = 5;
		last_erased_page = address & ~(page_size - 1);
	} else if (command == SET_ADDRESS) {
		buf[0] = 0x21;	/* Set Address Pointer command */
		length = 5;
	} else if (command == MASS_ERASE) {
		buf[0] = 0x41;	/* Mass erase command when length = 1 */
		length = 1;
	} else if (command == READ_UNPROTECT) {
		buf[0] = 0x92;
		length = 1;
	} else {
//		errx(EX_IOERR, "Non-supported special command %d", command);
		lastError = "Non-supported special command";
	}
	buf[1] = address & 0xff;
	buf[2] = (address >> 8) & 0xff;
	buf[3] = (address >> 16) & 0xff;
	buf[4] = (address >> 24) & 0xff;

	ret = dfuse_download(dif, length, buf, 0);
	if (ret < 0) {
//		errx(EX_IOERR, "Error during special command \"%s\" download",
//			dfuse_command_name[command]);
		lastError = "Error during special command download";
		return ret;
	}
	do {
		ret = dfu_get_status(dif, &dst);
		if (ret < 0) {
//			errx(EX_IOERR, "Error during special command \"%s\" get_status",
//			     dfuse_command_name[command]);
			lastError = "Error during special command get_status";
			return ret;
		}
		if (firstpoll) {
			firstpoll = 0;
			if (dst.bState != DFU_STATE_dfuDNBUSY) {
				printf("state(%u) = %s, status(%u) = %s\n", dst.bState,
				       dfu_state_to_string(dst.bState), dst.bStatus,
				       dfu_status_to_string(dst.bStatus));
//				errx(EX_IOERR, "Wrong state after command \"%s\" download",
//				     dfuse_command_name[command]);
				lastError = "Wrong state after command download";
				return -1;
			}
			/* STM32F405 lies about mass erase timeout */
			if (command == MASS_ERASE && dst.bwPollTimeout == 100) {
				dst.bwPollTimeout = 35000;
				// printf("Setting timeout to 35 seconds\n");
			}
		}
		/* wait while command is executed */
		if (dst.bwPollTimeout) nanosleep(&(struct timespec){0, dst.bwPollTimeout * 1000}, NULL);
		if (command == READ_UNPROTECT)
			return ret;
	} while (dst.bState == DFU_STATE_dfuDNBUSY);

	if (dst.bStatus != DFU_STATUS_OK) {
//		errx(EX_IOERR, "%s not correctly executed",
//			dfuse_command_name[command]);
		lastError = "Command not correctly executed";
		return -1;
	}
	return ret;
}

int dfuse_dnload_chunk(struct dfu_if *dif, unsigned char *data, int size,
		       int transaction)
{
	int bytes_sent;
	struct dfu_status dst;
	int ret;

	ret = dfuse_download(dif, size, size ? data : NULL, transaction);
	if (ret < 0) {
//		errx(EX_IOERR, "Error during download");
		return ret;
	}
	bytes_sent = ret;

	do {
		ret = dfu_get_status(dif, &dst);
		if (ret < 0) {
//			errx(EX_IOERR, "Error during download get_status");
			return ret;
		}
		if (dst.bwPollTimeout) nanosleep(&(struct timespec){0, dst.bwPollTimeout * 1000}, NULL);
	} while (dst.bState != DFU_STATE_dfuDNLOAD_IDLE &&
		 dst.bState != DFU_STATE_dfuERROR &&
		 dst.bState != DFU_STATE_dfuMANIFEST &&
		 !(dfuse_will_reset && (dst.bState == DFU_STATE_dfuDNBUSY)));

//	if (dst.bState == DFU_STATE_dfuMANIFEST)
//			printf("Transitioning to dfuMANIFEST state\n");

	if (dst.bStatus != DFU_STATUS_OK) {
//		printf(" failed!\n");
//		printf("state(%u) = %s, status(%u) = %s\n", dst.bState,
//		       dfu_state_to_string(dst.bState), dst.bStatus,
//		       dfu_status_to_string(dst.bStatus));
		return -1;
	}
	return bytes_sent;
}


/* Writes an element of any size to the device, taking care of page erases */
/* returns 0 on success, otherwise -EINVAL */
int dfuse_dnload_element(struct dfu_if *dif, unsigned int dwElementAddress,
			 unsigned int dwElementSize, unsigned char *data,
			 int xfer_size)
{
	int p;
	int ret;
	struct memsegment *segment;

	/* Check at least that we can write to the last address */
	segment = find_segment(mem_layout, dwElementAddress + dwElementSize - 1);
//	if (!dfuse_force &&
//            (!segment || !(segment->memtype & DFUSE_WRITEABLE))) {
//		errx(EX_IOERR, "Last page at 0x%08x is not writeable",
//			dwElementAddress + dwElementSize - 1);
//	}

//	dfu_progress_bar("Download", 0, 1);

	/* First pass: Erase involved pages if needed */
	for (p = 0; p < (int)dwElementSize; p += xfer_size) {
		int page_size;
		unsigned int erase_address;
		unsigned int address = dwElementAddress + p;
		int chunk_size = xfer_size;

//		segment = find_segment(mem_layout, address);
//		if (!dfuse_force &&
//		    (!segment || !(segment->memtype & DFUSE_WRITEABLE))) {
//			errx(EX_IOERR, "Page at 0x%08x is not writeable",
//				address);
//		}
		/* If the location is not in the memory map we skip erasing */
		/* since we wouldn't know the correct page size for flash erase */
		if (!segment)
			continue;

		page_size = segment->pagesize;

		/* check if this is the last chunk */
		if (p + chunk_size > (int)dwElementSize)
			chunk_size = dwElementSize - p;

		/* Erase only for flash memory downloads */
		if ((segment->memtype & DFUSE_ERASABLE) && !dfuse_mass_erase) {
			/* erase all involved pages */
			for (erase_address = address;
			     erase_address < address + chunk_size;
			     erase_address += page_size)
				if ((erase_address & ~(page_size - 1)) !=
				    last_erased_page)
					dfuse_special_command(dif,
							      erase_address,
							      ERASE_PAGE);

			if (((address + chunk_size - 1) & ~(page_size - 1)) !=
			    last_erased_page) {
				dfuse_special_command(dif,
						      address + chunk_size - 1,
						      ERASE_PAGE);
			}
		}
	}

	/* Second pass: Write data to (erased) pages */
	for (p = 0; p < (int)dwElementSize; p += xfer_size) {
		unsigned int address = dwElementAddress + p;
		int chunk_size = xfer_size;

		/* check if this is the last chunk */
		if (p + chunk_size > (int)dwElementSize)
			chunk_size = dwElementSize - p;

//		dfu_progress_bar("Download", p, dwElementSize);		
		dfuse_special_command(dif, address, SET_ADDRESS);

		/* transaction = 2 for no address offset */
		ret = dfuse_dnload_chunk(dif, data + p, chunk_size, 2);
		if (ret != chunk_size) {
//			errx(EX_IOERR, "Failed to write whole chunk: "
//				"%i of %i bytes", ret, chunk_size);
			return -EINVAL;
		}
	}

//	dfu_progress_bar("Download", dwElementSize, dwElementSize);
	return 0;
}

static void
dfuse_memcpy(unsigned char *dst, unsigned char **src, int *rem, int size)
{
	if (size > *rem) {
//		errx(EX_IOERR, "Corrupt DfuSe file: "
//		    "Cannot read %d bytes from %d bytes", size, *rem);
	}
	if (dst != NULL)
		memcpy(dst, *src, size);
	(*src) += size;
	(*rem) -= size;
}

/* Download raw binary file to DfuSe device */
int dfuse_do_bin_dnload(struct dfu_if *dif, int xfer_size,
			struct dfu_file *file, unsigned int start_address)
{
	unsigned int dwElementAddress;
	unsigned int dwElementSize;
	unsigned char *data;
	int ret;

	dwElementAddress = start_address;
	dwElementSize = file->size;

//	printf("Downloading to address = 0x%08x, size = %i\n",
//	       dwElementAddress, dwElementSize);

	data = file->firmware;

	ret = dfuse_dnload_element(dif, dwElementAddress, dwElementSize, data,
				   xfer_size);
	if (ret != 0)
		goto out_free;

	printf("File downloaded successfully\n");
	ret = dwElementSize;

 out_free:
	return ret;
}

int dfuse_do_dnload(struct dfu_if *dif, int xfer_size, struct dfu_file *file,
		    const char *dfuse_options)
{
	int ret;

	if (dfuse_options)
		dfuse_parse_options(dfuse_options);
	mem_layout = parse_memory_layout((char *)dif->alt_name);
//	if (!mem_layout) {
//		errx(EX_IOERR, "Failed to parse memory layout");
//	}
	if (dfuse_unprotect) {
		if (!dfuse_force) {
//			errx(EX_IOERR, "The read unprotect command "
//				"will erase the flash memory"
//				"and can only be used with force\n");
		}
		dfuse_special_command(dif, 0, READ_UNPROTECT);
		printf("Device disconnects, erases flash and resets now\n");
		exit(0);
	}
	if (dfuse_mass_erase) {
		if (!dfuse_force) {
//			errx(EX_IOERR, "The mass erase command "
//				"can only be used with force");
		}
		printf("Performing mass erase, this can take a moment\n");
		dfuse_special_command(dif, 0, MASS_ERASE);
	}
	if (dfuse_address_present) {
		ret = dfuse_do_bin_dnload(dif, xfer_size, file, dfuse_address);
	}
	free_segment_list(mem_layout);

	if (!dfuse_will_reset) {
		dfu_abort_to_idle(dif);
	}

	if (dfuse_leave) {
		dfuse_special_command(dif, dfuse_address, SET_ADDRESS);
		dfuse_dnload_chunk(dif, NULL, 0, 2); /* Zero-size */
	}
	return ret;
}
