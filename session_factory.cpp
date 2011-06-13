/*
 * session_factory.cpp
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

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include "session_factory.hpp"
#include "vxi11_session.hpp"
#include "socket_session.hpp"
#include "usbtmc_session.hpp"
#include "serial_session.hpp"
#include "opentmlib.hpp"

using namespace std;

session_factory::session_factory()
{

	return;

}

session_factory::~session_factory()
{

	return;

}

void session_factory::uppercase(string & string_to_change)
{

	for(int i = 0; i < string_to_change.length(); i++)
	{
		string_to_change[i] = toupper(string_to_change[i]);
	}

	return;

}

io_session *session_factory::open_session(string resource, int mode, unsigned int timeout)
{

	io_session *session;
	int n, board;

	// Break resource string into individual components (separated by "::")
	string remaining = resource;
	int m;
	vector<string> pieces;
	pieces.clear();
	string piece;
	do
	{
		m = remaining.find("::"); // Find first "::"
		if (m == -1)
		{
			// No further "::", this is the last field
			uppercase(remaining);
			pieces.push_back(remaining);
		}
		else
		{
			// There is another "::", get piece before
			piece = remaining.substr(0, m);
			uppercase(piece);
			pieces.push_back(piece);
			if (m + 2 < remaining.npos)
			{
				remaining = remaining.substr(m + 2, remaining.length());
			}
			else
			{
				remaining = "";
			}
		}
	}
	while (m != -1);

	// Process resource string (serial instruments)
	if ((n = pieces[0].find("ASRL")) != -1)
	{
		if (n + 4 < pieces[0].length())
		{
			istringstream stream(pieces[0].substr(n + 4, pieces[0].length()));
			if (!(stream >> board))
			{
				throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
				return NULL;
			}
			if (board < 0)
			{
				throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
				return NULL;
			}
		}
		else
		{
			board = 0;
		}
		if ((pieces.size() == 1) || (pieces.size() > 1) && (pieces[1] == "INSTR"))
		{
			try
			{
				session = new serial_session(board);
			}
			catch (int e)
			{
				throw e;
				return NULL;
			}
			goto session_created;
		}
		// Bad protocol field
		throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
		return NULL;
	}

	// Process resource string (LAN instruments)
	if ((n = pieces[0].find("TCPIP")) != -1)
	{
		unsigned short int port;
		if (n + 5 < pieces[0].length())
		{
			istringstream stream(pieces[0].substr(n + 5, pieces[0].length()));
			if (!(stream >> board))
			{
				throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
				return NULL;
			}
			if (board < 0)
			{
				throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
				return NULL;
			}
		}
		else
		{
			board = 0;
		}
		if ((pieces.size() < 4) || (pieces.size() == 4) && (pieces[3] == "INSTR"))
		{
			// This is a VXI-11 instrument
			string device;
			if (pieces.size() == 2)
				device = "inst0";
			if (pieces.size() == 3)
			{
				if (pieces[2] != "INSTR")
					device = pieces[2];
				else
					device = "inst0";
			}
			if (pieces.size() == 4)
				device = pieces[2];
			try
			{
				session = new vxi11_session(pieces[1], device, false, 5);
			}
			catch (int e)
			{
				throw e;
				return NULL;
			}
			goto session_created;
		}
		if ((pieces.size() == 4) && (pieces[3] == "SOCKET"))
		{
			// Direct socket connection
			int port;
			istringstream stream(pieces[2]);
			if (!(stream >> port))
			{
				throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
				return NULL;
			}
			if ((port < 0) || (port > 0xffff))
			{
				throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
				return NULL;
			}
			try
			{
				session = new socket_session(pieces[1], port);
			}
			catch (int e)
			{
				throw e;
				return NULL;
			}
			goto session_created;
		}
		// Bad protocol field
		throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
		return NULL;
	}

	// USB instruments
	if (((n = pieces[0].find("USB")) != -1) && (pieces.size() >= 4))
	{
		if (n + 3 < pieces[0].length())
		{
			istringstream stream(pieces[0].substr(n + 3, pieces[0].length()));
			if (!(stream >> board))
			{
				throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
				return NULL;
			}
			if (board < 0)
			{
				throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
				return NULL;
			}
		}
		else
		{
			board = 0;
		}
		unsigned int mfg_id;
		sscanf(pieces[1].c_str(), "%x", &mfg_id);
		unsigned int model;
		sscanf(pieces[2].c_str(), "%x", &model);
		if ((pieces.size() == 6) || ((pieces.size() == 5) && (pieces[4] != "INSTR")))
		{
			int interface;
			istringstream stream(pieces[4]);
			if (!(stream >> interface))
			{
				throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
				return NULL;
			}
		}
		if ((pieces.size() == 6) && (pieces[5] != "INSTR"))
		{
			throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
			return NULL;
		}
		try
		{
			session = new usbtmc_session(mfg_id, model, pieces[3]);
		}
		catch (int e)
		{
			throw e;
			return 0;
		}
		goto session_created;
	}

	// Bad protocol field
	throw -OPENTMLIB_ERROR_BAD_RESOURCE_STRING;
	return NULL;

session_created:

	if (session != NULL)
	{
		// Default settings
		session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
	}

	return session;

}

void session_factory::close_session(io_session *session_ptr)
{

	if (session_ptr != NULL)
	{
		delete session_ptr;
	}

	return;

}


