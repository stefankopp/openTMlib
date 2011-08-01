/*
 * demo_opentmlib.cpp
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
 *
 * In order to build this program for use with the opentmlib shared library, use something like:
 * g++ -c -o demo_opentmlib.o demo_opentmlib.cpp
 * g++ -o demo_opentmlib demo_opentmlib.o -lopentmlib
 *
 * The makefile supplied with the opentmlib package statically links opentmlib to this program.
 * This is convenient for testing/debugging, but it's probably not want you want to do with your
 * application. In most situations, it is more convenient to use the opentmlib shared library.
 */

#include <iostream>
#include <stdio.h>
#include "session_factory.hpp"
#include "configuration_store.hpp"
#include "opentmlib.hpp"

using namespace std;

int main()
{

	session_factory *factory;
	factory = new session_factory();

	io_session *session;
	session = factory->open_session("fgen", false, 5);

	string response;
	session->write_string("*IDN?");
	session->read_string(response);
	cout << "Instrument ID: " << response << endl;

	factory->close_session(session);
	delete(factory);

	exit(0);

}
