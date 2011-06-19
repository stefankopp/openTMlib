/*
 * demo_opentmlib.cpp
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

#include <iostream>
#include "session_factory.hpp"
#include "opentmlib.hpp"

using namespace std;

int main()
{

	string response = "";

	session_factory *factory;
	factory = new session_factory("instruments");

	io_session *session;

	try
	{

		session = factory->open_session("dmmusb", false, 5);

		session->clear();

		session->write_string("*RST", true);
		sleep(2);

		session->write_string("*IDN?", true);
		response.resize(200);
		session->read_string(response);
		cout << "Response is " << response << endl;

		session->write_string("READ?", true);
		response.resize(200);
		session->read_string(response);
		cout << "Response is " << response  << endl;

		delete session;

	}

	catch (int e)
	{
		string error_message;
		error_message.resize(120);
		opentmlib_error(e, error_message);
		cout << "Error: " << error_message << endl;
	}

	delete factory;

	return 0;

}
