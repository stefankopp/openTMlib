/*
 * socket_session.hpp
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
#include "io_session.hpp"

#define SOCKET_SESSION_LOCAL_BUFFER_SIZE					1024

using namespace std;

class socket_session : public io_session
{

public:
	socket_session(string address, unsigned short int port);
	~socket_session();
	int write_buffer(char *buffer, int count);
	int read_buffer(char *buffer, int max);
	int set_attribute(unsigned int attribute, unsigned int value);
	int get_attribute(unsigned int attribute, unsigned int *value);
	int io_operation(unsigned int operation, unsigned int value);

private:
	int instrument_socket; // Socket descriptor
	unsigned int timeout; // Timeout (s)
	int term_char_enable; // Termination character enable status (0 = off, 1 = on)
	unsigned char term_character; // Termination character
	char *session_buffer_ptr; // Pointer to local session buffer
	int write_index; // Write index into local session buffer

};
