/*
 * monitor.hpp
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

#ifndef IO_MONITOR_HPP
#define IO_MONITOR_HPP

#define DIRECTION_OUT			0
#define DIRECTION_IN			1

//#include <string>
//#include <boost/tokenizer.hpp>
//#include "opentmlib.hpp"

using namespace std;

class io_monitor
{

public:
	io_monitor(string log_file = "/usr/local/etc/opentmlib.monitor"); // Constructor
	~io_monitor(); // Destructor
	void log(string name, int direction, string message, bool eol = true);

private:
	int fd;

protected:

};

#endif
