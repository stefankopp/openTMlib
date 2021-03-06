/*
 * session_factory.hpp
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

#ifndef SESSION_FACTORY_HPP
#define SESSION_FACTORY_HPP

#include <string>
#include "io_session.hpp"
#include "configuration_store.hpp"
#include "io_monitor.hpp"

using namespace std;

class session_factory
{

public:
	session_factory(string config_store = ""); // Constructor
	~session_factory(); // Destructor
	io_session *open_session(string resource, bool mode, unsigned int timeout); // Create session
	void close_session(io_session *session_ptr);
	configuration_store *get_store();

private:
	string & uppercase(string & string_to_change);
	configuration_store *store;
	io_monitor *monitor;

protected:

};

#endif
