/*
 * usbtmc.h
 * This file is part of an open-source test and measurement I/O library.
 * See documentation for details.
 *
 * Copyright (C) 2008, 2011 Stefan Kopp, Gechingen, Germany
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

/* Maximum number of devices serviced by this driver */
#define USBTMC_MAX_DEVICES			 						32

#define USBTMC_SHORT_STR_LEN								20
#define USBTMC_LONG_STR_LEN									200

/* This structure is used to track instrument details */
struct usbtmc_instrument
{
	int minor_number;
	char manufacturer[USBTMC_LONG_STR_LEN];
	char product[USBTMC_LONG_STR_LEN];
	char serial_number[USBTMC_LONG_STR_LEN];
	unsigned short int manufacturer_code;
	unsigned short int product_code;
};

/* Device capabilities...
 * See section 4.2.1.8 of the USBTMC specification for details. */
struct usbtmc_dev_capabilities
{
	char interface_capabilities;
	char device_capabilities;
	char usb488_interface_capabilities;
	char usb488_device_capabilities;
};


/* This structure is used for reading/setting attributes */
struct usbtmc_attribute
{
	unsigned long int attribute;
	unsigned long int value;
};

/* This structure is used to send control messages to minor number zero. */
struct usbtmc_io_control
{
	unsigned int minor_number; /* Target driver/instrument for this message */
	unsigned int command;
	unsigned int argument;
	unsigned int value;
};

/* Command values for control messages (sent to minor number zero) */
#define USBTMC_CONTROL_SET_ATTRIBUTE						1
#define USBTMC_CONTROL_GET_ATTRIBUTE						2
#define USBTMC_CONTROL_REPORT_INSTRUMENT					3
#define USBTMC_CONTROL_IO_OPERATION							4

#define USBTMC_NO_ERROR										0
