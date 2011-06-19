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

	store = NULL;

	return;

}

session_factory::session_factory(string config_store)
{

	store = new configuration_store(config_store);

	return;

}

session_factory::~session_factory()
{

	if (store != NULL)
	{
		delete store;
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

io_session *session_factory::open_session(string resource, bool mode, unsigned int timeout)
{

	io_session *session;
	int n, board;
	vector<string> keys_found;
	vector<string> values_found;

	// Check if this is an alias
	if (resource.find("::") == -1)
	{
		// No "::" found, check if string is in store
		string temp = "";
		try
		{
			temp = store->lookup(resource, keys_found, values_found);
		}
		catch (int e)
		{
			if (e != -OPENTMLIB_ERROR_CSTORE_BAD_ALIAS)
			{
				// Unexpected error... pass up
				throw e;
			}
		}
		if (temp != "")
		{
			resource = temp;
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
		if ((pieces.size() == 1) || (pieces.size() > 1) && (uppercase(pieces[1]) == "INSTR"))
		{
			try
			{
				session = new serial_session(board, mode, 5);
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
				device = pieces[2];
			try
			{
				session = new vxi11_session(pieces[1], device, mode, 5);
			}
			catch (int e)
			{
				throw e;
				return NULL;
			}
			goto session_created;
		}
		if ((pieces.size() == 4) && (uppercase(pieces[3]) == "SOCKET"))
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
				session = new socket_session(pieces[1], port, mode, 5);
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
		if ((pieces.size() == 6) || ((pieces.size() == 5) && (uppercase(pieces[4]) != "INSTR")))
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
			session = new usbtmc_session(mfg_id, model, pieces[3], mode, 5);
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
		// Set default settings
		session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
		session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHARACTER, 10);
		session->set_attribute(OPENTMLIB_ATTRIBUTE_EOL_CHAR, 10);
		session->set_attribute(OPENTMLIB_ATTRIBUTE_TIMEOUT, 5);

		// Apply settings found in configuration store

		for (int k = 0; k < keys_found.size(); k++)
		{
			uppercase(keys_found[k]);
			uppercase(values_found[k]);

			if (keys_found[k] == "TERM_CHAR")
			{
				int term_char;
				istringstream stream(values_found[k]);
				if (!(stream >> term_char))
				{
					throw -OPENTMLIB_ERROR_CSTORE_BAD_VALUE;
					return NULL;
				}
				session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHARACTER, term_char);
			}

			if (keys_found[k] == "TERM_CHAR_ENABLE")
			{
				if ((values_found[k] != "ON") && (values_found[k] != "OFF"))
				{
					throw -OPENTMLIB_ERROR_CSTORE_BAD_VALUE;
					return NULL;
				}
				if (values_found[k] == "ON")
				{
					session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 1);
				}
				if (values_found[k] == "OFF")
				{
					session->set_attribute(OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE, 0);
				}
			}

			if (keys_found[k] == "EOL_CHAR")
			{
				int c;
				istringstream stream(values_found[k]);
				if (!(stream >> c))
				{
					throw -OPENTMLIB_ERROR_CSTORE_BAD_VALUE;
					return NULL;
				}
				if ((c < 0) || (c > 255))
				{
					throw -OPENTMLIB_ERROR_CSTORE_BAD_VALUE;
					return NULL;
				}
				session->set_attribute(OPENTMLIB_ATTRIBUTE_EOL_CHAR, c);
			}

			if (keys_found[k] == "TIMEOUT")
			{
				int seconds;
				istringstream stream(values_found[k]);
				if (!(stream >> seconds))
				{
					throw -OPENTMLIB_ERROR_CSTORE_BAD_VALUE;
					return NULL;
				}
				session->set_attribute(OPENTMLIB_ATTRIBUTE_TIMEOUT, seconds);
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


