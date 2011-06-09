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

usbtmc_session::usbtmc_session(string manufacturer, string product, string serial_number)
{

	int ret;

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
			if (ret == -USBTMC_MINOR_NUMBER_UNUSED)
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

usbtmc_session::usbtmc_session(int minor)
{

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
	}

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

	int bytes_written;

	// Write buffer to special file
	bytes_written = write(device_fd, buffer, count);

	if (bytes_written == -1)
		throw errno;

	return bytes_written;

}

int usbtmc_session::read_buffer(char *buffer, int max)
{

	int bytes_read;

	// Read from special file
	bytes_read = read(device_fd, buffer, max);

	if (bytes_read == -1)
		throw errno;

	return bytes_read;

}

int usbtmc_session::set_attribute(unsigned int attribute, unsigned int value)
{

	struct usbtmc_io_control control_msg;
	unsigned int usbtmc_attribute;

	// Map OPENTMLIB attribute codes to USBTMC attribute codes
	switch (attribute)
	{
	case OPENTMLIB_ATTRIBUTE_TIMEOUT:
		usbtmc_attribute = USBTMC_ATTRIBUTE_TIMEOUT;
		break;
	case OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE:
		usbtmc_attribute = USBTMC_ATTRIBUTE_TERMCHAR_ENABLE;
		break;
	case OPENTMLIB_ATTRIBUTE_TERM_CHARACTER:
		usbtmc_attribute = USBTMC_ATTRIBUTE_TERMCHAR;
		break;
	default:
		throw -OPENTMLIB_ERROR_BAD_ATTRIBUTE;
		return -1;
	}

	control_msg.minor_number = minor_number;
	control_msg.command = USBTMC_CONTROL_SET_ATTRIBUTE;
	control_msg.argument = usbtmc_attribute;
	control_msg.value = value;

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw -OPENTMLIB_ERROR_USBTMC_OPEN;
		return -1;
	}

	// Send control message to USBTMC driver
	if (write(usbtmc_ko_fd, &control_msg, sizeof(struct usbtmc_io_control)) != sizeof(struct usbtmc_io_control))
	{
		throw -OPENTMLIB_ERROR_USBTMC_WRITE;
		return -1;
	}

	close(usbtmc_ko_fd);

	return 0; // No error

}

int usbtmc_session::get_attribute(unsigned int attribute, unsigned int *value)
{

	struct usbtmc_io_control control_msg;
	int ret;
	unsigned int usbtmc_attribute;

	// Map OPENTMLIB attribute codes to USBTMC attribute codes
	switch (attribute)
	{
	case OPENTMLIB_ATTRIBUTE_TIMEOUT:
		usbtmc_attribute = USBTMC_ATTRIBUTE_TIMEOUT;
		break;
	case OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE:
		usbtmc_attribute = USBTMC_ATTRIBUTE_TERMCHAR_ENABLE;
		break;
	case OPENTMLIB_ATTRIBUTE_TERM_CHARACTER:
		usbtmc_attribute = USBTMC_ATTRIBUTE_TERMCHAR;
		break;
	case OPENTMLIB_ATTRIBUTE_USBTMC_INTERFACE_CAPS:
		usbtmc_attribute = USBTMC_ATTRIBUTE_INTERFACE_CAPABILITIES;
		break;
	case OPENTMLIB_ATTRIBUTE_USBTMC_DEVICE_CAPS:
		usbtmc_attribute = USBTMC_ATTRIBUTE_DEVICE_CAPABILITIES;
		break;
	case OPENTMLIB_ATTRIBUTE_USBTMC_488_INTERFACE_CAPS:
		usbtmc_attribute = USBTMC_ATTRIBUTE_USB488_INTERFACE_CAPABILITIES;
		break;
	case OPENTMLIB_ATTRIBUTE_USBTMC_488_DEVICE_CAPS:
		usbtmc_attribute = USBTMC_ATRIBUTE_USB488_DEVICE_CAPABILITIES;
		break;
	case OPENTMLIB_ATTRIBUTE_STATUS_BYTE:
		usbtmc_attribute = USBTMC_ATTRIBUTE_STATUS_BYTE;
		break;
	default:
		throw -OPENTMLIB_ERROR_BAD_ATTRIBUTE;
		return -1;
	}

	control_msg.minor_number = minor_number;
	control_msg.command = USBTMC_CONTROL_GET_ATTRIBUTE;
	control_msg.argument = usbtmc_attribute;

	if ((usbtmc_ko_fd = open("/dev/usbtmc0", O_RDWR)) == -1)
	{
		throw -OPENTMLIB_ERROR_USBTMC_OPEN;
		return -1;
	}

	// Send control message to USBTMC driver
	if (write(usbtmc_ko_fd, &control_msg, sizeof(struct usbtmc_io_control)) != sizeof(struct usbtmc_io_control))
	{
		throw -OPENTMLIB_ERROR_USBTMC_WRITE;
		ret = -1;
		goto close_usbtmc_ko;
	}


	// Read response from USBTMC driver
	if (read(usbtmc_ko_fd, value, sizeof(unsigned int)) != sizeof(unsigned int))
	{
		throw -OPENTMLIB_ERROR_USBTMC_READ;
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
	if (write(usbtmc_ko_fd, &control_msg, sizeof(struct usbtmc_io_control)) != sizeof(struct usbtmc_io_control))
	{
		throw -OPENTMLIB_ERROR_USBTMC_WRITE;
		ret = -1;
		goto close_usbtmc_ko;
	}

	ret = 0; // No error

close_usbtmc_ko:

	close(usbtmc_ko_fd);

	return ret;

}
