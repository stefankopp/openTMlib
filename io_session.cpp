/*
 * io_session.cpp
 * This file is part of an open-source test and measurement I/O library.
 * See documentation for details.
 *
 * Copyright (C) 2011 Stefan Kopp
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
#include <iostream>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include "io_session.hpp"

using namespace std;

int io_session::write_string(string message, bool eol)
{

	if (eol == true)
	{
		// Append EOL character
		string message_with_eol = message + eol_char;
		if ((tracing == 1) && (monitor != NULL))
		{
			monitor->log(name, DIRECTION_OUT, message_with_eol, true);
		}
		return write_buffer((char *) message_with_eol.c_str(), message_with_eol.length());
	}
	else
	{
		// Don't append EOL character
		if ((tracing == 1) && (monitor != NULL))
		{
			monitor->log(name, DIRECTION_OUT, message, false);
		}
		return write_buffer((char *) message.c_str(), message.length());
	}

}

int io_session::write_int(int value, bool eol)
{

	string value_str;
	stringstream stream;
	stream << value;
	value_str = stream.str();
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
	
	if ((tracing == 1) && (monitor != NULL))
	{
		stringstream stream;
		stream << count;
		monitor->log(name, DIRECTION_OUT, "BINBLOCK (" + stream.str() + " bytes)");
	}

	return count;

}

int io_session::read_string(string & message)
{

	int ret;

	if (message.size() < string_size)
	{
		message.resize(string_size);
	}

	ret = read_buffer((char *) message.c_str(), message.size());

	if (ret >= 0)
		message.resize(ret);

	if ((tracing == 1) && (monitor != NULL))
	{
		monitor->log(name, DIRECTION_IN, message);
	}

	return ret;

}

int io_session::read_int(int & value)
{

	int ret;
	string value_str;

	ret = read_string(value_str);

	istringstream stream(value_str);
	stream >> value;
	if (stream.fail())
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_FORMAT);
	}

	return ret;

}

int io_session::read_binblock(char *buffer, int max)
{

	char header[100];
	int ret, ret_val, done, remaining, digits;
	unsigned int length;

	// Disable termination character handling
	unsigned int tce_state = get_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE);
	if (tce_state == 1)
	{
		set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 0);
	}

	// Read first character (should be '#')
	if ((ret = read_buffer(&header[0], 1)) != 1)
	{
		if (tce_state == 1)
		{
			set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
		}
		throw_opentmlib_error(-OPENTMLIB_ERROR_BINBLOCK_HEADER);
	}
	if (header[0] != '#')
	{
		if (tce_state == 1)
		{
			set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
		}
		throw_opentmlib_error(-OPENTMLIB_ERROR_BINBLOCK_HEADER);
	}

	// Read next character (digits)
	if ((ret = read_buffer(&header[0], 1)) != 1)
	{
		if (tce_state == 1)
		{
			set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
		}
		throw_opentmlib_error(-OPENTMLIB_ERROR_BINBLOCK_HEADER);
	}
	digits = header[0] - 48;
	if ((digits < 1) || (digits > 9))
	{
		if (tce_state == 1)
		{
			set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
		}
		throw_opentmlib_error(-OPENTMLIB_ERROR_BINBLOCK_HEADER);
	}

	// Read length field (digits characters long)
	if ((ret = read_buffer(&header[0], digits)) != digits)
	{
		if (tce_state == 1)
		{
			set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
		}
		throw_opentmlib_error(-OPENTMLIB_ERROR_BINBLOCK_HEADER);
	}
	length = 0;
	int j;
	for (j = 0; j < digits; j++)
	{
		length *= 10;
		length += header[j] - 0x30;
	}

	// Make sure buffer provided by caller is large enough for this binblock
	if (length > max)
	{
		if (tce_state == 1)
		{
			set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
		}
		throw_opentmlib_error(-OPENTMLIB_ERROR_BINBLOCK_SIZE);
	}

	// Read binblock
	done = 0;
	remaining = length;
	while (remaining > 0)
	{
		if ((ret = read_buffer(buffer + done, remaining)) == -1)
		{
			ret_val = -1;
			goto read_binblock_exit;
		}
		done += ret;
		remaining -= ret;
	}

	ret_val = done;

read_binblock_exit:

	// Reenable termination character handling
	if (tce_state == 1)
	{
		set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
	}

	if ((tracing == 1) && (monitor != NULL))
	{
		stringstream stream;
		stream << length;
		monitor->log(name, DIRECTION_IN, "BINBLOCK (" + stream.str() + " bytes)");
	}

	return ret_val;

}

int io_session::query_string(string query, string & response)
{

	write_string(query);
	return read_string(response);

}

int io_session::query_int(string query, int & value)
{

	write_string(query);
	return read_int(value);

}

void io_session::base_set_attribute(unsigned int attribute, unsigned int value)
{

	switch (attribute)
	{

	case OPENTMLIB_ATTRIBUTE_STRING_SIZE:
		if (value < 1)
		{
			throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
		}
		string_size = value;
		break;

	case OPENTMLIB_ATTRIBUTE_ERROR_ON_SCPI_ERROR:
		if (value > 1)
		{
			throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
		}
		throw_on_scpi_error = value;
		break;

	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE);

	}

	return;

}

unsigned int io_session::base_get_attribute(unsigned int attribute)
{

	switch (attribute)
	{

	case OPENTMLIB_ATTRIBUTE_STRING_SIZE:
		return string_size;

	case OPENTMLIB_ATTRIBUTE_ERROR_ON_SCPI_ERROR:
		return throw_on_scpi_error;

	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE);

	}

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

	return get_attribute(OPENTMLIB_ATTRIBUTE_STATUS_BYTE);

}

void io_session::scpi_rst()
{

	write_string("*RST", true);

}

void io_session::scpi_cls()
{

	write_string("*CLS", true);

}

int io_session::scpi_check_errors(vector<string> & list, int max)
{

	string error_message;
	int error_code;
	int cycles = 0;

	list.clear();
	do
	{
		cycles++;
		write_string("SYSTEM:ERROR?");
		read_string(error_message);
		last_scpi_error = error_message;
		istringstream stream(error_message);
		if (!(stream >> error_code))
		{
			throw_opentmlib_error(-OPENTMLIB_ERROR_FORMAT);
		}
		if (error_code != 0)
		{
			if (throw_on_scpi_error == 1)
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_SCPI_ERROR);
			}
			else
			{
				cout << "Error: " << error_message << endl;
				list.push_back(error_message);
			}
		}
	}
	while ((error_code != 0) && (cycles < max));

	if (error_code != 0)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_SCPI_UNABLE_TO_CLEAR);
	}

	return list.size();

}
