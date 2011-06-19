/*
 * usbtmc_session.cpp
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
	bool lock, unsigned int lock_timeout)
{

	int ret;

	if (lock == true)
	{
		throw -OPENTMLIB_ERROR_LOCKING_NOT_SUPPORTED;
		return;
	}

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw -OPENTMLIB_ERROR_USBTMC_OPEN;
		return;
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
			throw -OPENTMLIB_ERROR_USBTMC_WRITE;
			goto close_usbtmc_ko;
		}

		// Read response from USBTMC driver
		if (read(usbtmc_ko_fd, &instrument_data, sizeof(struct usbtmc_instrument)) != sizeof(struct usbtmc_instrument))
		{
			throw -OPENTMLIB_ERROR_USBTMC_READ;
			goto close_usbtmc_ko;
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
			throw -OPENTMLIB_ERROR_USBTMC_OPEN;
			goto close_usbtmc_ko;
		}

		// Initialize member variables
		timeout = 5; // 5 s
		term_char_enable = 1; // Termination character enabled
		term_character = '\n';
		eol_char = '\n';

		goto close_usbtmc_ko;

try_next:

		minor_number++;

	}
	while (minor_number <= USBTMC_MAX_DEVICES);

	// Didn't find the device...
	throw -OPENTMLIB_ERROR_USBTMC_DEVICE_NOT_FOUND;

close_usbtmc_ko:

	close(usbtmc_ko_fd);
	return;

}

usbtmc_session::usbtmc_session(string manufacturer, string product, string serial_number, bool lock,
	unsigned int lock_timeout)
{

	int ret;

	if (lock == true)
	{
		throw -OPENTMLIB_ERROR_LOCKING_NOT_SUPPORTED;
		return;
	}

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw -OPENTMLIB_ERROR_USBTMC_OPEN;
		return;
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
			throw -OPENTMLIB_ERROR_USBTMC_WRITE;
			goto close_usbtmc_ko;
		}

		// Read response from USBTMC driver
		if (read(usbtmc_ko_fd, &instrument_data, sizeof(struct usbtmc_instrument)) != sizeof(struct usbtmc_instrument))
		{
			throw -OPENTMLIB_ERROR_USBTMC_READ;
			goto close_usbtmc_ko;
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
			throw -OPENTMLIB_ERROR_USBTMC_OPEN;
			goto close_usbtmc_ko;
		}

		// Initialize member variables
		timeout = 5; // 5 s
		term_char_enable = 1; // Termination character enabled
		term_character = '\n';
		eol_char = '\n';

		goto close_usbtmc_ko;

try_next:

		minor_number++;

	}
	while (minor_number <= USBTMC_MAX_DEVICES);

	// Didn't find the device...
	throw -OPENTMLIB_ERROR_USBTMC_DEVICE_NOT_FOUND;

close_usbtmc_ko:

	close(usbtmc_ko_fd);
	return;

}

usbtmc_session::usbtmc_session(int minor, bool lock, unsigned int lock_timeout)
{

	if (lock == true)
	{
		throw -OPENTMLIB_ERROR_LOCKING_NOT_SUPPORTED;
		return;
	}

	// Make sure minor number is in range
	if ((minor == 0) || (minor > USBTMC_MAX_DEVICES))
	{
		throw -OPENTMLIB_ERROR_USBTMC_MINOR_OUT_OF_RANGE;
		return;
	}

	// Open device driver
	char device_file[20];
	sprintf(device_file, "/dev/usbtmc%d", minor);
	if ((device_fd = open(device_file, O_RDWR)) == -1)
	{
		throw -OPENTMLIB_ERROR_USBTMC_OPEN;
		return;
	}

	// Initialize member variables
	timeout = 5; // 5 s
	term_char_enable = 1; // Termination character enabled
	term_character = '\n';
	eol_char = '\n';

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
			throw -save_errno;
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw ret;
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
			throw -save_errno;
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw ret;
		}
	}

	return ret;

}

int usbtmc_session::set_attribute(unsigned int attribute, unsigned int value)
{

	struct usbtmc_io_control control_msg;
	int ret;

	switch (attribute)
	{

	case OPENTMLIB_ATTRIBUTE_EOL_CHAR:
		if (value > 255)
		{
			throw -OPENTMLIB_ERROR_BAD_ATTRIBUTE_VALUE;
			return -1;
		}
		eol_char = value;
		return 0;

	}

	control_msg.minor_number = minor_number;
	control_msg.command = USBTMC_CONTROL_SET_ATTRIBUTE;
	control_msg.argument = attribute;
	control_msg.value = value;

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw -OPENTMLIB_ERROR_USBTMC_OPEN;
		return -1;
	}

	// Send control message to USBTMC driver
	ret = write(usbtmc_ko_fd, &control_msg, sizeof(struct usbtmc_io_control));
	if (ret < 0)
	{
		if (ret == -1)
		{
			// Driver returned a standard error number in errno
			throw -errno;
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw ret;
			ret = -1;
		}
		goto close_usbtmc_ko;
	}

	ret = 0;

close_usbtmc_ko:

	close(usbtmc_ko_fd);

	return ret;

}

int usbtmc_session::get_attribute(unsigned int attribute, unsigned int *value)
{

	struct usbtmc_io_control control_msg;
	int ret;

	switch (attribute)
	{

	case OPENTMLIB_ATTRIBUTE_EOL_CHAR:
		*value = eol_char;
		return 0;

	}

	control_msg.minor_number = minor_number;
	control_msg.command = USBTMC_CONTROL_GET_ATTRIBUTE;
	control_msg.argument = attribute;

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw -OPENTMLIB_ERROR_USBTMC_OPEN;
		return -1;
	}

	// Send control message to USBTMC driver
	ret = write(usbtmc_ko_fd, &control_msg, sizeof(struct usbtmc_io_control));
	if (ret < 0)
	{
		if (ret == -1)
		{
			// Driver returned a standard error number in errno
			throw -errno;
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw ret;
			ret = -1;
		}
		goto close_usbtmc_ko;
	}

	// Read response from USBTMC driver
	ret = read(usbtmc_ko_fd, value, sizeof(unsigned int));
	if (ret < 0)
	{
		if (ret == -1)
		{
			// Driver returned a standard error number in errno
			throw -errno;
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw ret;
			ret = -1;
		}
		goto close_usbtmc_ko;
	}
	if (ret != sizeof(unsigned int))
	{
		throw -OPENTMLIB_ERROR_USBTMC_READ_LESS_THAN_EXPECTED;
		ret = -1;
		goto close_usbtmc_ko;
	}

	ret = 0; // No error

close_usbtmc_ko:

	close(usbtmc_ko_fd);

	return ret;

}

int usbtmc_session::io_operation(unsigned int operation, unsigned int value)
{

	struct usbtmc_io_control control_msg;
	int ret;

	control_msg.minor_number = minor_number;
	control_msg.command = USBTMC_CONTROL_IO_OPERATION;
	control_msg.argument = operation;
	control_msg.value = value;

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw -OPENTMLIB_ERROR_USBTMC_OPEN;
		return -1;
	}

	// Send control message to USBTMC driver
	ret = write(usbtmc_ko_fd, &control_msg, sizeof(struct usbtmc_io_control));
	if (ret < 0)
	{
		if (ret == -1)
		{
			// Driver returned a standard error number in errno
			throw -errno;
		}
		else
		{
			// Driver returned a non-standard error number, errno is not set
			throw ret;
			ret = -1;
		}
		goto close_usbtmc_ko;
	}

	ret = 0; // No error

close_usbtmc_ko:

	close(usbtmc_ko_fd);

	return ret;

}
