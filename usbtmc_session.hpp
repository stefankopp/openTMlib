/*
 * usbtmc_session.hpp
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

using namespace std;

class usbtmc_session : io_session
{

public:
	usbtmc_session(string manufacturer, string product, string serial_number);
	usbtmc_session(int minor);
	~usbtmc_session();
	int write_buffer(char *buffer, int count);
	int read_buffer(char *buffer, int max);
	int set_attribute(unsigned int attribute, unsigned int value);
	int get_attribute(unsigned int attribute, unsigned int *value);
	int io_operation(unsigned int operation, unsigned int value);

private:
	int usbtmc_ko_fd;
	int device_fd;
	int minor_number;

};

enum USBTMC_SESSION_ERRORS
{

	USBTMC_SESSION_OPEN_DRIVER_ERROR = 0x4100,
	USBTMC_SESSION_READ_ERROR = 0x4101,
	USBTMC_SESSION_WRITE_ERROR = 0x4102

};
