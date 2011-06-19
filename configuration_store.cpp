/*
 * configuration_store.cpp
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
#include <fcntl.h>
#include <errno.h>
#include "opentmlib.hpp"
#include "configuration_store.hpp"

using namespace std;

configuration_store::configuration_store(string store)
{

	store_file = store;
	reload_store();

	return;

}

configuration_store::~configuration_store()
{

	return;

}

void configuration_store::reload_store()
{

	// Open store
	if ((file = open(store_file.c_str(), O_RDONLY)) < 0)
	{
		throw errno;
		return;
	}

	// Load contents to string
	string file_contents;
	file_contents.resize(20 * 1024);
	int ret;
	if ((ret = read(file, (char *) file_contents.c_str(), 20 * 1024)) < 0)
	{
		throw errno;
		return;
	}
	if (ret == 20 * 1024)
	{
		throw -OPENTMLIB_ERROR_CSTORE_FILE_SIZE;
		return;
	}
	file_contents.resize(ret);

	// Process contents
	keys.clear();
	values.clear();
	string line;
	int nl;
	do
	{

		line = "";

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

		// Process this line (ignore empty lines)
		if (line != "")
		{
			int tab = line.find("\t");
			if (tab == -1)
			{
				keys.push_back(line.substr(0, line.length()));
				values.push_back("");
			}
			if (tab > 0)
			{
				keys.push_back(line.substr(0, tab));
				values.push_back(line.substr(tab + 1, line.length() - tab - 1));
			}
		}

	}
	while (nl != -1);

	close(file);

//	for (int j = 0; j < keys.size(); j++)
//	{
//		cout << "Entry " << j << " : " << keys[j] << " = " << values[j] << endl;
//	}

	return;

}

string configuration_store::lookup(string symbolic_name, vector<string> & keys_found, vector<string> & values_found)
{

	string key_wanted;
	string address;

	key_wanted = "[" + symbolic_name + "]";
	keys_found.clear();
	values_found.clear();
	address = "";

	for (int i = 0; i < keys.size(); i++)
	{
		if (keys[i] == key_wanted)
		{
			int j = 1;
			while ((i + j < values.size()) && (values[i + j] != ""))
			{
				keys_found.push_back(keys[i + j]);
				values_found.push_back(values[i + j]);
				if (keys[i + j] == "address")
				{
					address = values[i + j];
				}
				j++;
			}
			if (address == "")
			{
				throw -OPENTMLIB_ERROR_CSTORE_BAD_ALIAS;
				return "";
			}
			else
			{
				return address;
			}
		}
	}

	throw -OPENTMLIB_ERROR_CSTORE_BAD_ALIAS;
	return "";

}
