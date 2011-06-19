/*
 * io_session.cpp
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
#include <stdio.h>
#include <string.h>
#include "io_session.hpp"

int io_session::write_string(string message, bool eol)
{

	if (eol == true)
	{
		// Append EOL character
		string message_with_eol = message + eol_char;
		return write_buffer((char *) message_with_eol.c_str(), message_with_eol.length());
	}
	else
	{
		// Don't append EOL character
		return write_buffer((char *) message.c_str(), message.length());
	}

}

int io_session::write_int(int value, bool eol)
{

	string value_str;

	value_str = value;
	return write_string(value_str, eol);

}

int io_session::write_binblock(char *buffer, int count)
{

	char length[100];
	char header[100];
	int ret;

	// Assemble and write header
	sprintf(length, "%d", count);
	sprintf(header, "#%d%d", strlen(length), count);
	if ((ret = write_buffer(&header[0], strlen(header))) != strlen(header))
	{
		return -1;
	}

	// write binblock data block
	if ((ret = write_buffer(buffer, count)) != count)
	{
		return -1;
	}

	return count;

}

int io_session::read_string(string &message)
{

	int ret;

	ret = read_buffer((char *) message.c_str(), message.length());

	if (ret >= 0)
		message.resize(ret);

	return ret;

}

int io_session::read_int(int &value)
{

	string value_str;
	int ret;

	ret = read_string(value_str);

	// Convert to int...
	value = 0;
	ret;

}

int io_session::read_binblock(char *buffer, int max)
{

	char header[100];
	int ret;

	// Read first character (should be '#')
	if ((ret = read_buffer(&header[0], 1)) != 1)
	{
		throw -OPENTMLIB_ERROR_BINBLOCK_HEADER;
		return -1;
	}
	if (header[0] != '#')
	{
		throw -OPENTMLIB_ERROR_BINBLOCK_HEADER;
		return -1;
	}

	// Read next character (digits)
	if ((ret = read_buffer(&header[0], 1)) != 1)
	{
		throw -OPENTMLIB_ERROR_BINBLOCK_HEADER;
		return -1;
	}
	int digits = buffer[0] - 48;
	if ((digits < 1) || (digits > 9))
	{
		throw -OPENTMLIB_ERROR_BINBLOCK_HEADER;
		return -1;
	}

	// Read length field (digits characters long)
	if ((ret = read_buffer(&header[0], digits)) != digits)
	{
		throw -OPENTMLIB_ERROR_BINBLOCK_HEADER;
		return -1;
	}
	unsigned int length = 0;
	int j;
	for (j = 0; j < digits; j++)
	{
		length *= 10;
		length += header[j] - 0x30;
	}

	// Make sure buffer provided by caller is large enough for this binblock
	if (length > max)
	{
		throw -OPENTMLIB_ERROR_BINBLOCK_SIZE;
		return -1;
	}

	// Read binblock
	int done = 0;
	int remaining = length;
	while (remaining > 0)
	{
		if ((ret = read_buffer(buffer + done, remaining)) == -1)
		{
			return -1;
		}
		done += ret;
		remaining -= ret;
	}

	return done;

}

void io_session::trigger()
{

	io_operation(OPENTMLIB_OPERATION_TRIGGER, 0);
	return;

}

void io_session::clear()
{

	io_operation(OPENTMLIB_OPERATION_CLEAR, 0);
	return;

}

void io_session::remote()
{

	io_operation(OPENTMLIB_OPERATION_REMOTE, 0);
	return;

}

void io_session::local()
{

	io_operation(OPENTMLIB_OPERATION_LOCAL, 0);
	return;

}

void io_session::lock()
{

	io_operation(OPENTMLIB_OPERATION_LOCK, 0);
	return;

}

void io_session::unlock()
{

	io_operation(OPENTMLIB_OPERATION_UNLOCK, 0);
	return;

}

void io_session::abort()
{

	io_operation(OPENTMLIB_OPERATION_ABORT, 0);
	return;

}

unsigned int io_session::read_stb()
{

	unsigned int value;

	get_attribute(OPENTMLIB_ATTRIBUTE_STATUS_BYTE, &value);

	return value;

}

void io_session::scpi_rst()
{

	write_string("*RST", true);

}

void io_session::scpi_cls()
{

	write_string("*CLS", true);

}

int io_session::scpi_check_errors(int max)
{

	string error_message;
	error_message.resize(120);
	int cycles = 0;

	do
	{
		cycles++;
		write_string("SYSTEM:ERROR?", true);
		read_string(error_message);
	}
	while ((error_message != "+0,\"No error\"") && (cycles < max));

	if (error_message != "+0,\"No error\"")
		return -1;
	else
		return 0;

}
