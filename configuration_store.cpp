/*
 * configuration_store.cpp
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

#include <fcntl.h>
#include <errno.h>
#include "opentmlib.hpp"
#include "configuration_store.hpp"

using namespace std;

configuration_store::configuration_store(string store)
{

	store_file = store;
	load();

	return;

}

configuration_store::~configuration_store()
{

	return;

}

void configuration_store::load()
{

	int file;

	// Open store file for reading
	if ((file = open(store_file.c_str(), O_RDONLY)) < 0)
	{
		throw_opentmlib_error(-errno);
	}

	// Load complete file into string (even for a large configuration this shouldn't be too much data))
	string file_contents;
	file_contents.resize(CONFIGURATION_STORE_MAX_SIZE);
	int ret;
	if ((ret = read(file, (char *) file_contents.c_str(), CONFIGURATION_STORE_MAX_SIZE)) < 0)
	{
		throw_opentmlib_error(-errno);
	}
	if (ret == CONFIGURATION_STORE_MAX_SIZE)
	{
		// Configuration store file seems to be larger than maximum size expected
		throw_opentmlib_error(-OPENTMLIB_ERROR_CSTORE_FILE_SIZE);
	}
	file_contents.resize(ret);

	// Process contents
	keys.clear();
	values.clear();
	int nl;
	do
	{
		string line = "";

		// Extract one line
		nl = file_contents.find("\n");
		if (nl == -1)
		{
			// No more NL found, this must be the last line
			line = file_contents;
		}
		if (nl == 0)
		{
			// Empty line, skip...
			file_contents = file_contents.substr(1, file_contents.size() - 1);
		}
		if (nl > 0)
		{
			line = file_contents.substr(0, nl);
			file_contents = file_contents.substr(nl + 1, file_contents.size() - nl - 1);
		}

		// Process line (ignore empty lines and lines starting with # character)
		if ((line != "") && (line.substr(0,1) != "#"))
		{
			int space = line.find(" ");
			if (space == -1)
			{
				// No space character in this line, this must be a section name
				keys.push_back(line.substr(0, line.length()));
				values.push_back("");
			}
			if (space > 0)
			{
				// There is a space character, this must be an option/value line
				keys.push_back(line.substr(0, space));
				values.push_back(line.substr(space + 1, line.length() - space - 1));
			}
		}

	}
	while (nl != -1);

	close(file);

	return;

}

void configuration_store::save()
{

	int file;
	string line;

	// Open store file for writing (and truncate)
	if ((file = open(store_file.c_str(), O_WRONLY | O_TRUNC)) < 0)
	{
		throw_opentmlib_error(-errno);
	}

	// Write current database to store file
	string notice = "# openTMlib configuration store file\n";
	if (write(file, notice.c_str(), notice.length()) < 0)
	{
		throw_opentmlib_error(-errno);
	}
	for (int i = 0; i < keys.size(); i++)
	{
		if (values[i] == "")
		{
			// Key is a section name (has no value)
			line = "\n" + keys[i] + "\n";
		}
		else
		{
			line = keys[i] + " " + values[i] + "\n";
		}
		if (write(file, line.c_str(), line.length()) < 0)
		{
			throw_opentmlib_error(-errno);
		}
	}

	close(file);

	return;

}

string configuration_store::lookup(string section, string option)
{

	string key_wanted;

	key_wanted = "[" + section + "]";

	for (int i = 0; i < keys.size(); i++)
	{
		if (keys[i] == key_wanted)
		{
			// Found the wanted section, now look for option in this section
			int j = 1;
			while ((i + j < values.size()) && (values[i + j] != ""))
			{
				if (keys[i + j] == option)
				{
					return values[i + j];
				}
				j++;
			}
			return ""; // Wanted option is not part of this section
		}
	}

	return ""; // Wanted section not found

}

void configuration_store::update(string section, string option, string value)
{

	if ((option == "") || (value == ""))
	{
		// Empty strings are not allowed.
		throw_opentmlib_error(-OPENTMLIB_ERROR_CSTORE_BAD_VALUE);
	}

	string key_wanted = "[" + section + "]";

	for (int i = 0; i < keys.size(); i++)
	{
		if (keys[i] == key_wanted)
		{
			// Found the wanted section, now look for option in this section
			int j = 1;
			while ((i + j < values.size()) && (values[i + j] != ""))
			{
				if (keys[i + j] == option)
				{
					// Option found, update and return
					values[i + j] = value;
					return;
				}
				j++;
			}
			// Option does not exist yet, add it
			keys.insert(keys.begin() + i + 1, option);
			values.insert(values.begin() + i + 1, value);
			return;
		}
	}

	// Whole section does not exist yet, add it
	keys.push_back(key_wanted);
	values.push_back("");
	keys.push_back(option);
	values.push_back(value);

	return;

}

void configuration_store::remove(string section, string option)
{

	string key_wanted;

	key_wanted = "[" + section + "]";

	if (option != "")
	{
		for (int i = 0; i < keys.size(); i++)
		{
			if (keys[i] == key_wanted)
			{
				// Found the wanted section, now look for option in this section
				int j = 1;
				while ((i + j < values.size()) && (values[i + j] != ""))
				{
					if (keys[i + j] == option)
					{
						// Option found, delete
						keys.erase(keys.begin() + i + j);
						values.erase(values.begin() + i + j);
						return;
					}
					j++;
				}
				// Option does not exist
				throw_opentmlib_error(-OPENTMLIB_ERROR_CSTORE_BAD_OPTION);
			}
		}
	}
	else
	{
		for (int i = 0; i < keys.size(); i++)
		{
			if (keys[i] == key_wanted)
			{
				while ((i + 1 < values.size()) && (values[i + 1] != ""))
				{
					keys.erase(keys.begin() + i + 1);
					values.erase(values.begin() + i + 1);
				}
				keys.erase(keys.begin() + i);
				values.erase(values.begin() + i);
				return;
			}
		}
		// Section does not exist
		throw_opentmlib_error(-OPENTMLIB_ERROR_CSTORE_BAD_SECTION);
	}

}
