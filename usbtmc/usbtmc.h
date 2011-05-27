/*
 * usbtmc.h
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
#define USBTMC_MAX_DEVICES			 						64

#define USBTMC_SHORT_STR_LEN								20
#define USBTMC_LONG_STR_LEN									200

/* This structure is used to track instrument details */
struct usbtmc_instrument {
	int minor_number;
	char manufacturer[USBTMC_LONG_STR_LEN];
	char product[USBTMC_LONG_STR_LEN];
	char serial_number[USBTMC_LONG_STR_LEN];
	unsigned short int manufacturer_code;
	unsigned short int product_code;
};

/*
 * This structure is used with USBTMC_IOCTL_GET_CAPABILITIES.
 * See section 4.2.1.8 of the USBTMC specification for details.
 */
struct usbtmc_dev_capabilities {
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
	unsigned int minor_number;
	unsigned int command;
	unsigned int argument;
	unsigned int value;
};

/* Command values for control messages (sent to minor number zero) */
#define USBTMC_CONTROL_SET_ATTRIBUTE						1
#define USBTMC_CONTROL_GET_ATTRIBUTE						2
#define USBTMC_CONTROL_REPORT_INSTRUMENT					3
#define USBTMC_CONTROL_IO_OPERATION							4

/* IO operation values (argument for command USBTMC_CONTROL_IO_OPERATION */
#define USBTMC_IO_OP_INDICATOR_PULSE						1
#define USBTMC_IO_OP_ABORT_WRITE							2
#define USBTMC_IO_OP_ABORT_READ								3
#define USBTMC_IO_OP_USB_CLEAR_OUT_HALT						4
#define USBTMC_IO_OP_USB_CLEAR_IN_HALT						5
#define USBTMC_IO_OP_RESET									6
#define USBTMC_IO_OP_CLEAR									7
#define USBTMC_IO_OP_TRIGGER								8
#define USBTMC_IO_OP_REN_CONTROL							9
#define USBTMC_IO_OP_GO_TO_LOCAL							10
#define USBTMC_IO_OP_LOCAL_LOCKOUT							11

/* Attribute values (argument for command USBTMC_CONTROL_SET_ATTRIBUTE/USBTMC_CONTROL_GET_ATTRIBUTE */
#define USBTMC_ATTRIBUTE_TIMEOUT							1
#define USBTMC_ATTRIBUTE_TERMCHAR_ENABLE					2
#define USBTMC_ATTRIBUTE_TERMCHAR							3
#define USBTMC_ATTRIBUTE_INTERFACE_CAPABILITIES				4
#define USBTMC_ATTRIBUTE_DEVICE_CAPABILITIES				5
#define USBTMC_ATTRIBUTE_USB488_INTERFACE_CAPABILITIES		6
#define USBTMC_ATRIBUTE_USB488_DEVICE_CAPABILITIES			7
#define USBTMC_ATTRIBUTE_STATUS_BYTE						8

enum USBTMC_ERRORS
{

	USBTMC_MINOR_NUMBER_UNUSED = 0x4000,
	USBTMC_MINOR_NUMBER_OUT_OF_RANGE = 0x4001,
	USBTMC_MEMORY_ACCESS_ERROR = 0x4002,
	USBTMC_BULK_OUT_ERROR = 0x4003,
	USBTMC_WRONG_CONTROL_MESSAGE_SIZE = 0x4004,
	USBTMC_WRONG_DRIVER_STATE = 0x4005,
	USBTMC_BULK_IN_ERROR = 0x4006,

};
