/*
 * serial_session.hpp
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

#ifndef SERIAL_SESSION_HPP
#define SERIAL_SESSION_HPP

#include <termios.h>
#include <string>
#include "io_session.hpp"

#define SERIAL_SESSION_LOCAL_BUFFER_SIZE					1024

using namespace std;

class serial_session : public io_session
{

public:
	serial_session(int port, bool lock, unsigned int lock_timeout);
	~serial_session();
	int write_buffer(char *buffer, int count);
	int read_buffer(char *buffer, int max);
	int set_attribute(unsigned int attribute, unsigned int value);
	int get_attribute(unsigned int attribute, unsigned int *value);
	int io_operation(unsigned int operation, unsigned int value);

private:
	int set_basic_options();
	int set_attribute_baudrate(unsigned int value);
	int get_attribute_baudrate(unsigned int *value);
	int set_attribute_size(unsigned int value);
	int get_attribute_size(unsigned int *value);
	int set_attribute_parity(unsigned int value);
	int get_attribute_parity(unsigned int *value);
	int set_attribute_stopbits(unsigned int value);
	int get_attribute_stopbits(unsigned int *value);
	int set_attribute_rtscts(unsigned int value);
	int get_attribute_rtscts(unsigned int *value);
	int set_attribute_xonxoff(unsigned int value);
	int get_attribute_xonxoff(unsigned int *value);

private:
	int file_descriptor;
	struct termios old_settings;
	char *session_buffer_ptr; // Pointer to local session buffer
	int write_index; // Write index into local session buffer

};

#endif
