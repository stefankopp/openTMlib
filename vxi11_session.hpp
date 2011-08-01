/*
 * vxi11_session.hpp
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

#ifndef VXI11_SESSION_HPP
#define VXI11_SESSION_HPP

#include <string>
#include "vxi11.h"
#include "io_session.hpp"
#include "io_monitor.hpp"

using namespace std;

class vxi11_session : public io_session
{

public:
	vxi11_session(string address, string logical_name = "inst0", bool lock = false, unsigned int timeout = 5,
		io_monitor *monitor = NULL);
	~vxi11_session();
	int write_buffer(char *buffer, int count);
	int read_buffer(char *buffer, int max);
	void set_attribute(unsigned int attribute, unsigned int value);
	unsigned int get_attribute(unsigned int attribute);
	void io_operation(unsigned int operation, unsigned int value);

private:
	int throw_proper_error(int error_code);
	CLIENT *vxi11_link; // Link to CORE RPC server
	Device_Link device_link; // Handle to logical instrument
	CLIENT * vxi11_abort_link; // Link to ASYNC RPC server
	unsigned short abort_port; // Port number for abort channel
	unsigned long int max_message_size; // Maximum message size
	long last_operation_error; // Error code returned by last operation
	int abort_socket;

};

#endif
