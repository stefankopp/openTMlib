/*
 * configuration_store.hpp
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

#ifndef CONFIGURATION_STORE_HPP
#define CONFIGURATION_STORE_HPP

#include <string>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

#define CONFIGURATION_STORE_MAX_SIZE		50 * 1024

using namespace std;

class configuration_store
{

public:
	configuration_store(string store = "/usr/local/etc/opentmlib.store"); // Constructor
	~configuration_store(); // Destructor
	void load(); // Load contents of configuration store file
	void save(); // Save configuration to configuration store file
	string lookup(string section, string option); // Find an option in a section and return its value
	void update(string section, string option, string value); // Update or add an option in a section
	void remove(string section, string option = ""); // Remove an option or a complete section

private:
	string store_file;
	vector<string> keys;
	vector<string> values;

protected:

};

#endif

