/*
 * vxi11_session.cpp
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

#include <string>
#include <iostream>
#include <stdexcept>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include "vxi11_session.hpp"

using namespace std;

vxi11_session::vxi11_session(string address, string logical_name, bool lock, unsigned int lock_timeout)
{

	Create_LinkParms create_link_parms;
	Create_LinkResp *create_link_response;

	// Initialize connection to VXI-11 RPC server in the instrument
	if ((vxi11_link = clnt_create(address.c_str(), DEVICE_CORE, DEVICE_CORE_VERSION, "tcp")) == NULL)
	{
		throw -OPENTMLIB_ERROR_VXI11_CONNECTION;
		return;
	}

	// Initialize VXI-11 link to logical device
	create_link_parms.clientId = 0; // Not used
	create_link_parms.lockDevice = lock; // Do or don't lock device
	create_link_parms.lock_timeout = lock_timeout * 1000; // Timeout in ms
	create_link_parms.device = (char *) logical_name.c_str();
	if ((create_link_response = create_link_1(&create_link_parms, vxi11_link)) == NULL)
	{
		throw -OPENTMLIB_ERROR_VXI11_RPC;
		clnt_destroy(vxi11_link);
		return;
	}

	if (create_link_response->error != 0)
		{
			last_operation_error = create_link_response->error;
			if (throw_proper_error(create_link_response->error) == -1)
			{
				throw -OPENTMLIB_ERROR_VXI11_LINK;
			}
			clnt_destroy(vxi11_link);
			return;
		}

	// Store link ID, abort port etc. for later
	device_link = create_link_response->lid;
	abort_port = create_link_response->abortPort;
	max_message_size = create_link_response->maxRecvSize;

	// Initialize connection to VXI-11 ASYNC server
	struct sockaddr_in abort_address;
	memset(&abort_address, 0, sizeof(struct sockaddr_in));
	abort_address.sin_family = PF_INET; // IPv4
	abort_address.sin_port = htons(abort_port); // Port number
	abort_address.sin_addr.s_addr = inet_addr(address.c_str()); // IP Address
	abort_socket = RPC_ANYSOCK;
	if ((vxi11_abort_link = clnttcp_create(&abort_address, DEVICE_ASYNC, DEVICE_ASYNC_VERSION,
		&abort_socket, 0, 0)) == NULL)
	{
		throw -OPENTMLIB_ERROR_VXI11_ABORT_CONNECTION;
		clnt_destroy(vxi11_link);
		return;
	}

	// Initialize member variables
	timeout = 5; // 5 s
	term_char_enable = 1; // Termination character enabled
	term_character = '\n';
	eol_char = '\n';

	return;

}

vxi11_session::~vxi11_session()
{

	// Tear down link to logical device
	if (destroy_link_1(&device_link, vxi11_link) == NULL)
	{
		throw -OPENTMLIB_ERROR_VXI11_RPC;
		return;
	}

	// Close connection to RPC servers
	clnt_destroy(vxi11_abort_link);
	clnt_destroy(vxi11_link);

	return;

}

int vxi11_session::write_buffer(char *buffer, int count)
{

	Device_WriteParms write_parms;
	Device_WriteResp *write_response;
	long flags;
	int this_chunk, remaining_bytes, done;

	remaining_bytes = count;
	done = 0;

	// Break buffer into pieces no larger than max_message_size
	do
	{

		if (remaining_bytes > (int) max_message_size)
			this_chunk = max_message_size;
		else
			this_chunk = remaining_bytes;

		// Write chunk to logical device
		write_parms.lid = device_link; // Handle to logical instrument
		write_parms.io_timeout = timeout * 1000; // Timeout in ms
		write_parms.lock_timeout = timeout * 1000; // Timeout in ms
		if (wait_lock == 1)
			flags = 1; // Wait for lock (until timeout)
		else
			flags = 0; // Don't wait, return error if lock not possible
		if ((set_end_indicator == 1) && (remaining_bytes <= (int) max_message_size))
			flags |= 0x08;
		write_parms.flags = flags;
		write_parms.data.data_val = buffer + done; // Data to send
		write_parms.data.data_len = this_chunk; // Number of characters to send
		if ((write_response = device_write_1(&write_parms, vxi11_link)) == NULL)
		{
			throw -OPENTMLIB_ERROR_VXI11_RPC;
			return -1;
		}

		if (write_response->error != 0)
		{
			last_operation_error = write_response->error;
			if (throw_proper_error(write_response->error) == -1)
			{
				throw -OPENTMLIB_ERROR_VXI11_WRITE;
			}
			return -1;
		}

		done += write_response->size;
		remaining_bytes -= write_response->size;

	}
	while (done < count);

	return done;

}

int vxi11_session::read_buffer(char *buffer, int max)
{

	Device_ReadParms read_parms;
	Device_ReadResp *read_response;
	long flags;

	// Read from logical instrument
	read_parms.lid = device_link; // Handle to logical instrument
	read_parms.requestSize = max; // Max number of characters
	read_parms.io_timeout = timeout * 1000; // Timeout in ms
	read_parms.lock_timeout = timeout * 1000; // Timeout in ms
	if (wait_lock == 1)
		flags = 1; // Wait for lock (until timeout)
	else
		flags = 0; // Don't wait, return error if lock not possible
	if (term_char_enable == 1)
		flags |= 0x80; // Use term character to terminate read
	read_parms.flags = flags;
	read_parms.termChar = term_character; // Term character
	if ((read_response = device_read_1(&read_parms, vxi11_link)) == NULL)
	{
		throw -OPENTMLIB_ERROR_VXI11_RPC;
		return -1;
	}

	if (read_response->error != 0)
	{
		last_operation_error = read_response->error;
		if (throw_proper_error(read_response->error) == -1)
		{
			throw -OPENTMLIB_ERROR_VXI11_READ;
		}
		return -1;
	}

	// Copy response to target buffer
	memcpy(buffer, read_response->data.data_val, read_response->data.data_len);

	return read_response->data.data_len;

}

int vxi11_session::set_attribute(unsigned int attribute, unsigned int value)
{

	switch (attribute)
	{

	case OPENTMLIB_ATTRIBUTE_EOL_CHAR:
		if (value > 255)
		{
			throw -OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE;
			return -1;
		}
		eol_char = value;
		break;

	case OPENTMLIB_ATTRIBUTE_TIMEOUT:
		timeout = value;
		break;

	case OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE:
		if (value > 1)
		{
			throw -OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE;
			return -1;
		}
		term_char_enable = value;
		break;

	case OPENTMLIB_ATTRIBUTE_TERM_CHARACTER:
		if (value > 0xff)
		{
			throw -OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE;
			return -1;
		}
		term_character = value;
		break;

	case OPENTMLIB_ATTRIBUTE_WAIT_LOCK:
		if (value > 1)
		{
			throw -OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE;
			return -1;
		}
		wait_lock = value;
		break;

	case OPENTMLIB_ATTRIBUTE_SET_END_INDICATOR:
		if (value > 1)
		{
			throw -OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE;
			return -1;
		}
		set_end_indicator = value;
		break;

	default:
		throw -OPENTMLIB_ERROR_BAD_ATTRIBUTE;
		return -1;

	}

	return 0; // No error

}

int vxi11_session::get_attribute(unsigned int attribute, unsigned int *value)
{

	switch (attribute)
	{

	case OPENTMLIB_ATTRIBUTE_EOL_CHAR:
		*value = eol_char;
		break;

	case OPENTMLIB_ATTRIBUTE_TIMEOUT:
		*value = timeout;
		break;

	case OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE:
		*value = term_char_enable;
		break;

	case OPENTMLIB_ATTRIBUTE_TERM_CHARACTER:
		*value = term_character;
		break;

	case OPENTMLIB_ATTRIBUTE_STATUS_BYTE:

		{
			Device_GenericParms parms;
			Device_ReadStbResp *response;
			long flags;

			parms.lid = device_link; // Handle to logical instrument
			if (wait_lock == 1)
					flags = 1; // Wait for lock (until timeout)
				else
					flags = 0; // Don't wait, return error if lock not possible
			parms.flags = flags; // Not used
			parms.lock_timeout = timeout * 1000; // Timeout in ms
			parms.io_timeout = timeout * 1000; // Timeout in ms
			if ((response = device_readstb_1(&parms, vxi11_link)) == NULL)
			{
				throw -OPENTMLIB_ERROR_VXI11_RPC;
				return -1;
			}

			if (response->error != 0)
			{
				last_operation_error = response->error;
				if (throw_proper_error(response->error) == -1)
				{
					throw -OPENTMLIB_ERROR_VXI11_READ_STB;
				}
				return -1;
			}

			*value = response->stb;
		}
		break;

	case OPENTMLIB_ATTRIBUTE_VXI11_MAXRECVSIZE:
		*value = max_message_size;
		break;

	case OPENTMLIB_ATTRIBUTE_WAIT_LOCK:
		*value = wait_lock;
		break;

	case OPENTMLIB_ATTRIBUTE_SET_END_INDICATOR:
		*value = set_end_indicator;
		break;

	case OPENTMLIB_ATTRIBUTE_VXI11_LAST_ERROR:
		*value = last_operation_error;
		break;

	default:
		throw -OPENTMLIB_ERROR_BAD_ATTRIBUTE;
		return -1;

	}

	return 0; // No error

}

int vxi11_session::io_operation(unsigned int operation, unsigned int value)
{

	switch (operation)
	{

	case OPENTMLIB_OPERATION_ABORT:
		{
			Device_Error *response;

			// TODO: Returns NULL, need to find our why!
			if ((response = device_abort_1(&device_link, vxi11_abort_link)) == NULL)
			{
				throw -OPENTMLIB_ERROR_VXI11_RPC;
				return -1;
			}

			if (response->error != 0)
			{
				last_operation_error = response->error;
				if (throw_proper_error(response->error) == -1)
				{
					throw -OPENTMLIB_ERROR_VXI11_ABORT;
				}
				return -1;
			}

		}
		break;

	case OPENTMLIB_OPERATION_TRIGGER:

		{
			Device_GenericParms parms;
			Device_Error *response;
			long flags;

			parms.lid = device_link; // Handle to logical instrument
			if (wait_lock == 1)
				flags = 1; // Wait for lock (until timeout)
			else
				flags = 0; // Don't wait, return error if lock not possible
			parms.flags = flags;
			parms.lock_timeout = timeout * 1000; // Timeout in ms
			parms.io_timeout = timeout * 1000; // Timeout in ms
			if ((response = device_trigger_1(&parms, vxi11_link)) == NULL)
			{
				throw -OPENTMLIB_ERROR_VXI11_RPC;
				return -1;
			}

			if (response->error != 0)
			{
				last_operation_error = response->error;
				if (throw_proper_error(response->error) == -1)
				{
					throw -OPENTMLIB_ERROR_VXI11_TRIGGER;
				}
				return -1;
			}

		}
		break;

	case OPENTMLIB_OPERATION_CLEAR:

		{
			Device_GenericParms parms;
			Device_Error *response;
			long flags;

			parms.lid = device_link; // Handle to logical instrument
			if (wait_lock == 1)
				flags = 1; // Wait for lock (until timeout)
			else
				flags = 0; // Don't wait, return error if lock not possible
			parms.flags = flags;
			parms.lock_timeout = timeout * 1000; // Timeout in ms
			parms.io_timeout = timeout * 1000; // Timeout in ms
			if ((response = device_clear_1(&parms, vxi11_link)) == NULL)
			{
				throw -OPENTMLIB_ERROR_VXI11_RPC;
				return -1;
			}

			if (response->error != 0)
			{
				last_operation_error = response->error;
				if (throw_proper_error(response->error) == -1)
				{
					throw -OPENTMLIB_ERROR_VXI11_CLEAR;
				}
				return -1;
			}

		}
		break;

	case OPENTMLIB_OPERATION_REMOTE:

		{
			Device_GenericParms parms;
			Device_Error *response;
			long flags;

			parms.lid = device_link; // Handle to logical instrument
			if (wait_lock == 1)
				flags = 1; // Wait for lock (until timeout)
			else
				flags = 0; // Don't wait, return error if lock not possible
			parms.flags = flags;
			parms.lock_timeout = timeout * 1000; // Timeout in ms
			parms.io_timeout = timeout * 1000; // Timeout in ms
			if ((response = device_remote_1(&parms, vxi11_link)) == NULL)
			{
				throw -OPENTMLIB_ERROR_VXI11_RPC;
				return -1;
			}

			if (response->error != 0)
			{
				last_operation_error = response->error;
				if (throw_proper_error(response->error) == -1)
				{
					throw -OPENTMLIB_ERROR_VXI11_REMOTE;
				}
				return -1;
			}

		}
		break;

	case OPENTMLIB_OPERATION_LOCAL:

		{
			Device_GenericParms parms;
			Device_Error *response;
			long flags;

			parms.lid = device_link; // Handle to logical instrument
			if (wait_lock == 1)
				flags = 1; // Wait for lock (until timeout)
			else
				flags = 0; // Don't wait, return error if lock not possible
			parms.flags = flags;
			parms.lock_timeout = timeout * 1000; // Timeout in ms
			parms.io_timeout = timeout * 1000; // Timeout in ms
			if ((response = device_local_1(&parms, vxi11_link)) == NULL)
			{
				throw -OPENTMLIB_ERROR_VXI11_RPC;
				return -1;
			}

			if (response->error != 0)
			{
				last_operation_error = response->error;
				if (throw_proper_error(response->error) == -1)
				{
					throw -OPENTMLIB_ERROR_VXI11_LOCAL;
				}
				return -1;
			}

		}
		break;

	case OPENTMLIB_OPERATION_LOCK:

		{
			Device_LockParms parms;
			Device_Error *response;
			long flags;

			parms.lid = device_link; // Handle to logical instrument
			if (wait_lock == 1)
				flags = 1; // Wait for lock (until timeout)
			else
				flags = 0; // Don't wait, return error if lock not possible
			parms.flags = flags;
			parms.lock_timeout = timeout * 1000; // Timeout in ms
			if ((response = device_lock_1(&parms, vxi11_link)) == NULL)
			{
				throw -OPENTMLIB_ERROR_VXI11_RPC;
				return -1;
			}

			if (response->error != 0)
			{
				last_operation_error = response->error;
				if (throw_proper_error(response->error) == -1)
				{
					throw -OPENTMLIB_ERROR_VXI11_LOCK;
				}
				return -1;
			}

		}
		break;

	case OPENTMLIB_OPERATION_UNLOCK:

		{
			Device_Error *response;
			if ((response = device_unlock_1(&device_link, vxi11_link)) == NULL)
			{
				throw -OPENTMLIB_ERROR_VXI11_RPC;
				return -1;
			}

			if (response->error != 0)
			{
				last_operation_error = response->error;
				if (throw_proper_error(response->error) == -1)
				{
					throw -OPENTMLIB_ERROR_VXI11_UNLOCK;
				}
				return -1;
			}

		}
		break;

	default:
		throw -OPENTMLIB_ERROR_BAD_OPERATION;
		return -1;

	}

	return 0;

}

int vxi11_session::throw_proper_error(int error_code)
{

	switch (error_code)
	{
	case 1: // Syntax error
		throw -OPENTMLIB_ERROR_VXI11_SYNTAX;
		break;
	case 3: // Device not accessible
		throw -OPENTMLIB_ERROR_VXI11_DEVICE_NOT_ACCESSIBLE;
		break;
	case 4: // Invalid link identifier
		throw -OPENTMLIB_ERROR_VXI11_INVALID_LINK_ID;
		break;
	case 5: // Parameter error
		throw -OPENTMLIB_ERROR_VXI11_PARAMETER;
		break;
	case 6: // Channel not established
		throw -OPENTMLIB_ERROR_VXI11_CHANNEL_NOT_ESTABLISHED;
		break;
	case 8: // Operation not supported
		throw -OPENTMLIB_ERROR_OPERATION_UNSUPPORTED;
		break;
	case 9: // Out of resources
		throw -OPENTMLIB_ERROR_VXI11_OUT_OF_RESOURCES;
		break;
	case 11: // Device locked by another link
		throw -OPENTMLIB_ERROR_DEVICE_LOCKED;
		break;
	case 12: // No lock held
		throw -OPENTMLIB_ERROR_NO_LOCK_HELD;
		break;
	case 15: // Timeout
		throw -OPENTMLIB_ERROR_TIMEOUT;
		break;
	case 17: // I/O error
		throw -OPENTMLIB_ERROR_IO_ISSUE;
		break;
	case 21: // invalid address
		throw -OPENTMLIB_ERROR_VXI11_INVALID_ADDRESS;
		break;
	case 23: // Abort
		throw -OPENTMLIB_ERROR_TRANSACTION_ABORTED;
		break;
	case 29: // Channel already established
		throw -OPENTMLIB_ERROR_VXI11_CHANNEL_ESTABLISHED;
		break;
	default:
		return -1;
	}

	return 0;

}
