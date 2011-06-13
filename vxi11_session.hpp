/*
 * vxi11_session.hpp
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

#ifndef VXI11_SESSION_HPP
#define VXI11_SESSION_HPP

#include <string>
#include "vxi11.h"
#include "io_session.hpp"

using namespace std;

class vxi11_session : public io_session
{

public:
	vxi11_session(string address, string logical_name, bool lock, unsigned int timeout);
	~vxi11_session();
	int write_buffer(char *buffer, int count);
	int read_buffer(char *buffer, int max);
	int set_attribute(unsigned int attribute, unsigned int value);
	int get_attribute(unsigned int attribute, unsigned int *value);
	int io_operation(unsigned int operation, unsigned int value);

private:
	CLIENT *vxi11_link; // Link to CORE RPC server
	Device_Link device_link; // Handle to logical instrument
	CLIENT * vxi11_abort_link; // Link to ASYNC RPC server
	unsigned short abort_port; // Port number for abort channel
	unsigned long int max_message_size; // Maximum message size
	unsigned int timeout; // Timeout (s)
	int term_char_enable; // Termination character enable status (0 = off, 1 = on)
	unsigned char term_character; // Termination character
	long last_operation_error; // Error code returned by last operation
	unsigned int wait_lock; // Wait for lock (1) or return immediately (0)
	unsigned int set_end_indicator; // Set end indicator with last byte written
	int abort_socket;

};

#endif
