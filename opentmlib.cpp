/*
 * opentmlib.cpp
 * This file is part of an open-source test and measurement I/O library.
 * See documentation for details.
 *
 * Copyright (C) 2011, Stefan Kopp, Gechingen, Germany
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * The GNU General Public License is available at
 * http://www.gnu.org/copyleft/gpl.html.
 */

#include <string.h>
#include "opentmlib.hpp"

struct error_list usbtmc_errors[] =
{

	/* General error codes */
	{ OPENTMLIB_ERROR_BAD_ATTRIBUTE, "Bad attribute" },
	{ OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE, "Bad attribute value" },
	{ OPENTMLIB_ERROR_BAD_OPERATION, "Bad operation" },
	{ OPENTMLIB_ERROR_BAD_OPERATION_VALUE, "Bad operation value" },
	{ OPENTMLIB_ERROR_TIMEOUT, "Timeout" },
	{ OPENTMLIB_ERROR_MEMORY_ALLOCATION, "Out of memory" },
	{ OPENTMLIB_ERROR_BUFFER_OVERFLOW, "Buffer overflow" },
	{ OPENTMLIB_ERROR_BINBLOCK_HEADER, "Bad binblock header" },
	{ OPENTMLIB_ERROR_BINBLOCK_SIZE, "Binblock too big" },
	{ OPENTMLIB_ERROR_BAD_RESOURCE_STRING, "Bad resource string" },

	/* Error codes specific to socket driver */
	{ OPENTMLIB_ERROR_SOCKET_REQUEST_TOO_MUCH, "Requesting too much data" },
	{ OPENTMLIB_ERROR_SOCKET_CREATE, "Unable to create socket" },
	{ OPENTMLIB_ERROR_SOCKET_CONNECT, "Unable to establish connection" },
	{ OPENTMLIB_ERROR_SOCKET_CLOSE, "Issue closing socket" },

	/* Error codes specific to USBTMC driver */
	{ OPENTMLIB_ERROR_USBTMC_OPEN, "Issue opening device driver" },
	{ OPENTMLIB_ERROR_USBTMC_WRITE, "Issue writing to device driver" },
	{ OPENTMLIB_ERROR_USBTMC_READ, "Issue reading from device driver" },
	{ OPENTMLIB_ERROR_USBTMC_DEVICE_NOT_FOUND, "Device not found" },
	{ OPENTMLIB_ERROR_USBTMC_MINOR_OUT_OF_RANGE, "Minor number out of range" },

	/* Error codes specific to VXI11 driver */
	{ OPENTMLIB_ERROR_VXI11_RPC, "RPC issue" },
	{ OPENTMLIB_ERROR_VXI11_CONNECTION, "Issue connecting to logical device" },
	{ OPENTMLIB_ERROR_VXI11_ABORT_CONNECTION, "Unable to abort connection" },
	{ OPENTMLIB_ERROR_VXI11_LINK, "Link issue" },
	{ OPENTMLIB_ERROR_VXI11_READ, "Issue reading from device" },
	{ OPENTMLIB_ERROR_VXI11_WRITE, "Issue writing to device" },
	{ OPENTMLIB_ERROR_VXI11_READ_STB, "Issue reading status byte" },
	{ OPENTMLIB_ERROR_VXI11_OPERATION, "Issue with operation" },
	{ OPENTMLIB_ERROR_VXI11_OP_UNSUPPORTED, "Operation unsupported" },
	{ OPENTMLIB_ERROR_VXI11_MSG_SIZE, "Issue with message size" },

	/* Error codes specific to serial */
	{ OPENTMLIB_ERROR_SERIAL_OPEN, "Issue opening device driver" },
	{ OPENTMLIB_ERROR_SERIAL_CLOSE, "Issue closing device driver" },
	{ OPENTMLIB_ERROR_SERIAL_BAD_PORT, "Bad serial port" },
	{ OPENTMLIB_ERROR_SERIAL_REQUEST_TOO_MUCH, "Requesting too much data" }

};

void opentmlib_error(int code, string & message)
{

	// Find out how many errors are in teh list
	int number_of_entries = sizeof(usbtmc_errors) / sizeof(struct error_list);

	// Set string to default value (in case we don't find this error)
	message = "";

	// Try to find error in the list and set error message
	for (int i = 0; i < number_of_entries; i++)
	{
		if (usbtmc_errors[i].error_code == code)
		{
			message = usbtmc_errors[i].error_message;
			i = number_of_entries; // Exit loop
		}
	}

	// Try to get error message via strerror()
	if (message == "")
	{
		string message_str;
		message_str.resize(100);
		strerror_r(code, (char *) message_str.c_str(), message_str.length());
		message = message_str;
	}

	return;

}
