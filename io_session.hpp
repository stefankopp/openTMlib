/*
 * io_session.hpp
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

#ifndef IO_SESSION_HPP
#define IO_SESSION_HPP

#include <string>
#include <boost/tokenizer.hpp>
#include "opentmlib.hpp"
#include "io_monitor.hpp"

using namespace std;

class io_session
{

// Basic I/O methods to be implemented by various session types/classes
public:
	virtual int write_buffer(char *buffer, int count) = 0; // Write <count> bytes from buffer to device
	virtual int read_buffer(char *buffer, int max) = 0; // Read up to <max> bytes from device to buffer
	virtual void set_attribute(unsigned int attribute, unsigned int value) = 0; // Set attribute
	virtual unsigned int get_attribute(unsigned int attribute) = 0; // Get attribute
	virtual void io_operation(unsigned int operation, unsigned int value = 0) = 0; // Perform special I/O operation
	string name;

// Higher-level I/O methods which session classes will inherit/share
public:
	int write_string(string message, bool eol = true); // Write string (with or without NL character)
	int read_string(string & message); // Read string
	int write_binblock(char *buffer, int count); // Write arbitrary length binblock
	int read_binblock(char *buffer, int max); // Read arbitrary length binblock
	int write_int(int value, bool eol = true); // Write int value (as string)
	int read_int(int & value); // Read int value (as string)
	int query_string(string query, string & response); // Combination of write_string and read_string
	int query_int(string query, int & value); // Combination of write_string and read_int
	void trigger();
	void clear();
	void remote();
	void local();
	void lock();
	void unlock();
	void abort();
	unsigned int read_stb();
	void scpi_rst();
	void scpi_cls();
	int scpi_check_errors(vector<string> & list, int max = 20);
	string last_scpi_error;

private:

protected:
	void base_set_attribute(unsigned int attribute, unsigned int value);
	unsigned int base_get_attribute(unsigned int attribute);
	int string_size;
	int throw_on_scpi_error;
	int tracing;
	int term_char_enable; // Termination character enable status (0 = off, 1 = on)
	unsigned char term_character; // Termination character
	char eol_char; // End of line character (for write)
	unsigned int timeout; // Timeout (s)
	unsigned int wait_lock; // Wait for lock (1) or return immediately (0)
	unsigned int set_end_indicator; // Set end indicator with last byte written
	io_monitor *monitor;

};

#endif
