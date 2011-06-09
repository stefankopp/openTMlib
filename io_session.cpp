/*
 * io_session.cpp
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

int io_session::write_string(string message, bool eol)
{

	if (eol == true)
	{
		// Append EOL character
		string message_with_eol = message + "\n";
		return write_buffer((char *) message_with_eol.c_str(), message_with_eol.length());
	}
	else
	{
		// Don't append EOL character
		return write_buffer((char *) message.c_str(), message.length());
	}

}

int io_session::read_string(string &message)
{

	int ret;

	ret = read_buffer((char *) message.c_str(), message.length());

	if (ret >= 0)
		message.resize(ret);

	return ret;

}
