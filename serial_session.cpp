/*
 * serial_session.cpp
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
#include <iostream>
#include <stdexcept>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include "serial_session.hpp"

using namespace std;

int serial_session::set_basic_options()
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings
	settings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Set line options
	settings.c_oflag &= ~OPOST; // Raw (unprocessed) output
	settings.c_cflag |= CLOCAL | CREAD; // Keep current port owner and enable receiver
	// Disable various input processing options
	settings.c_iflag &= ~(IGNPAR | PARMRK | IGNBRK | BRKINT | INLCR | IGNCR | ICRNL | IUCLC | IMAXBEL);
	tcsetattr(file_descriptor, TCSANOW, &settings); // Activate new settings (immediately)

	return 0;

}

serial_session::serial_session(int port, bool lock, unsigned int lock_timeout, io_monitor *monitor)
{

	if (lock == true)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_LOCKING_NOT_SUPPORTED);
	}

	// Check port number given
	if (port < 0)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_SERIAL_BAD_PORT);
	}

	// Allocate memory for session buffer
	if ((session_buffer_ptr = (char *) malloc(SERIAL_SESSION_LOCAL_BUFFER_SIZE)) == NULL)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_MEMORY_ALLOCATION);
	}

	// Open COM port
	char device_file[20];
	sprintf(device_file, "/dev/ttyS%d", port);
	if ((file_descriptor = open(device_file, O_RDWR | O_NOCTTY | O_NDELAY)) == -1)
	{
		free(session_buffer_ptr);
		throw_opentmlib_error(-OPENTMLIB_ERROR_SERIAL_OPEN);
	}

	// Save current settings, then set basic opions
	tcgetattr(file_descriptor, &old_settings);
	set_basic_options();

	// Initialize member variables
	timeout = 5; // 5 s
	term_char_enable = 1; // Termination character enabled
	term_character = '\n';
	write_index = 0;
	eol_char = '\n';
	string_size = 200;
	throw_on_scpi_error = 1;
	tracing = 0;
	this->monitor = monitor;

	return;

}

serial_session::~serial_session()
{

	// Restore settings saved in constructor
	tcsetattr(file_descriptor, TCSANOW, &old_settings);

	// Close COM port
	if (close(file_descriptor) == -1)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_SERIAL_CLOSE);
	}

	// Free session buffer memory
	free(session_buffer_ptr);

	return;

}

int serial_session::write_buffer(char *buffer, int count)
{

	int bytes_written, done;
	struct timeval timeout_s;
	fd_set writefdset;

	done = 0;

	// Set up timeout structure
	timeout_s.tv_sec = timeout;
	timeout_s.tv_usec = 0;

	// Set up file descriptor set for write
	FD_ZERO(&writefdset);
	FD_SET(file_descriptor, &writefdset);

	do
	{

		if (timeout != 0)
		{
			// Wait for write to become possible
			if (select(file_descriptor + 1, NULL, &writefdset, NULL, &timeout_s) != 1)
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_TIMEOUT);
			}
		}

		// Write buffer to socket
		if ((bytes_written = send(file_descriptor, buffer, count, 0)) == -1)
		{
			throw_opentmlib_error(-errno);
		}

		done += bytes_written;

	}
	while (done < count);

	return done;

}

int serial_session::read_buffer(char *buffer, int max)
{

	int bytes_read, done;
	struct timeval timeout_s;
	fd_set readfdset;

	// Set up timeout structure
	timeout_s.tv_sec = timeout;
	timeout_s.tv_usec = 0;

	// Set up file descriptor set for read
	FD_ZERO(&readfdset);
	FD_SET(file_descriptor, &readfdset);

	if (term_char_enable == 0)
	{

		// Not checking for term character, just read as much data as we get

		if (timeout != 0)
		{
			// Wait for data to become possible
			if (select(file_descriptor + 1, &readfdset, NULL, NULL, &timeout_s) != 1)
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_TIMEOUT);
			}
		}

		// No need to buffer data locally, read directly to target buffer
		if ((bytes_read = read(file_descriptor, buffer, max)) == -1)
		{
			throw_opentmlib_error(-errno);
		}

		return bytes_read;

	}
	else
	{

		// Checking for term character and using local buffer

		if (max > SERIAL_SESSION_LOCAL_BUFFER_SIZE)
		{
			// Potential buffer overflow
			throw_opentmlib_error(-OPENTMLIB_ERROR_SERIAL_REQUEST_TOO_MUCH);
		}

		// Check if data in local buffer includes term character
		// In this case, no recv() should be done
		int i = 0;
		while ((*(session_buffer_ptr + i) != term_character) && (i < write_index)) i++;
		if (i < write_index)
		{
			// Found term character, copy buffer contents up to term character to target buffer
			memcpy(buffer, session_buffer_ptr, i + 1);
			// Copy remaining data (if any) to beginning of buffer
			if (write_index - i - 1 > 0)
				memcpy(session_buffer_ptr, session_buffer_ptr + i + 1, write_index - i - 1);
			write_index -= i + 1;
			return i + 1;
		}

		done = write_index; // Start with what we have in local buffer already

		do
		{

			if (timeout != 0)
			{
				// Wait for data to become possible
				if (select(file_descriptor + 1, &readfdset, NULL, NULL, &timeout_s) != 1)
				{
					throw_opentmlib_error(-OPENTMLIB_ERROR_TIMEOUT);
				}
			}

			// Read data to local buffer
			if ((bytes_read = read(file_descriptor, session_buffer_ptr + done, max - done)) == -1)
			{
				throw_opentmlib_error(-errno);
			}

			// Check if data read includes term character
			int i = 0;
			while ((*(session_buffer_ptr + done + i) != term_character) && (i < bytes_read)) i++;
			if (i < bytes_read)
			{
				// Found term character, copy buffer up to term character to target
				memcpy(buffer, session_buffer_ptr, done + i + 1);
				// Copy remaining data (if any) to beginning of local buffer
				if (bytes_read - i - 1 > 0)
					memcpy(session_buffer_ptr, session_buffer_ptr + done + i + 1, bytes_read - i - 1);
				write_index = bytes_read - i - 1;
				return done + i + 1;
			}

			done += bytes_read;

		}
		while (done < max);

		// No termination character found but max number of bytes requested reached
		// Return what we have...
		memcpy(buffer, session_buffer_ptr, max);
		write_index = 0;
		throw_opentmlib_error(-OPENTMLIB_ERROR_BUFFER_OVERFLOW);

	}

}

void serial_session::set_attribute_baudrate(unsigned int value)
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	// Set speed
	switch(value)
	{
	case 50:
		cfsetispeed(&settings, B50);
		cfsetospeed(&settings, B50);
		break;
	case 75:
		cfsetispeed(&settings, B75);
		cfsetospeed(&settings, B75);
		break;
	case 110:
		cfsetispeed(&settings, B110);
		cfsetospeed(&settings, B110);
		break;
	case 134:
		cfsetispeed(&settings, B134);
		cfsetospeed(&settings, B134);
		break;
	case 150:
		cfsetispeed(&settings, B150);
		cfsetospeed(&settings, B150);
		break;
	case 200:
		cfsetispeed(&settings, B200);
		cfsetospeed(&settings, B200);
		break;
	case 300:
		cfsetispeed(&settings, B300);
		cfsetospeed(&settings, B300);
		break;
	case 600:
		cfsetispeed(&settings, B600);
		cfsetospeed(&settings, B600);
		break;
	case 1200:
		cfsetispeed(&settings, B1200);
		cfsetospeed(&settings, B1200);
		break;
	case 1800:
		cfsetispeed(&settings, B1800);
		cfsetospeed(&settings, B1800);
		break;
	case 2400:
		cfsetispeed(&settings, B2400);
		cfsetospeed(&settings, B2400);
		break;
	case 4800:
		cfsetispeed(&settings, B4800);
		cfsetospeed(&settings, B4800);
		break;
	case 9600:
		cfsetispeed(&settings, B9600);
		cfsetospeed(&settings, B9600);
		break;
	case 19200:
		cfsetispeed(&settings, B19200);
		cfsetospeed(&settings, B19200);
		break;
	case 38400:
		cfsetispeed(&settings, B38400);
		cfsetospeed(&settings, B38400);
		break;
	case 57600:
		cfsetispeed(&settings, B57600);
		cfsetospeed(&settings, B57600);
		break;
	case 115200:
		cfsetispeed(&settings, B115200);
		cfsetospeed(&settings, B115200);
		break;
	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
	}

	tcsetattr(file_descriptor, TCSANOW, &settings); // Activate new settings (immediately)

	return;

}

unsigned int serial_session::get_attribute_baudrate()
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	settings.c_cflag &= B50 | B75 | B110 | B150 | B134 | B150 | B200 | B300 | B600 | B1200 | B1800 |
		B2400 | B4800 | B9600 | B19200 | B38400 | B57600 | B115200;

	switch(settings.c_cflag)
	{
	case B50:
		return 50;
	case B75:
		return 75;
	case B110:
		return 110;
	case B134:
		return 134;
	case B150:
		return 150;
	case B200:
		return 200;
	case B300:
		return 300;
	case B600:
		return 600;
	case B1200:
		return 1200;
	case B1800:
		return 1800;
	case B2400:
		return 2400;
	case B4800:
		return 4800;
	case B9600:
		return 9600;
	case B19200:
		return 19200;
	case B38400:
		return 38400;
	case B57600:
		return 57600;
	case B115200:
		return 115200;
	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
	}

}

void serial_session::set_attribute_size(unsigned int value)
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	// Set character size
	switch(value)
	{
	case 5:
		settings.c_cflag &= ~CSIZE;
		settings.c_cflag |= CS5;
		break;
	case 6:
		settings.c_cflag &= ~CSIZE;
		settings.c_cflag |= CS6;
		break;
	case 7:
		settings.c_cflag &= ~CSIZE;
		settings.c_cflag |= CS7;
		break;
	case 8:
		settings.c_cflag &= ~CSIZE;
		settings.c_cflag |= CS8;
		break;
	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
	}

	tcsetattr(file_descriptor, TCSANOW, &settings); // Activate new settings (immediately)

	return;

}

unsigned int serial_session::get_attribute_size()
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	settings.c_cflag &= CS5 | CS6 | CS7 | CS8;

	switch(settings.c_cflag)
	{
	case CS5:
		return 5;
	case CS6:
		return 6;
	case CS7:
		return 7;
	case CS8:
		return 8;
	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
	}

}

void serial_session::set_attribute_parity(unsigned int value)
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	// Set parity
	switch(value)
	{
	case OPENTMLIB_SERIAL_PARITY_NONE:
		settings.c_cflag &= ~PARENB;
		settings.c_iflag &= ~(INPCK | ISTRIP);
		break;
	case OPENTMLIB_SERIAL_PARITY_EVEN:
		settings.c_cflag |= PARENB;
		settings.c_cflag &= ~PARODD;
		settings.c_iflag |= INPCK | ISTRIP;
		break;
	case OPENTMLIB_SERIAL_PARITY_ODD:
		settings.c_cflag |= PARENB;
		settings.c_cflag |= PARODD;
		settings.c_iflag |= INPCK | ISTRIP;
		break;
	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
	}

	tcsetattr(file_descriptor, TCSANOW, &settings); // Activate new settings (immediately)

	return;

}

unsigned int serial_session::get_attribute_parity()
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	if (!(settings.c_cflag & PARENB))
	{
		return OPENTMLIB_SERIAL_PARITY_NONE;
	}
	if (settings.c_cflag & PARODD)
	{
		return OPENTMLIB_SERIAL_PARITY_ODD;
	}
	return OPENTMLIB_SERIAL_PARITY_EVEN;

}

void serial_session::set_attribute_stopbits(unsigned int value)
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	// Set stop bits
	switch(value)
	{
	case 1:
		settings.c_cflag &= ~CSTOPB;
		break;
	case 2:
		settings.c_cflag |= CSTOPB;
		break;
	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
	}

	tcsetattr(file_descriptor, TCSANOW, &settings); // Activate new settings (immediately)

	return;

}

unsigned int serial_session::get_attribute_stopbits()
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	if (settings.c_cflag & CSTOPB)
	{
		return 2;
	}
	return 1;

}

void serial_session::set_attribute_rtscts(unsigned int value)
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	// Set hardware flow control status
	switch(value)
	{
	case 0:
		settings.c_cflag &= ~CRTSCTS;
		break;
	case 1:
		settings.c_cflag |= CRTSCTS;
		break;
	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
	}

	tcsetattr(file_descriptor, TCSANOW, &settings); // Activate new settings (immediately)

	return;

}

unsigned int serial_session::get_attribute_rtscts()
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	if (settings.c_cflag & CRTSCTS)
	{
		return 1;
	}
	return 0;

}

void serial_session::set_attribute_xonxoff(unsigned int value)
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	// Set software flow control status
	switch(value)
	{
	case 0:
		settings.c_iflag &= ~(IXON | IXOFF | IXANY);
		break;
	case 1:
		settings.c_iflag |= IXON | IXOFF | IXANY;
		break;
	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
	}

	tcsetattr(file_descriptor, TCSANOW, &settings); // Activate new settings (immediately)

	return;

}

unsigned int serial_session::get_attribute_xonxoff()
{

	struct termios settings;

	tcgetattr(file_descriptor, &settings); // Get actual settings

	if ((settings.c_iflag & IXON) && (settings.c_iflag & IXOFF) && (settings.c_iflag & IXANY))
	{
		return 1;
	}
	return 0;

}

void serial_session::set_attribute(unsigned int attribute, unsigned int value)
{

	// Check if attribute is known to parent class
	try
	{
		base_set_attribute(attribute, value);
		return;
	}

	catch (opentmlib_exception & e)
	{
		if (e.code != -OPENTMLIB_ERROR_BAD_ATTRIBUTE)
		{
			// Attribute was processed by parent class but an error was thrown. Pass up...
			throw e;
		}
	}

	switch (attribute)
	{

	case OPENTMLIB_ATTRIBUTE_TRACING:
		if (value > 1)
		{
			throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
		}
		tracing = value;
		break;

	case OPENTMLIB_ATTRIBUTE_EOL_CHAR:
		if (value > 255)
		{
			throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
		}
		eol_char = value;
		break;

	case OPENTMLIB_ATTRIBUTE_TIMEOUT:
		timeout = value;
		break;

	case OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE:
		if (value > 1)
		{
			throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
		}
		term_char_enable = value;
		break;

	case OPENTMLIB_ATTRIBUTE_TERM_CHARACTER:
		if (value > 0xff)
		{
			throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
		}
		term_character = value;
		break;

	case OPENTMLIB_ATTRIBUTE_SERIAL_BAUDRATE:
		set_attribute_baudrate(value);
		break;

	case OPENTMLIB_ATTRIBUTE_SERIAL_SIZE:
		set_attribute_size(value);
		break;

	case OPENTMLIB_ATTRIBUTE_SERIAL_PARITY:
		set_attribute_parity(value);
		break;

	case OPENTMLIB_ATTRIBUTE_SERIAL_STOPBITS:
		set_attribute_stopbits(value);
		break;

	case OPENTMLIB_ATTRIBUTE_SERIAL_RTSCTS:
		set_attribute_rtscts(value);
		break;

	case OPENTMLIB_ATTRIBUTE_SERIAL_XONXOFF:
		set_attribute_xonxoff(value);
		break;

	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE);

	}

	return;

}

unsigned int serial_session::get_attribute(unsigned int attribute)
{

	// Check if attribute is known to parent class
	try
	{
		unsigned int value;
		value = base_get_attribute(attribute);
		return value;
	}

	catch (opentmlib_exception & e)
	{
		if (e.code != -OPENTMLIB_ERROR_BAD_ATTRIBUTE)
		{
			// Attribute was processed by parent class but an error was thrown. Pass up...
			throw e;
		}
	}

	switch (attribute)
	{

	case OPENTMLIB_ATTRIBUTE_TRACING:
		return tracing;

	case OPENTMLIB_ATTRIBUTE_EOL_CHAR:
		return eol_char;

	case OPENTMLIB_ATTRIBUTE_TIMEOUT:
		return timeout;

	case OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE:
		return term_char_enable;

	case OPENTMLIB_ATTRIBUTE_TERM_CHARACTER:
		return term_character;

	case OPENTMLIB_ATTRIBUTE_SOCKET_BUFFER_SIZE:
		return SERIAL_SESSION_LOCAL_BUFFER_SIZE;

	case OPENTMLIB_ATTRIBUTE_SERIAL_BAUDRATE:
		return get_attribute_baudrate();

	case OPENTMLIB_ATTRIBUTE_SERIAL_SIZE:
		return get_attribute_size();

	case OPENTMLIB_ATTRIBUTE_SERIAL_PARITY:
		return get_attribute_parity();

	case OPENTMLIB_ATTRIBUTE_SERIAL_STOPBITS:
		return get_attribute_stopbits();

	case OPENTMLIB_ATTRIBUTE_SERIAL_RTSCTS:
		return get_attribute_rtscts();

	case OPENTMLIB_ATTRIBUTE_SERIAL_XONXOFF:
		return get_attribute_xonxoff();

	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE);

	}

}

void serial_session::io_operation(unsigned int operation, unsigned int value)
{

	switch (operation)
	{

	default:
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_OPERATION);

	}

	return;

}
