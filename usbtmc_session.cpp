/*
 * usbtmc_session.cpp
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

#include <string>
#include <iostream>
#include <string.h>
#include <stdexcept>
#include <stdio.h>
#include <fcntl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "usbtmc_session.hpp"
#include "usbtmc/usbtmc.h"

using namespace std;

usbtmc_session::usbtmc_session(unsigned short int mfg_id, unsigned short int model, string serial_number,
	bool lock, unsigned int lock_timeout, io_monitor *monitor)
{

	int ret;

	if (lock == true)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_LOCKING_NOT_SUPPORTED);
	}

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_OPEN);
	}

	struct usbtmc_io_control control_msg;
	struct usbtmc_instrument instrument_data;
	int minor = 1;

	do
	{

		// Fill control_msg structure
		control_msg.minor_number = 0;
		control_msg.command = USBTMC_CONTROL_REPORT_INSTRUMENT;
		control_msg.argument = minor;

		// Send control message to USBTMC driver
		if ((ret = write(usbtmc_ko_fd, &control_msg, sizeof(struct usbtmc_io_control))) != sizeof(struct usbtmc_io_control))
		{
			if (ret == -OPENTMLIB_ERROR_USBTMC_MINOR_NUMBER_UNUSED)
				goto try_next;
			close(usbtmc_ko_fd);
			throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_WRITE);
		}

		// Read response from USBTMC driver
		if (read(usbtmc_ko_fd, &instrument_data, sizeof(struct usbtmc_instrument)) != sizeof(struct usbtmc_instrument))
		{
			close(usbtmc_ko_fd);
			throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_READ);
		}

		{

			int serial_length = (serial_number.length() <  strlen(instrument_data.serial_number) ?
				serial_number.length() : strlen(instrument_data.serial_number));
			string serial_number_found(instrument_data.serial_number, serial_length);
			if (serial_number != serial_number_found)
				goto try_next;
			if (mfg_id != instrument_data.manufacturer_code)
				goto try_next;
			if (model != instrument_data.product_code)
				goto try_next;

		}

		// Found it! Open this device's minor number
		minor_number = minor;
		char device_file[20];
		sprintf(device_file, "/dev/usbtmc%d", minor);
		if ((device_fd = open(device_file, O_RDWR)) == -1)
		{
			close(usbtmc_ko_fd);
			throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_OPEN);
		}

		// Initialize member variables
		timeout = 5; // 5 s
		term_char_enable = 1; // Termination character enabled
		term_character = '\n';
		eol_char = '\n';
		string_size = 200;
		throw_on_scpi_error = 1;
		tracing = 0;
		this->monitor = monitor;

		close(usbtmc_ko_fd);
		return;

try_next:

		minor_number++;

	}
	while (minor_number <= USBTMC_MAX_DEVICES);

	// Didn't find the device...
	close(usbtmc_ko_fd);
	throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_DEVICE_NOT_FOUND);

}

usbtmc_session::usbtmc_session(string manufacturer, string product, string serial_number, bool lock,
	unsigned int lock_timeout, io_monitor *monitor)
{

	int ret;

	if (lock == true)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_LOCKING_NOT_SUPPORTED);
	}

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_OPEN);
	}

	struct usbtmc_io_control control_msg;
	struct usbtmc_instrument instrument_data;
	int minor = 1;

	do
	{

		// Fill control_msg structure
		control_msg.minor_number = 0;
		control_msg.command = USBTMC_CONTROL_REPORT_INSTRUMENT;
		control_msg.argument = minor;

		// Send control message to USBTMC driver
		if ((ret = write(usbtmc_ko_fd, &control_msg, sizeof(struct usbtmc_io_control))) != sizeof(struct usbtmc_io_control))
		{
			if (ret == -OPENTMLIB_ERROR_USBTMC_MINOR_NUMBER_UNUSED)
				goto try_next;
			close(usbtmc_ko_fd);
			throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_WRITE);
		}

		// Read response from USBTMC driver
		if (read(usbtmc_ko_fd, &instrument_data, sizeof(struct usbtmc_instrument)) != sizeof(struct usbtmc_instrument))
		{
			close(usbtmc_ko_fd);
			throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_READ);
		}

		{

			// Convert strings and compare against wanted instrument
			int manufacturer_length = (manufacturer.length() <  strlen(instrument_data.manufacturer) ?
				manufacturer.length() : strlen(instrument_data.manufacturer));
			int product_length = (product.length() <  strlen(instrument_data.product) ?
					product.length() : strlen(instrument_data.product));
			int serial_length = (serial_number.length() <  strlen(instrument_data.serial_number) ?
					serial_number.length() : strlen(instrument_data.serial_number));
			string manufacturer_found(instrument_data.manufacturer, manufacturer_length);
			string product_found(instrument_data.product, product_length);
			string serial_number_found(instrument_data.serial_number, serial_length);
			if (manufacturer != manufacturer_found)
				goto try_next;
			if (product != product_found)
				goto try_next;
			if (serial_number != serial_number_found)
				goto try_next;

		}

		// Found it! Open this device's minor number
		minor_number = minor;
		char device_file[20];
		sprintf(device_file, "/dev/usbtmc%d", minor);
		if ((device_fd = open(device_file, O_RDWR)) == -1)
		{
			close(usbtmc_ko_fd);
			throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_OPEN);
		}

		// Initialize member variables
		timeout = 5; // 5 s
		term_char_enable = 1; // Termination character enabled
		term_character = '\n';
		eol_char = '\n';
		string_size = 200;
		throw_on_scpi_error = 1;
		tracing = 0;
		this->monitor = monitor;

		close(usbtmc_ko_fd);
		return;

try_next:

		minor_number++;

	}
	while (minor_number <= USBTMC_MAX_DEVICES);

	// Didn't find the device...
	close(usbtmc_ko_fd);
	throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_DEVICE_NOT_FOUND);

}

usbtmc_session::usbtmc_session(int minor, bool lock, unsigned int lock_timeout, io_monitor *monitor)
{

	if (lock == true)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_LOCKING_NOT_SUPPORTED);
	}

	// Make sure minor number is in range
	if ((minor == 0) || (minor > USBTMC_MAX_DEVICES))
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_MINOR_OUT_OF_RANGE);
	}

	// Open device driver
	char device_file[20];
	sprintf(device_file, "/dev/usbtmc%d", minor);
	if ((device_fd = open(device_file, O_RDWR)) == -1)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_OPEN);
	}

	// Initialize member variables
	timeout = 5; // 5 s
	term_char_enable = 1; // Termination character enabled
	term_character = '\n';
	eol_char = '\n';
	string_size = 200;
	throw_on_scpi_error = 1;
	tracing = 0;
	this->monitor = monitor;

	return;

}

usbtmc_session::~usbtmc_session()
{

	// Close special file
	close(device_fd);
	return;

}

int usbtmc_session::write_buffer(char *buffer, int count)
{

	int ret;

	// Write buffer to special file
	ret = write(device_fd, buffer, count);

	if (ret < 0)
	{
		int save_errno = errno;
		io_operation(OPENTMLIB_OPERATION_USBTMC_ABORT_WRITE, 0);
		if (ret == -1)
		{
			// Driver returned a standard error number in errno
			throw_opentmlib_error(-save_errno);
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw_opentmlib_error(ret);
		}
	}

	return ret;

}

int usbtmc_session::read_buffer(char *buffer, int max)
{

	int ret;

	// Read from special file
	ret = read(device_fd, buffer, max);

	if (ret < 0)
	{
		int save_errno = errno;
		io_operation(OPENTMLIB_OPERATION_USBTMC_ABORT_READ, 0);
		if (ret == -1)
		{
			// Driver returned a standard error number in errno
			throw_opentmlib_error(-save_errno);
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw_opentmlib_error(ret);
		}
	}

	return ret;

}

void usbtmc_session::set_attribute(unsigned int attribute, unsigned int value)
{

	struct usbtmc_io_control control_msg;
	int ret;

	// Check if attribute is known to parent class
	try
	{
		base_set_attribute(attribute, value);
		return;
	}

	catch (opentmlib_exception & e)
	{
		if (e.code != -OPENTMLIB_ERROR_BAD_ATTRIBUTE)
		{
			// Attribute was processed by parent class but an error was thrown. Pass up...
			throw e;
		}
	}

	switch (attribute)
	{

	case OPENTMLIB_ATTRIBUTE_TRACING:
		if (value > 1)
		{
			throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
		}
		tracing = value;
		return;

	case OPENTMLIB_ATTRIBUTE_EOL_CHAR:
		if (value > 255)
		{
			throw_opentmlib_error(-OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE);
		}
		eol_char = value;
		return;

	}

	control_msg.minor_number = minor_number;
	control_msg.command = USBTMC_CONTROL_SET_ATTRIBUTE;
	control_msg.argument = attribute;
	control_msg.value = value;

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_OPEN);
	}

	// Send control message to USBTMC driver
	ret = write(usbtmc_ko_fd, &control_msg, sizeof(struct usbtmc_io_control));
	if (ret < 0)
	{
		close(usbtmc_ko_fd);
		if (ret == -1)
		{
			// Driver returned a standard error number in errno
			throw_opentmlib_error(-errno);
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw_opentmlib_error(ret);
		}
	}

	close(usbtmc_ko_fd);
	return;

}

unsigned int usbtmc_session::get_attribute(unsigned int attribute)
{

	struct usbtmc_io_control control_msg;
	int ret;

	// Check if attribute is known to parent class
	try
	{
		unsigned int value;
		value = base_get_attribute(attribute);
		return value;
	}

	catch (opentmlib_exception & e)
	{
		if (e.code != -OPENTMLIB_ERROR_BAD_ATTRIBUTE)
		{
			// Attribute was processed by parent class but an error was thrown. Pass up...
			throw e;
		}
	}

	switch (attribute)
	{

	case OPENTMLIB_ATTRIBUTE_EOL_CHAR:
		return eol_char;

	case OPENTMLIB_ATTRIBUTE_TRACING:
		return tracing;

	}

	control_msg.minor_number = minor_number;
	control_msg.command = USBTMC_CONTROL_GET_ATTRIBUTE;
	control_msg.argument = attribute;

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_OPEN);
	}

	// Send control message to USBTMC driver
	ret = write(usbtmc_ko_fd, &control_msg, sizeof(struct usbtmc_io_control));
	if (ret < 0)
	{
		close(usbtmc_ko_fd);
		if (ret == -1)
		{
			// Driver returned a standard error number in errno
			throw_opentmlib_error(-errno);
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw_opentmlib_error(ret);
		}
	}

	// Read response from USBTMC driver
	unsigned int value;
	ret = read(usbtmc_ko_fd, &value, sizeof(unsigned int));
	if (ret < 0)
	{
		close(usbtmc_ko_fd);
		if (ret == -1)
		{
			// Driver returned a standard error number in errno
			throw_opentmlib_error(-errno);
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw_opentmlib_error(ret);
		}
	}
	if (ret != sizeof(unsigned int))
	{
		close(usbtmc_ko_fd);
		throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_READ_LESS_THAN_EXPECTED);
	}

	close(usbtmc_ko_fd);
	return value; // No error

}

void usbtmc_session::io_operation(unsigned int operation, unsigned int value)
{

	struct usbtmc_io_control control_msg;
	int ret;

	control_msg.minor_number = minor_number;
	control_msg.command = USBTMC_CONTROL_IO_OPERATION;
	control_msg.argument = operation;
	control_msg.value = value;

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw_opentmlib_error(-OPENTMLIB_ERROR_USBTMC_OPEN);
	}

	// Send control message to USBTMC driver
	ret = write(usbtmc_ko_fd, &control_msg, sizeof(struct usbtmc_io_control));
	if (ret < 0)
	{
		close(usbtmc_ko_fd);
		if (ret == -1)
		{
			// Driver returned a standard error number in errno
			throw_opentmlib_error(-errno);
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw_opentmlib_error(ret);
		}
	}

	close(usbtmc_ko_fd);
	return; // No error

}
