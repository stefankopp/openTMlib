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
#include <iostream>
#include "opentmlib.hpp"

struct error_list usbtmc_errors[] =
{

	/* General error codes */
	{ OPENTMLIB_ERROR_BAD_ATTRIBUTE, "Bad attribute" },
	{ OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE, "Bad attribute value" },
	{ OPENTMLIB_ERROR_BAD_OPERATION, "Bad operation" },
	{ OPENTMLIB_ERROR_BAD_OPERATION_VALUE, "Bad operation value" },
	{ OPENTMLIB_ERROR_MEMORY_ALLOCATION, "Out of memory" },
	{ OPENTMLIB_ERROR_BUFFER_OVERFLOW, "Buffer overflow" },
	{ OPENTMLIB_ERROR_BINBLOCK_HEADER, "Bad binblock header" },
	{ OPENTMLIB_ERROR_BINBLOCK_SIZE, "Binblock too big" },
	{ OPENTMLIB_ERROR_BAD_RESOURCE_STRING, "Bad instrument resource (address) string" },
	{ OPENTMLIB_ERROR_CSTORE_BAD_ALIAS, "Unknown configuration store alias" },
	{ OPENTMLIB_ERROR_CSTORE_BAD_VALUE, "Bad configuration store value" },
	{ OPENTMLIB_ERROR_CSTORE_FILE_SIZE, "Configuration store is too big" },
	{ OPENTMLIB_ERROR_TIMEOUT, "Timeout" },
	{ OPENTMLIB_ERROR_IO_ISSUE, "I/O issue" },
	{ OPENTMLIB_ERROR_TRANSACTION_ABORTED, "Transaction aborted" },
	{ OPENTMLIB_ERROR_DEVICE_LOCKED, "Device is locked" },
	{ OPENTMLIB_ERROR_OPERATION_UNSUPPORTED, "Operation not supported" },
	{ OPENTMLIB_ERROR_NO_LOCK_HELD, "No lock held" },
	{ OPENTMLIB_ERROR_LOCKING_NOT_SUPPORTED, "Locking not supported" },

	/* Error codes specific to socket driver */
	{ OPENTMLIB_ERROR_SOCKET_REQUEST_TOO_MUCH, "Requesting too much data" },
	{ OPENTMLIB_ERROR_SOCKET_CREATE, "Unable to create socket" },
	{ OPENTMLIB_ERROR_SOCKET_CONNECT, "Unable to establish connection" },
	{ OPENTMLIB_ERROR_SOCKET_CLOSE, "Issue closing socket" },

	/* Error codes specific to USBTMC driver */
	{ OPENTMLIB_ERROR_USBTMC_OPEN, "USBTMC: issue opening device driver" },
	{ OPENTMLIB_ERROR_USBTMC_WRITE, "USBTMC: issue writing to device driver" },
	{ OPENTMLIB_ERROR_USBTMC_READ, "USBTMC: issue reading from device driver" },
	{ OPENTMLIB_ERROR_USBTMC_DEVICE_NOT_FOUND, "USBTMC: device not found" },
	{ OPENTMLIB_ERROR_USBTMC_MINOR_OUT_OF_RANGE, "USBTMC: minor number out of range" },
	{ OPENTMLIB_ERROR_USBTMC_MINOR_NUMBER_UNUSED, "USBTMC: minor number unused" },
	{ OPENTMLIB_ERROR_USBTMC_MEMORY_ACCESS_ERROR, "USBTMC: memory addressing issue" },
	{ OPENTMLIB_ERROR_USBTMC_BULK_OUT_ERROR, "USBTMC: error during bulk out transfer" },
	{ OPENTMLIB_ERROR_USBTMC_WRONG_CONTROL_MESSAGE_SIZE, "USBTMC: memory addressing issue" },
	{ OPENTMLIB_ERROR_USBTMC_WRONG_DRIVER_STATE, "USBTMC: wrong driver state" },
	{ OPENTMLIB_ERROR_USBTMC_BULK_IN_ERROR, "USBTMC: error during bulk in transfer" },
	{ OPENTMLIB_ERROR_USBTMC_INVALID_REQUEST, "USBTMC: invalid request" },
	{ OPENTMLIB_ERROR_USBTMC_INVALID_OP_CODE, "USBTMC: invalid operation" },
	{ OPENTMLIB_ERROR_USBTMC_CONTROL_OUT_ERROR, "USBTMC: error during control out request" },
	{ OPENTMLIB_ERROR_USBTMC_CONTROL_IN_ERROR, "USBTMC: error during control in request" },
	{ OPENTMLIB_ERROR_USBTMC_STATUS_UNSUCCESSFUL, "USBTMC: unsuccessful status returned" },
	{ OPENTMLIB_ERROR_USBTMC_FEATURE_NOT_SUPPORTED, "USBTMC: feature not supported" },
	{ OPENTMLIB_ERROR_USBTMC_NO_TRANSFER, "USBTMC: no transfer" },
	{ OPENTMLIB_ERROR_USBTMC_NO_TRANSFER_IN_PROGRESS, "USBTMC: no transfer in progress" },
	{ OPENTMLIB_ERROR_USBTMC_UNABLE_TO_GET_WMAXPACKETSIZE, "USBTMC: unable to get WMAXPACKETSIZE" },
	{ OPENTMLIB_ERROR_USBTMC_UNABLE_TO_CLEAR_BULK_IN, "USBTMC: unable to clear bulk in endpoint" },
	{ OPENTMLIB_ERROR_USBTMC_UNEXPECTED_STATUS, "USBTMC: unexpected status returned" },
	{ OPENTMLIB_ERROR_USBTMC_INVALID_ATTRIBUTE_CODE, "USBTMC: invalid attribute" },
	{ OPENTMLIB_ERROR_USBTMC_INVALID_ATTRIBUTE_VALUE, "USBTMC: invalid attribute value" },
	{ OPENTMLIB_ERROR_USBTMC_INVALID_PARAMETER, "USBTMC: invalid parameter" },
	{ OPENTMLIB_ERROR_USBTMC_RESET_ERROR, "USBTMC: error during USB reset" },
	{ OPENTMLIB_ERROR_USBTMC_READ_LESS_THAN_EXPECTED, "USBTMC: Read less than expected number of bytes" },

	/* Error codes specific to VXI11 driver */
	{ OPENTMLIB_ERROR_VXI11_RPC, "VXI11: RPC issue" },
	{ OPENTMLIB_ERROR_VXI11_CONNECTION, "VXI11: unable to establish connection (CORE channel)" },
	{ OPENTMLIB_ERROR_VXI11_ABORT_CONNECTION, "VXI11: unable to establish connection (ABORT channel)" },
	{ OPENTMLIB_ERROR_VXI11_LINK, "VXI11: unknown error while trying to connect to logical device" },
	{ OPENTMLIB_ERROR_VXI11_READ, "VXI11: unknown error during read operation" },
	{ OPENTMLIB_ERROR_VXI11_WRITE, "VXI11: unknown error during write operation" },
	{ OPENTMLIB_ERROR_VXI11_READ_STB, "VXI11: unknown error while reading status byte" },
	{ OPENTMLIB_ERROR_VXI11_INVALID_LINK_ID, "VXI11: invalid link ID" },
	{ OPENTMLIB_ERROR_VXI11_ABORT, "VXI11: unknown error during ABORT operation" },
	{ OPENTMLIB_ERROR_VXI11_TRIGGER, "VXI11: unknown error during TRIGGER operation" },
	{ OPENTMLIB_ERROR_VXI11_CLEAR, "VXI11: unknown error during CLEAR operation" },
	{ OPENTMLIB_ERROR_VXI11_REMOTE, "VXI11: unknown error during REMOTE operation" },
	{ OPENTMLIB_ERROR_VXI11_LOCAL, "VXI11: unknown error during LOCAL operation" },
	{ OPENTMLIB_ERROR_VXI11_LOCK, "VXI11: unknown error during LOCK operation" },
	{ OPENTMLIB_ERROR_VXI11_UNLOCK, "VXI11: unknown error during UNLOCK operation" },
	{ OPENTMLIB_ERROR_VXI11_SYNTAX, "VXI11: syntax error" },
	{ OPENTMLIB_ERROR_VXI11_DEVICE_NOT_ACCESSIBLE, "VXI11: device not accessible" },
	{ OPENTMLIB_ERROR_VXI11_OUT_OF_RESOURCES, "VXI11: device out of resources" },
	{ OPENTMLIB_ERROR_VXI11_INVALID_ADDRESS, "VXI11: invalid address" },
	{ OPENTMLIB_ERROR_VXI11_PARAMETER, "VXI11: invalid parameter" },
	{ OPENTMLIB_ERROR_VXI11_CHANNEL_NOT_ESTABLISHED, "VXI11: channel not established" },
	{ OPENTMLIB_ERROR_VXI11_CHANNEL_ESTABLISHED, "VXI11: channel already established" },

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
		if (usbtmc_errors[i].error_code == -code)
		{
			message = usbtmc_errors[i].error_message;
			i = number_of_entries; // Exit loop
		}
	}

	// Try to get error message via strerror()
	if (message == "")
	{
		string message_str;
		message_str.resize(120);
		strncpy((char *) message_str.c_str(), strerror(-code), 120);
		message = message_str;
	}

	return;

}
