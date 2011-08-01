/*
 * session_factory.cpp
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

session_factory::session_factory(string config_store)
{

	store = NULL;
	monitor = NULL;

	if (config_store == "")
	{
		// Use default configuration store file
		store = new configuration_store();
	}
	else
	{
		// Use configuration store file given
		store = new configuration_store(config_store);
	}

	monitor = new io_monitor();

	return;

}

session_factory::~session_factory()
{

	if (store != NULL)
	{
		delete store;
	}

	if (monitor != NULL)
	{
		delete monitor;
	}

	return;

}

string & session_factory::uppercase(string & string_to_change)
{

	for(int i = 0; i < string_to_change.length(); i++)
	{
		string_to_change[i] = toupper(string_to_change[i]);
	}

	return string_to_change;

}

io_session *session_factory::open_session(string resource, bool lock, unsigned int timeout)
{

	string message;
	io_session *session;
	int n, board;
	string alias = "";
	string name;

	// Check if this is an alias
	if (resource.find("::") == -1)
	{
		// No "::" found, check if string is in store
		string temp = store->lookup(resource, "address");
		if (temp != "")
		{
			// Entry was found, use it
			alias = resource;
			resource = temp;
			name = alias;
		}
		else
		{
			name = resource;
		}
	}

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
			//uppercase(remaining);
			pieces.push_back(remaining);
		}
		else
		{
			// There is another "::", get piece before
			piece = remaining.substr(0, m);
			//uppercase(piece);
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
	uppercase(pieces[0]); // To simplify comparisons below

	// Process resource string (serial instruments)
	if ((n = pieces[0].find("ASRL")) != -1)
	{
		if (n + 4 < pieces[0].length())
		{
			istringstream stream(pieces[0].substr(n + 4, pieces[0].length()));
			if (!(stream >> board))
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
			}
			if (board < 0)
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
			}
		}
		else
		{
			board = 0;
		}
		if ((pieces.size() == 1) || (pieces.size() > 1) && (uppercase(pieces[1]) == "INSTR"))
		{

			session = new serial_session(board, lock, 5, monitor);
			session->name = name;
			goto session_created;
		}
		// Bad protocol field
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
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
				throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
			}
			if (board < 0)
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
			}
		}
		else
		{
			board = 0;
		}
		if ((pieces.size() < 4) || (pieces.size() == 4) && (uppercase(pieces[3]) == "INSTR"))
		{
			// This is a VXI-11 instrument
			string device;
			if (pieces.size() == 2)
				device = "inst0";
			if (pieces.size() == 3)
			{
				// Not allowed to uppercase third field as VXI11 logical instrument name is case-sensitive
				string temp = uppercase(pieces[2]);
				if (temp != "INSTR")
					device = pieces[2];
				else
					device = "inst0";
			}
			if (pieces.size() == 4)
			{
				device = pieces[2];
			}
			session = new vxi11_session(pieces[1], device, lock, 5, monitor);
			session->name = name;
			goto session_created;
		}
		if ((pieces.size() == 4) && (uppercase(pieces[3]) == "SOCKET"))
		{
			// Direct socket connection
			int port;
			istringstream stream(pieces[2]);
			if (!(stream >> port))
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
			}
			if ((port < 0) || (port > 0xffff))
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
			}
			session = new socket_session(pieces[1], port, lock, 5, monitor);
			session->name = name;
			goto session_created;
		}
		// Bad protocol field
		throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
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
				throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
			}
			if (board < 0)
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
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
		if ((pieces.size() == 6) || ((pieces.size() == 5) && (uppercase(pieces[4]) != "INSTR")))
		{
			int interface;
			istringstream stream(pieces[4]);
			if (!(stream >> interface))
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
			}
		}
		if ((pieces.size() == 6) && (pieces[5] != "INSTR"))
		{
			throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);
		}
		session = new usbtmc_session(mfg_id, model, pieces[3], lock, 5, monitor);
		session->name = name;
		goto session_created;
	}

	// Bad protocol field
	throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_RESOURCE_STRING);

session_created:

	if (session != NULL)
	{

		// Apply session settings (from configuration store or defaults)
		string temp;

		temp = store->lookup(alias, "term_char");
		if (temp != "")
		{
			int term_char;
			istringstream stream(temp);
			if (!(stream >> term_char))
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_CSTORE_BAD_VALUE);
			}
			session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHARACTER, term_char);
		}
		else
		{
			session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHARACTER, '\n');
		}

		temp = store->lookup(alias, "term_char_enable");
		if (temp != "")
		{
			uppercase(temp);
			if ((temp != "ON") && (temp != "OFF"))
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_CSTORE_BAD_VALUE);
			}
			if (temp == "ON")
			{
				session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
			}
			else
			{
				session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 0);
			}
		}
		else
		{
			session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
		}

		temp = store->lookup(alias, "eol_char");
		if (temp != "")
		{
			int c;
			istringstream stream(temp);
			if (!(stream >> c))
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_CSTORE_BAD_VALUE);
			}
			if ((c < 0) || (c > 255))
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_CSTORE_BAD_VALUE);
			}
			session->set_attribute(OPENTMLIB_ATTRIBUTE_EOL_CHAR, c);
		}
		else
		{
			session->set_attribute(OPENTMLIB_ATTRIBUTE_EOL_CHAR, 10);
		}

		temp = store->lookup(alias, "timeout");
		if (temp != "")
		{
			int seconds;
			istringstream stream(temp);
			if (!(stream >> seconds))
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_CSTORE_BAD_VALUE);
			}
			session->set_attribute(OPENTMLIB_ATTRIBUTE_TIMEOUT, seconds);
		}
		else
		{
			session->set_attribute(OPENTMLIB_ATTRIBUTE_TIMEOUT, 5);
		}

		temp = store->lookup(alias, "tracing");
		if (temp != "")
		{
			uppercase(temp);
			if ((temp != "ON") && (temp != "OFF"))
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_CSTORE_BAD_VALUE);
			}
			if (temp == "ON")
			{
				session->set_attribute(OPENTMLIB_ATTRIBUTE_TRACING, 1);
			}
			else
			{
				session->set_attribute(OPENTMLIB_ATTRIBUTE_TRACING, 0);
			}
		}
		else
		{
			session->set_attribute(OPENTMLIB_ATTRIBUTE_TRACING, 0);
		}

		temp = store->lookup(alias, "set_end_indicator");
		if (temp != "")
		{
			uppercase(temp);
			if ((temp != "ON") && (temp != "OFF"))
			{
				throw_opentmlib_error(-OPENTMLIB_ERROR_CSTORE_BAD_VALUE);
			}
			if (temp == "ON")
			{
				session->set_attribute(OPENTMLIB_ATTRIBUTE_SET_END_INDICATOR, 1);
			}
			else
			{
				session->set_attribute(OPENTMLIB_ATTRIBUTE_SET_END_INDICATOR, 0);
			}
		}

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

configuration_store *session_factory::get_store()
{
	return store;
}
