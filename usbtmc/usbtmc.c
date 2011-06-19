/*
 * usbtmc.c (kernel driver for USBTMC devices)
 * This file is part of an open-source test and measurement I/O library.
 * See documentation for details.
 *
 * Copyright (C) 2007, 2011, Stefan Kopp, Gechingen, Germany
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

#include <linux/usb.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/ioctl.h>
#include <linux/slab.h>

#include "usbtmc.h"
#include "../opentmlib.h"

#define USBTMC_DEBUG

/* Define for debugging messages. If USBTMC_DEBUG is defined, messages are sent to kernel log. */
#ifdef USBTMC_DEBUG
	#define PDEBUG(fmt, args...) printk(KERN_DEBUG "usbtmc: " fmt, ## args)
#else
	#define PDEBUG(fmt, args...) /* Do nothing */
#endif

/* Size of driver internal buffer for regular I/O (bytes). Must be a multiple of 4 and at least as large
 * as USB parameter wMaxPacketSize (which is usually 512 bytes). */
#define USBTMC_SIZE_IOBUFFER 							4096

/* Default timeout (jiffies) */
#define USBTMC_DEFAULT_TIMEOUT 							5 * HZ

/* Maximum number of read cycles to empty bulk in endpoint during CLEAR and ABORT_BULK_IN requests.
 * Ends the loop if (for whatever reason) a short packet is not read in time. */
#define USBTMC_MAX_READS_TO_CLEAR_BULK_IN				100

/* Driver state */
#define USBTMC_DRV_STATE_CLOSED							0
#define USBTMC_DRV_STATE_OPEN							1

/* USBTMC base class status values */
#define USBTMC_STATUS_SUCCESS							0x01
#define USBTMC_STATUS_PENDING							0x02
#define USBTMC_STATUS_FAILED							0x80
#define USBTMC_STATUS_TRANSFER_NOT_IN_PROGRESS			0x81
#define USBTMC_STATUS_SPLIT_NOT_IN_PROGRESS				0x82
#define USBTMC_STATUS_SPLIT_IN_PROGRESS					0x83
/* USB488 sub class status values */
#define USBTMC_STATUS_STATUS_INTERRUPT_IN_BUSY			0x20

/* USBTMC base class bRequest values */
#define USBTMC_BREQUEST_INITIATE_ABORT_BULK_OUT			1
#define USBTMC_BREQUEST_CHECK_ABORT_BULK_OUT_STATUS		2
#define USBTMC_BREQUEST_INITIATE_ABORT_BULK_IN			3
#define USBTMC_BREQUEST_CHECK_ABORT_BULK_IN_STATUS		4
#define USBTMC_BREQUEST_INITIATE_CLEAR					5
#define USBTMC_BREQUEST_CHECK_CLEAR_STATUS				6
#define USBTMC_BREQUEST_GET_CAPABILITIES				7
#define USBTMC_BREQUEST_INDICATOR_PULSE					64
/* USB488 sub class bRequest values */
#define USBTMC_BREQUEST_READ_STATUS_BYTE				128
#define USBTMC_BREQUEST_REN_CONTROL						160
#define USBTMC_BREQUEST_GO_TO_LOCAL						161
#define USBTMC_BREQUEST_LOCAL_LOCKOUT					162

/* USBTMC MsgID values */
#define USBTMC_MSGID_DEV_DEP_MSG_OUT					1
#define USBTMC_MSGID_DEV_DEP_MSG_IN						2
#define USBTMC_MSGID_REQUEST_DEV_DEP_MSG_IN				2
#define USBTMC_MSGID_VENDOR_SPECIFIC_OUT				126
#define USBTMC_MSGID_VENDOR_SPECIFIC_IN					127
#define USBTMC_MSGID_REQUEST_VENDOR_SPECIFIC_IN			127
#define USBTMC_MSGID_TRIGGER							128

/* This list defines which devices are handled by this driver. This driver handles USBTMC devices, so
 * we look for the corresponding class (application specific) and subclass (USBTMC). */
static struct usb_device_id usbtmc_devices[] =
{
	{
		.match_flags = USB_DEVICE_ID_MATCH_INT_CLASS | USB_DEVICE_ID_MATCH_INT_SUBCLASS,
		.bInterfaceClass = 254, /* Application specific */
		.bInterfaceSubClass = 3 /* Test and measurement class (USBTMC) */
	},
	{ } /* Empty (terminating) entry */
};

/* Base (first) major/minor number to be used. The major number used is allocated dynamically in
 * usbtmc_init. */
static dev_t dev;

/* This array will hold the private data pointers of the instruments. It is used by the driver to get
 * access to the various instrument/USB sessions and retrieve instrument information. It is also used
 * to track the status of the minor numbers allocated by the driver (NULL = minor number unused). */
static struct usbtmc_device_data *usbtmc_devs[USBTMC_MAX_DEVICES];

/* This structure holds private data for each USBTMC device. One copy is allocated for each device. */
struct usbtmc_device_data
{
	struct cdev cdev; /* Character device structure */
	int devno; /* Major and minor number used */
	struct usb_interface *intf; /* USB interface structure */
	const struct usb_device_id *id;
	unsigned int bulk_in_endpoint; /* Bulk in endpoint */
	unsigned int bulk_out_endpoint; /* Bulk out endpoint */
	unsigned int interrupt_in_endpoint; /* Interrupt IN endpoint */
	u8 bTag; /* bTag (transfer identifier) for next transfer */
	struct usb_device *usb_dev; /* USB device structure */
	int driver_state; /* Open, closed... */
	u8 term_char; /* Termination character */
	int term_char_enabled; /* Terminate read automatically? */
	unsigned short int timeout; /* Timeout value (s) */
	/* Last bTag values (needed for abort) */
	unsigned char usbtmc_last_write_bTag;
	unsigned char usbtmc_last_read_bTag;
	unsigned int number_of_bytes;
};

/* This structure holds registration information for the driver. The information is passed to the system
 * through usb_register(), called in the driver's init function. */
static struct usb_driver usbtmc_driver;

/* Pointer to buffer for I/O data (allocated in usbtmc_init). */
static unsigned char *usbtmc_buffer;

/* Forward declarations */
static int usbtmc_probe(struct usb_interface *, const struct usb_device_id *);
static void usbtmc_disconnect(struct usb_interface *);
int usbtmc_open(struct inode *, struct file *);
int usbtmc_release(struct inode *, struct file *);
ssize_t usbtmc_read(struct file *, char __user *, size_t, loff_t *);
ssize_t usbtmc_write(struct file *, const char __user *, size_t, loff_t *);
loff_t usbtmc_llseek(struct file *, loff_t, int);
int usbtmc_dispatch_control_message(struct usbtmc_io_control *control_message);
int usbtmc_control_report_instrument(struct usbtmc_io_control *control_message);
int usbtmc_control_io_operation(struct usbtmc_io_control *control_message);
int usbtmc_trigger(struct usbtmc_io_control *control_message);
int usbtmc_get_stb(struct usbtmc_io_control *control_message, unsigned int *value);
int usbtmc_abort_bulk_out(struct usbtmc_io_control *control_message);
int usbtmc_clear_in_halt(struct usbtmc_io_control *control_message);
int usbtmc_clear_out_halt(struct usbtmc_io_control *control_message);
int usbtmc_control_set_attribute(struct usbtmc_io_control *control_message);
int usbtmc_control_get_attribute(struct usbtmc_io_control *control_message);
int usbtmc_indicator_pulse(struct usbtmc_io_control *control_message);
int usbtmc_abort_bulk_in(struct usbtmc_io_control *control_message);
int usbtmc_reset_conf(struct usbtmc_io_control *control_message);
int usbtmc_clear(struct usbtmc_io_control *control_message);
int usbtmc_get_capabilities(struct usbtmc_io_control *control_message, struct usbtmc_dev_capabilities *caps);
int usbtmc_ren_control(struct usbtmc_io_control *control_message);
int usbtmc_go_to_local(struct usbtmc_io_control *control_message);
int usbtmc_local_lockout(struct usbtmc_io_control *control_message);
	
/* This structure is used to pass information about this USB driver to the USB core (via usb_register). */
static struct usb_driver usbtmc_driver =
{
	.name = "USBTMC", /* Driver name */
	.id_table = usbtmc_devices, /* Devices handles by the driver */
	.probe = usbtmc_probe, /* Probe function (called when USBTMC device is connected) */
	.disconnect = usbtmc_disconnect /* Disconnect function (called when USBTMC device is disconnected) */
};

/* File_operations structure... This is used to publish the char device driver entry points. */
static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.read = usbtmc_read,
	.write = usbtmc_write,
	.open = usbtmc_open,
	.release = usbtmc_release,
	.llseek = usbtmc_llseek,
};

int usbtmc_verify_state(struct usbtmc_device_data *p_device_data, int driver_state)
{

	/* Verify if this minor number is in use */
	if (p_device_data == NULL)
	{
		PDEBUG("Minor number not in use\n");
		return -OPENTMLIB_ERROR_USBTMC_MINOR_NUMBER_UNUSED;
	}

	/* Verify if driver is in correct state */
	if (p_device_data->driver_state != driver_state)
	{
		PDEBUG("Wrong driver state\n");
		return -OPENTMLIB_ERROR_USBTMC_WRONG_DRIVER_STATE;
	}

	return 0; // No error

}

/* This method is called when opening an instrument device file. It looks for the device's USB endpoints
 * for later access. */
int usbtmc_open(struct inode *inode, struct file *filp)
{

	struct usbtmc_device_data *p_device_data;
	unsigned char n, bulk_in_endpoint, bulk_out_endpoint, interrupt_in_endpoint;
	struct usb_host_interface *current_setting;
	struct usb_endpoint_descriptor *endpoint;
	int ret;
		
	PDEBUG("usbtmc_open() called\n");
	
	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[iminor(inode)];
	
	/* Verify pointer and driver state */
	if ((ret = usbtmc_verify_state(p_device_data, USBTMC_DRV_STATE_CLOSED)) != USBTMC_NO_ERROR)
		return ret;
	
	/* Store pointer in file structure's private data field for access by other entry points */
	filp->private_data = p_device_data;
	
	if (iminor(inode) == 0)
		goto minor_null;

	/* USBTMC devices have only one setting, so use that (current setting) */
	current_setting = p_device_data->intf->cur_altsetting;

	/* Find bulk in endpoint */
	bulk_in_endpoint = 0;
	for (n = 0; n < current_setting->desc.bNumEndpoints; n++)
	{
		endpoint = &(current_setting->endpoint[n].desc);
		if ((endpoint->bEndpointAddress & USB_DIR_IN) &&
			((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK))
		{
			bulk_in_endpoint = endpoint->bEndpointAddress;
			PDEBUG("Found bulk in endpoint at %u\n", bulk_in_endpoint);
			n = current_setting->desc.bNumEndpoints; /* Exit loop */
		}
	}
	p_device_data->bulk_in_endpoint = bulk_in_endpoint;

	/* Find bulk out endpoint */
	bulk_out_endpoint = 0;
	for (n = 0; n < current_setting->desc.bNumEndpoints; n++)
	{
		endpoint = &(current_setting->endpoint[n].desc);
		if (!(endpoint->bEndpointAddress & USB_DIR_IN) &&
			((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)== USB_ENDPOINT_XFER_BULK))
		{
			bulk_out_endpoint = endpoint->bEndpointAddress;
			PDEBUG("Found bulk out endpoint at %u\n", bulk_out_endpoint);
			n = current_setting->desc.bNumEndpoints; /* Exit loop */
		}
	}
	p_device_data->bulk_out_endpoint = bulk_out_endpoint;

	/* See if this device has an interrupt in endpoint */
	interrupt_in_endpoint = 0;
	for (n = 0; n < current_setting->desc.bNumEndpoints; n++)
	{
		endpoint = &(current_setting->endpoint[n].desc);
		if (!(endpoint->bEndpointAddress & USB_DIR_IN) &&
			((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)== USB_ENDPOINT_XFER_INT))
		{
			interrupt_in_endpoint = endpoint->bEndpointAddress;
			PDEBUG("Found interrupt in endpoint at %u\n", interrupt_in_endpoint);
			n = current_setting->desc.bNumEndpoints; /* Exit loop */
		}
	}
	p_device_data->interrupt_in_endpoint = interrupt_in_endpoint;

minor_null:
	
	/* Update driver state */
	p_device_data->driver_state = USBTMC_DRV_STATE_OPEN;
	
	return USBTMC_NO_ERROR;
	
}

/* This method is called when closing the instrument device file. */
int usbtmc_release(struct inode *inode, struct file *filp)
{

	struct usbtmc_device_data *p_device_data;
	int ret;
			
	PDEBUG("usbtmc_release() called\n");
	
	/* Get pointer to private data structure */
	p_device_data = filp->private_data;
	
	/* Verify pointer and driver state */
	if ((ret = usbtmc_verify_state(p_device_data, USBTMC_DRV_STATE_OPEN)) != USBTMC_NO_ERROR)
		return ret;
	
	/* Update driver state */
	p_device_data->driver_state = USBTMC_DRV_STATE_CLOSED;
	
	return USBTMC_NO_ERROR;

}

/* This function reads the instrument's output buffer through a USMTMC DEV_DEP_MSG_IN message. */
ssize_t usbtmc_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{

	struct usbtmc_device_data *p_device_data;
	unsigned int pipe;
	int ret, actual, remaining, done, this_part;
	unsigned long int num_of_characters; // TODO: Shouldn't a normal unsigned int be enough?
	
	PDEBUG("usbtmc_read() called\n");

	/* Get pointer to private data structure */
	p_device_data = filp->private_data;
	
	/* Verify pointer and driver state */
	if ((ret = usbtmc_verify_state(p_device_data, USBTMC_DRV_STATE_OPEN)) != USBTMC_NO_ERROR)
		return ret;
	
	if (MINOR(p_device_data->devno) == 0)
		goto minor_null;

	usbtmc_devs[0]->number_of_bytes = 0; /* In case of data left over in buffer for minor number zero */
		
	remaining = count;
	done = 0;
	
	while (remaining > 0)
	{

		/* Check if remaining data bytes to be read fit in the driver's buffer. Make sure there is enough
		 * space for the header (12 bytes) and alignment bytes (up to 3 bytes). */
		if (remaining > USBTMC_SIZE_IOBUFFER - 12 - 3)
		{
			this_part = USBTMC_SIZE_IOBUFFER - 12 - 3;
		}
		else
		{
			this_part = remaining;
		}
		
		/* Setup IO buffer for DEV_DEP_MSG_IN message */
		usbtmc_buffer[0x00] = USBTMC_MSGID_REQUEST_DEV_DEP_MSG_IN;
		usbtmc_buffer[0x01] = p_device_data->bTag; /* Transfer ID (bTag) */
		usbtmc_buffer[0x02] = ~(p_device_data->bTag); /* Inverse of bTag */
		usbtmc_buffer[0x03] = 0; /* Reserved */
		usbtmc_buffer[0x04] = this_part & 255; /* Max transfer (first byte) */
		usbtmc_buffer[0x05] = (this_part >> 8) & 255; /* Second byte */
		usbtmc_buffer[0x06] = (this_part >> 16) & 255; /* Third byte */
		usbtmc_buffer[0x07] = (this_part >> 24) & 255; /* Fourth byte */
		usbtmc_buffer[0x08] = p_device_data->term_char_enabled * 2;
		usbtmc_buffer[0x09] = p_device_data->term_char; /* Term character */
		usbtmc_buffer[0x0a] = 0; /* Reserved */
		usbtmc_buffer[0x0b] = 0; /* Reserved */
	
		/* Create pipe and send USB request */
		pipe = usb_sndbulkpipe(p_device_data->usb_dev, p_device_data->bulk_out_endpoint);
		ret = usb_bulk_msg(p_device_data->usb_dev, pipe, usbtmc_buffer, 12, &actual,
			p_device_data->timeout);
			
		/* Store bTag (in case we need to abort) */
		p_device_data->usbtmc_last_write_bTag = p_device_data->bTag;
	
		/* Increment bTag -- and increment again if zero */
		p_device_data->bTag++;
		if (p_device_data->bTag == 0)
			p_device_data->bTag++;
		
		if (ret < 0)
		{
			PDEBUG("usb_bulk_msg() returned %d\n", ret);
			return ret;
		}
	
		/* Create pipe and send USB request */
		pipe = usb_rcvbulkpipe(p_device_data->usb_dev, p_device_data->bulk_in_endpoint);
		ret = usb_bulk_msg(p_device_data->usb_dev, pipe, usbtmc_buffer, USBTMC_SIZE_IOBUFFER, &actual,
			p_device_data->timeout);
		
		/* Store bTag (in case we need to abort) */
		p_device_data->usbtmc_last_read_bTag = p_device_data->bTag;
	
		if (ret < 0)
		{
			PDEBUG("usb_bulk_msg() returned %d\n", ret);
			return ret;
		}
	
		/* How many characters did the instrument send? */
		num_of_characters = usbtmc_buffer[4] + (usbtmc_buffer[5] << 8) + (usbtmc_buffer[6] << 16) +
			(usbtmc_buffer[7] << 24);
	
		/* Copy buffer to user space */
		if (copy_to_user(buf + done, &usbtmc_buffer[12], num_of_characters))
		{
			/* There must have been an addressing problem */
			return -OPENTMLIB_ERROR_USBTMC_MEMORY_ACCESS_ERROR;
		}
		
		done += num_of_characters;

		if (num_of_characters < this_part)
		{
			/* Short package received (less than requested amount of bytes), exit loop */
			remaining = 0;
		}

	}
	
	/* Update file position value */
	*f_pos = *f_pos + done;
	
	return done; /* Number of bytes read (total) */
	
minor_null:

	if (p_device_data->number_of_bytes == 0)
		return USBTMC_NO_ERROR;
	
	/* Copy buffer to user space */
	if (copy_to_user(buf, &usbtmc_buffer[0], p_device_data->number_of_bytes))
	{
		/* There must have been an addressing problem */
		return -OPENTMLIB_ERROR_USBTMC_MEMORY_ACCESS_ERROR;
	}

	ret = p_device_data->number_of_bytes;
	p_device_data->number_of_bytes = 0;
	return ret;

}

/* This function sends a string to an instrument by wrapping it in a USMTMC DEV_DEP_MSG_OUT message. */
ssize_t usbtmc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{

	struct usbtmc_device_data *p_device_data;
	unsigned int pipe;
	int ret, n, actual, remaining, done, this_part;
	unsigned long int num_of_bytes;
	unsigned char last_transaction;
	struct usbtmc_io_control control_message;
	
	PDEBUG("usbtmc_write() called\n");

	/* Get pointer to private data structure */
	p_device_data = filp->private_data;

	/* Verify pointer and driver state */
	if ((ret = usbtmc_verify_state(p_device_data, USBTMC_DRV_STATE_OPEN)) != USBTMC_NO_ERROR)
		return ret;

	if (MINOR(p_device_data->devno) == 0)
		goto minor_null;
	
	usbtmc_devs[0]->number_of_bytes = 0; /* In case of data left over in buffer for minor number zero */

	remaining = count;
	done = 0;
	
	while (remaining > 0) /* Still bytes to send */
	{

		if (remaining > USBTMC_SIZE_IOBUFFER - 12)
		{
			/* Use maximum size (limited by driver internal buffer size) */
			this_part = USBTMC_SIZE_IOBUFFER - 12; /* Use maximum size */
			last_transaction = 0; /* This is not the last transfer */
		}
		else
		{
			/* Can send remaining bytes in a single transaction */
			this_part = remaining;
			last_transaction = 1; /* Message ends w/ this transfer */
		}
		
		/* Setup IO buffer for DEV_DEP_MSG_OUT message */
		usbtmc_buffer[0x00] = USBTMC_MSGID_DEV_DEP_MSG_OUT;
		usbtmc_buffer[0x01] = p_device_data->bTag; /* Transfer ID (bTag) */
		usbtmc_buffer[0x02] = ~p_device_data->bTag; /* Inverse of bTag */
		usbtmc_buffer[0x03] = 0; /* Reserved */
		usbtmc_buffer[0x04] = this_part & 255; /* Transfer size (first byte) */
		usbtmc_buffer[0x05] = (this_part >> 8) & 255; /* Transfer size (second byte) */
		usbtmc_buffer[0x06] = (this_part >> 16) & 255; /* Transfer size (third byte) */
		usbtmc_buffer[0x07] = (this_part >> 24) & 255; /* Transfer size (fourth byte) */
		usbtmc_buffer[0x08] = last_transaction; /* 1 = yes, 0 = no */
		usbtmc_buffer[0x09] = 0; /* Reserved */
		usbtmc_buffer[0x0a] = 0; /* Reserved */
		usbtmc_buffer[0x0b] = 0; /* Reserved */
		
		/* Append write buffer (instrument command) to USBTMC message */
		if (copy_from_user(&usbtmc_buffer[12], buf + done, this_part))
		{
			/* There must have been an addressing problem */
			return -OPENTMLIB_ERROR_USBTMC_MEMORY_ACCESS_ERROR;
		}	
		
		/* Add zero bytes to achieve 4-byte alignment */
		num_of_bytes = 12 + this_part;
		if (this_part % 4)
		{
			num_of_bytes += 4 - this_part % 4;
			for (n = 12 + this_part; n < num_of_bytes; n++)
				usbtmc_buffer[n] = 0;
		}
	
		/* Create pipe and send USB request */
		pipe = usb_sndbulkpipe(p_device_data->usb_dev, p_device_data->bulk_out_endpoint);
		ret = usb_bulk_msg(p_device_data->usb_dev, pipe, usbtmc_buffer, num_of_bytes, &actual,
			p_device_data->timeout);
	
		/* Store bTag (in case we need to abort) */
		p_device_data->usbtmc_last_write_bTag = p_device_data->bTag;
		
		/* Increment bTag -- and increment again if zero */
		p_device_data->bTag++;
		if (p_device_data->bTag == 0)
			p_device_data->bTag++;
		
		if (ret < 0)
		{
			PDEBUG("usb_bulk_msg() returned %d\n", ret);
			return ret;
		}
		
		remaining -= this_part;
		done += this_part;

	}
	
	return count;
	
minor_null:

	usbtmc_devs[0]->number_of_bytes = 0; /* In case of data left over in buffer for minor number zero */

	/* Make sure message has the right size */
	if (count != sizeof(struct usbtmc_io_control))
	{
		return -OPENTMLIB_ERROR_USBTMC_WRONG_CONTROL_MESSAGE_SIZE;
	}

	/* Copy message to local structure */
	if (copy_from_user(&control_message, buf, sizeof(struct usbtmc_io_control)))
	{
		return -OPENTMLIB_ERROR_USBTMC_MEMORY_ACCESS_ERROR;
	}

	/* Make sure minor number given is in range */
	if (control_message.minor_number > USBTMC_MAX_DEVICES)
	{
		return -OPENTMLIB_ERROR_USBTMC_MINOR_OUT_OF_RANGE;
	}

	/* Make sure minor number given is in use */
	if (usbtmc_devs[control_message.minor_number] == NULL)
	{
		return -OPENTMLIB_ERROR_USBTMC_MINOR_NUMBER_UNUSED;
	}

	/* Dispatch message */
	if ((ret = usbtmc_dispatch_control_message(&control_message)) != USBTMC_NO_ERROR)
		return ret;

	return count;

}

/* Dispatches control message (sent to minor number zero) on behalf of usbtmc_write(). */
int usbtmc_dispatch_control_message(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	int ret;

	PDEBUG("usbtmc_dispatch_control_message() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Verify pointer and driver state */
	if ((ret = usbtmc_verify_state(p_device_data, USBTMC_DRV_STATE_OPEN)) != USBTMC_NO_ERROR)
		return ret;

	if (MINOR(p_device_data->devno) == 0)
		goto minor_null;

	switch (control_message->command)
	{

	case USBTMC_CONTROL_SET_ATTRIBUTE:
		return usbtmc_control_set_attribute(control_message);

	case USBTMC_CONTROL_GET_ATTRIBUTE:
		return usbtmc_control_get_attribute(control_message);

	case USBTMC_CONTROL_IO_OPERATION:
		return usbtmc_control_io_operation(control_message);

	default:
		return -OPENTMLIB_ERROR_USBTMC_INVALID_REQUEST;

	}

minor_null:

	switch (control_message->command)
	{

	case USBTMC_CONTROL_REPORT_INSTRUMENT:
		return usbtmc_control_report_instrument(control_message);

	default:
		return -OPENTMLIB_ERROR_USBTMC_INVALID_REQUEST;

	}

}

/* Seek entry point (random access) */
loff_t usbtmc_llseek(struct file *filp, loff_t position, int x)
{
	
	PDEBUG("usbtmc_llseek() called\n");

	return -EPERM; /* Operation not permitted */

}

/* Returns details about an instrument. */
int usbtmc_control_report_instrument(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	struct usbtmc_instrument instrument;
	struct usb_device *p_device;
	
	PDEBUG("usbtmc_control_report_instrument() called\n");

	/* Check argument (minor number) */
	if ((control_message->argument == 0) || (control_message->argument > USBTMC_MAX_DEVICES))
		return -OPENTMLIB_ERROR_USBTMC_MINOR_OUT_OF_RANGE;

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->argument];
	
	/* Make sure this device exists */
	if (p_device_data == NULL)
		return -OPENTMLIB_ERROR_USBTMC_MINOR_NUMBER_UNUSED;

	/* Fill structure */
	p_device = interface_to_usbdev(p_device_data->intf);
	instrument.minor_number = control_message->minor_number;
	strncpy(instrument.product, p_device->product, USBTMC_LONG_STR_LEN);
	instrument.product[USBTMC_LONG_STR_LEN - 1] = 0; /* Make sure there is an EOS character */
	strncpy(instrument.manufacturer, p_device->manufacturer, USBTMC_LONG_STR_LEN);
	instrument.manufacturer[USBTMC_LONG_STR_LEN - 1] = 0; /* Make sure there is an EOS character */
	strncpy(instrument.serial_number, p_device->serial, USBTMC_LONG_STR_LEN);
	instrument.serial_number[USBTMC_LONG_STR_LEN - 1] = 0; /* Make sure there is an EOS character */
	instrument.manufacturer_code = p_device->descriptor.idVendor;
	instrument.product_code = p_device->descriptor.idProduct;

	/* Write data to I/O buffer to be sent during upcoming read */
	memcpy(usbtmc_buffer, &instrument, sizeof(struct usbtmc_instrument));
	usbtmc_devs[0]->number_of_bytes = sizeof(struct usbtmc_instrument);

	return USBTMC_NO_ERROR;

}

/* This function is used to do special I/O operations, such as ABORT_BULK_IN. */
int usbtmc_control_io_operation(struct usbtmc_io_control *control_message)
{

	PDEBUG("usbtmc_control_io_operation() called\n");

	switch (control_message->argument)
	{

	case OPENTMLIB_OPERATION_INDICATOR_PULSE:
		return usbtmc_indicator_pulse(control_message);

	case OPENTMLIB_OPERATION_USBTMC_ABORT_WRITE:
		return usbtmc_abort_bulk_out(control_message);

	case OPENTMLIB_OPERATION_USBTMC_ABORT_READ:
		return usbtmc_abort_bulk_in(control_message);

	case OPENTMLIB_OPERATION_USBTMC_CLEAR_OUT_HALT:
		return usbtmc_clear_out_halt(control_message);

	case OPENTMLIB_OPERATION_USBTMC_CLEAR_IN_HALT:
		return usbtmc_clear_in_halt(control_message);

	case OPENTMLIB_OPERATION_USBTMC_RESET:
		return usbtmc_reset_conf(control_message);

	case OPENTMLIB_OPERATION_CLEAR:
		return usbtmc_clear(control_message);

	case OPENTMLIB_OPERATION_TRIGGER:
		return usbtmc_trigger(control_message);

	case OPENTMLIB_OPERATION_USBTMC_REN_CONTROL:
		return usbtmc_ren_control(control_message);

	case OPENTMLIB_OPERATION_USBTMC_GO_TO_LOCAL:
		return usbtmc_go_to_local(control_message);

	case OPENTMLIB_OPERATION_USBTMC_LOCAL_LOCKOUT:
		return usbtmc_local_lockout(control_message);

	default:
		return -OPENTMLIB_ERROR_USBTMC_INVALID_OP_CODE;

	}

}

/* Triggers the device. */
int usbtmc_trigger(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	unsigned int pipe;
	int ret;
	int actual;

	PDEBUG("usbtmc_trigger() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Setup IO buffer for TRIGGER message */
	usbtmc_buffer[0x00] = USBTMC_MSGID_TRIGGER;
	usbtmc_buffer[0x01] = p_device_data->bTag; /* Transfer ID (bTag) */
	usbtmc_buffer[0x02] = ~(p_device_data->bTag); /* Inverse of bTag */
	usbtmc_buffer[0x03] = 0; /* Reserved */
	usbtmc_buffer[0x04] = 0; /* Reserved */
	usbtmc_buffer[0x05] = 0; /* Reserved */
	usbtmc_buffer[0x06] = 0; /* Reserved */
	usbtmc_buffer[0x07] = 0; /* Reserved */
	usbtmc_buffer[0x08] = 0; /* Reserved */
	usbtmc_buffer[0x09] = 0; /* Reserved */
	usbtmc_buffer[0x0a] = 0; /* Reserved */
	usbtmc_buffer[0x0b] = 0; /* Reserved */
	
	/* Create pipe and send USB request */
	pipe = usb_sndbulkpipe(p_device_data->usb_dev, p_device_data->bulk_out_endpoint);
	ret = usb_bulk_msg(p_device_data->usb_dev, pipe, usbtmc_buffer, 12, &actual, p_device_data->timeout);

	/* Store bTag (in case we need to abort) */
	p_device_data->usbtmc_last_write_bTag = p_device_data->bTag;
	
	/* Increment bTag -- and increment again if zero */
	p_device_data->bTag++;
	if(p_device_data->bTag == 0)
		p_device_data->bTag++;
		
	if (ret < 0)
	{
		PDEBUG("usb_bulk_msg() returned %d\n", ret);
		return ret;
	}

	return USBTMC_NO_ERROR;

}

/* Reads the device's status byte */
int usbtmc_get_stb(struct usbtmc_io_control *control_message, unsigned int *value)
{

	struct usbtmc_device_data *p_device_data;
	unsigned int pipe;
	int ret;

	PDEBUG("usbtmc_get_stb() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];
	
	if (p_device_data->bTag < 2)
		p_device_data->bTag = 2;

	if (p_device_data->bTag > 127)
		p_device_data->bTag = 2;

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_READ_STATUS_BYTE,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, p_device_data->bTag,	0, usbtmc_buffer,
		3, p_device_data->timeout);
	
	/* Increment bTag -- and increment again if zero */
	p_device_data->bTag++;
	if (p_device_data->bTag == 0)
		p_device_data->bTag++;
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg() returned %d\n", ret);
		return ret;
	}
			
	if (usbtmc_buffer[0] != USBTMC_STATUS_SUCCESS)
	{
		PDEBUG("READ_STATUS_BYTE returned %x\n", usbtmc_buffer[0]);
		return -OPENTMLIB_ERROR_USBTMC_STATUS_UNSUCCESSFUL;
	}
	
	*value = usbtmc_buffer[3];
	
	if (p_device_data->interrupt_in_endpoint != 0)
	{
		/* Device has an interrupt in endpoint, so the STB value is delivered through it */
		return -OPENTMLIB_ERROR_USBTMC_FEATURE_NOT_SUPPORTED;
		/* TODO: Add support for int in endpoint */
	}

	return USBTMC_NO_ERROR;

}

/* Abort the last bulk in transfer and restore synchronization.
 * See section 4.2.1.4 of the USBTMC specifcation for details. */
int usbtmc_abort_bulk_in(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	int ret, n, actual, max_size;
	struct usb_host_interface *current_setting;
	unsigned int pipe;

	PDEBUG("usbtmc_abort_bulk_in() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev,0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_INITIATE_ABORT_BULK_IN,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT, p_device_data->usbtmc_last_read_bTag,
		p_device_data->bulk_in_endpoint, usbtmc_buffer, 2, p_device_data->timeout);
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}
	
	if (usbtmc_buffer[0] == USBTMC_STATUS_FAILED)
	{
		return -OPENTMLIB_ERROR_USBTMC_NO_TRANSFER;
	}
		
	if (usbtmc_buffer[0] != USBTMC_STATUS_SUCCESS)
	{
		return -OPENTMLIB_ERROR_USBTMC_NO_TRANSFER_IN_PROGRESS;
	}
			
	/* Get wMaxPacketSize */
	max_size = 0;
	current_setting = p_device_data->intf->cur_altsetting;
	for (n = 0; n < current_setting->desc.bNumEndpoints; n++)
		if (current_setting->endpoint[n].desc.bEndpointAddress == p_device_data->bulk_in_endpoint)
			max_size = current_setting->endpoint[n].desc.wMaxPacketSize;
	if (max_size == 0)
	{
		return -OPENTMLIB_ERROR_USBTMC_UNABLE_TO_GET_WMAXPACKETSIZE;
	}
	PDEBUG("wMaxPacketSize is %d\n", max_size);
			
	n = 0;
			
	do
	{

		/* Read a chunk of data from bulk in endpoint */
		pipe = usb_rcvbulkpipe(p_device_data->usb_dev, p_device_data->bulk_in_endpoint);
		ret = usb_bulk_msg(p_device_data->usb_dev, pipe, usbtmc_buffer, USBTMC_SIZE_IOBUFFER, &actual,
			p_device_data->timeout);
				
		n++;
				
		if (ret < 0)
		{
			PDEBUG("usb_bulk_msg returned %d\n", ret);
			return ret;
		}

	}
	while ((actual == max_size) && (n < USBTMC_MAX_READS_TO_CLEAR_BULK_IN));
			
	if (actual == max_size)
	{
		return -OPENTMLIB_ERROR_USBTMC_UNABLE_TO_CLEAR_BULK_IN;
	}
			
	n = 0;
			
usbtmc_abort_bulk_in_status:

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_CHECK_ABORT_BULK_IN_STATUS,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT, 0, p_device_data->bulk_in_endpoint,
		usbtmc_buffer, 0x08, p_device_data->timeout);
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}
			
	if (usbtmc_buffer[0] == USBTMC_STATUS_SUCCESS)
		return USBTMC_NO_ERROR;
			
	if (usbtmc_buffer[0] != USBTMC_STATUS_PENDING)
	{
		PDEBUG("INITIATE_ABORT_BULK_IN returned %x\n", usbtmc_buffer[0]);
		return -OPENTMLIB_ERROR_USBTMC_UNEXPECTED_STATUS;
	}
	
	/* Is there data to read off the device? */
	if (usbtmc_buffer[1] == 1)
		do
		{

			/* Read a chunk of data from bulk in endpoint */
			pipe = usb_rcvbulkpipe(p_device_data->usb_dev, p_device_data->bulk_in_endpoint);
			ret = usb_bulk_msg(p_device_data->usb_dev, pipe, usbtmc_buffer, USBTMC_SIZE_IOBUFFER,
				&actual, p_device_data->timeout);

			n++;

			if (ret < 0)
			{
				PDEBUG("usb_bulk_msg returned %d\n", ret);
				return ret;
			}

		}
		while ((actual == max_size) && (n < USBTMC_MAX_READS_TO_CLEAR_BULK_IN));
				
	if (actual == max_size)
	{
		return -OPENTMLIB_ERROR_USBTMC_UNABLE_TO_CLEAR_BULK_IN;
	}

	/* Device should be clear at this point. Now check status again! */
	goto usbtmc_abort_bulk_in_status;

}

/* Abort the last bulk out transfer and restore synchronization.
 * See section 4.2.1.2 of the USBTMC specifcation for details. */
int usbtmc_abort_bulk_out(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	int ret, n;
	unsigned int pipe;

	PDEBUG("usbtmc_abort_bulk_out() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_INITIATE_ABORT_BULK_OUT,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT, p_device_data->usbtmc_last_write_bTag,
		p_device_data->bulk_out_endpoint, usbtmc_buffer, 2, p_device_data->timeout);
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg() returned %d\n", ret);
		return ret;
	}



	if (usbtmc_buffer[0] == USBTMC_STATUS_FAILED)
	{
		return -OPENTMLIB_ERROR_USBTMC_NO_TRANSFER;
	}
		
	if (usbtmc_buffer[0] != USBTMC_STATUS_SUCCESS)
	{
		return -OPENTMLIB_ERROR_USBTMC_NO_TRANSFER_IN_PROGRESS;
	}
			
	n = 0;
			
usbtmc_abort_bulk_out_check_status:

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev,0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_CHECK_ABORT_BULK_OUT_STATUS,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_ENDPOINT, 0, p_device_data->bulk_out_endpoint,
		usbtmc_buffer, 0x08, p_device_data->timeout);
			
	n++;
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}
			
	if (usbtmc_buffer[0] == USBTMC_STATUS_SUCCESS)
		goto usbtmc_abort_bulk_out_clear_halt;
		
	if ((usbtmc_buffer[0] == USBTMC_STATUS_PENDING) && (n < USBTMC_MAX_READS_TO_CLEAR_BULK_IN))
		goto usbtmc_abort_bulk_out_check_status;
			
	PDEBUG("CHECK_ABORT_BULK_OUT returned %x\n", usbtmc_buffer[0]);
	return -OPENTMLIB_ERROR_USBTMC_UNEXPECTED_STATUS;
			
usbtmc_abort_bulk_out_clear_halt:

	/* Create pipe and send USB request */
	pipe = usb_sndctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USB_REQ_CLEAR_FEATURE,
		USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT, USB_ENDPOINT_HALT,
		p_device_data->bulk_out_endpoint, usbtmc_buffer, 0, p_device_data->timeout);
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}
			
	return USBTMC_NO_ERROR;

}

/* Clear the device's input and output buffers.
 * See section 4.2.1.6 of the USBTMC specification for details. */
int usbtmc_clear(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	int ret, n, actual, max_size;
	struct usb_host_interface *current_setting;
	unsigned int pipe;

	PDEBUG("usbtmc_clear() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_INITIATE_CLEAR,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 0, usbtmc_buffer, 1,
		p_device_data->timeout);
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg() returned %d\n", ret);
		return ret;
	}
			
	if (usbtmc_buffer[0] != USBTMC_STATUS_SUCCESS)
	{
		PDEBUG("INITIATE_CLEAR returned %x\n", usbtmc_buffer[0]);
		return -OPENTMLIB_ERROR_USBTMC_STATUS_UNSUCCESSFUL;
	}

	/* Get wMaxPacketSize */
	max_size = 0;
	current_setting = p_device_data->intf->cur_altsetting;
	for (n = 0; n < current_setting->desc.bNumEndpoints; n++)
		if (current_setting->endpoint[n].desc.bEndpointAddress == p_device_data->bulk_in_endpoint)
			max_size = current_setting->endpoint[n].desc.wMaxPacketSize;
	if (max_size == 0)
	{
		return -OPENTMLIB_ERROR_USBTMC_UNABLE_TO_GET_WMAXPACKETSIZE;
	}
	PDEBUG("wMaxPacketSize is %d\n", max_size);
	
	n = 0;
			
usbtmc_clear_check_status:			

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev,0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_CHECK_CLEAR_STATUS,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 0, usbtmc_buffer, 2,
		p_device_data->timeout);
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}
			
	if (usbtmc_buffer[0] == USBTMC_STATUS_SUCCESS)
	{
		/* Done. No data to read off the device. */
		goto usbtmc_clear_bulk_out_halt;
	}
			
	if (usbtmc_buffer[0] != USBTMC_STATUS_PENDING)
	{
		PDEBUG("CHECK_CLEAR_STATUS returned %x\n", usbtmc_buffer[0]);
		return -OPENTMLIB_ERROR_USBTMC_UNEXPECTED_STATUS;
	}
	
	/* Check bmClear field to see if data needs to be read off the device */
			
	if (usbtmc_buffer[1] == 1)
		do
		{

			/* Read a chunk of data from bulk in endpoint */

			PDEBUG("Reading from bulk in EP\n");

			/* Create pipe and send USB request */
			pipe = usb_rcvbulkpipe(p_device_data->usb_dev, p_device_data->bulk_in_endpoint);
			ret = usb_bulk_msg(p_device_data->usb_dev, pipe, usbtmc_buffer, USBTMC_SIZE_IOBUFFER,
				&actual, p_device_data->timeout);

			n++;

			if (ret < 0)
			{
				PDEBUG("usb_control_msg returned %d\n", ret);
				return ret;
			}

		}
		while ((actual == max_size) && (n < USBTMC_MAX_READS_TO_CLEAR_BULK_IN));
		
	if (actual == max_size)
	{
		PDEBUG("Couldn't clear device buffer within %d cycles\n", USBTMC_MAX_READS_TO_CLEAR_BULK_IN);
		return -OPENTMLIB_ERROR_USBTMC_UNABLE_TO_CLEAR_BULK_IN;
	}
			
	/* Device should be clear at this point. Now check status again! */
	goto usbtmc_clear_check_status;

usbtmc_clear_bulk_out_halt:	
			
	/* Finally, clear bulk out halt */

	/* Create pipe and send USB request */
	pipe = usb_sndctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USB_REQ_CLEAR_FEATURE,
		USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT, USB_ENDPOINT_HALT,
		p_device_data->bulk_out_endpoint, usbtmc_buffer, 0, p_device_data->timeout);
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}
			
	return 0;

}

/* Set driver attribute. */
int usbtmc_control_set_attribute(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	
	PDEBUG("usbtmc_control_set_attribute() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];
	
	switch (control_message->argument)
	{

	case OPENTMLIB_ATTRIBUTE_TIMEOUT:
		p_device_data->timeout = control_message->value * HZ; // Timeout value in jiffies
		break;

	case OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE:
		if ((control_message->value != 0) && (control_message->value != 1))
			return -OPENTMLIB_ERROR_USBTMC_INVALID_ATTRIBUTE_VALUE;
		p_device_data->term_char_enabled = control_message->value;
		break;

	case OPENTMLIB_ATTRIBUTE_TERM_CHARACTER:
		if ((control_message->value < 0) || (control_message->value > 255))
			return -OPENTMLIB_ERROR_USBTMC_INVALID_ATTRIBUTE_VALUE;
		p_device_data->term_char = control_message->value;
		break;
				
	default:
		/* Bad attribute or read-only */
		return -OPENTMLIB_ERROR_USBTMC_INVALID_ATTRIBUTE_CODE;

	}
		
	return USBTMC_NO_ERROR;

}

/* Read driver or instrument attribute. */
int usbtmc_control_get_attribute(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	unsigned int value;
	struct usbtmc_dev_capabilities caps;
	
	PDEBUG("usbtmc_control_get_attribute() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];
	
	switch (control_message->argument)
	{

	case OPENTMLIB_ATTRIBUTE_TIMEOUT:
		value = p_device_data->timeout / HZ; // Back to s from jiffies
		break;

	case OPENTMLIB_ATTRIBUTE_TERM_CHAR_ENABLE:
		value = p_device_data->term_char_enabled;
		break;
		
	case OPENTMLIB_ATTRIBUTE_TERM_CHARACTER:
		value = p_device_data->term_char;
		break;
		
	case OPENTMLIB_ATTRIBUTE_USBTMC_INTERFACE_CAPS:
		usbtmc_get_capabilities(control_message, &caps);
		value = caps.interface_capabilities;
		break;

	case OPENTMLIB_ATTRIBUTE_USBTMC_DEVICE_CAPS:
		usbtmc_get_capabilities(control_message, &caps);
		value = caps.device_capabilities;
		break;

	case OPENTMLIB_ATTRIBUTE_USBTMC_488_INTERFACE_CAPS:
		usbtmc_get_capabilities(control_message, &caps);
		value = caps.usb488_interface_capabilities;
		break;

	case OPENTMLIB_ATTRIBUTE_USBTMC_488_DEVICE_CAPS:
		usbtmc_get_capabilities(control_message, &caps);
		value = caps.usb488_device_capabilities;
		break;

	case OPENTMLIB_ATTRIBUTE_STATUS_BYTE:
		usbtmc_get_stb(control_message, &value);
		value = 0;
		break;
		
	default:
		return -OPENTMLIB_ERROR_USBTMC_INVALID_ATTRIBUTE_CODE;
		
	}
	
	/* Write data to I/O buffer to be sent during upcoming read */
	memcpy(&usbtmc_buffer[0], &value, sizeof(unsigned int));
	p_device_data->number_of_bytes = sizeof(unsigned int);
	
	return USBTMC_NO_ERROR;

}

/* Send CLEAR_FEATURE request to clear bulk out endpoint halt. */
int usbtmc_clear_out_halt(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	int ret;
	unsigned int pipe;

	PDEBUG("usbtmc_clear_out_halt() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Create pipe and send USB request */
	pipe = usb_sndctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USB_REQ_CLEAR_FEATURE,
		USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT, USB_ENDPOINT_HALT,
		p_device_data->bulk_out_endpoint, usbtmc_buffer, 0, p_device_data->timeout);
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}

	return USBTMC_NO_ERROR;

}

/* Send CLEAR_FEATURE request to clear bulk in endpoint halt. */
int usbtmc_clear_in_halt(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	int ret;
	unsigned int pipe;
	
	PDEBUG("usbtmc_clear_in_halt() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Create pipe and send USB request */
	pipe = usb_sndctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USB_REQ_CLEAR_FEATURE,
		USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT, USB_ENDPOINT_HALT,
		p_device_data->bulk_in_endpoint, usbtmc_buffer, 0, p_device_data->timeout);
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}
	
	return 0;

}

/* Returns information about the device's optional capabilities.
 * See section 4.2.1.8 of the USBTMC specifcation for details. */
int usbtmc_get_capabilities(struct usbtmc_io_control *control_message, struct usbtmc_dev_capabilities *caps)
{

	struct usbtmc_device_data *p_device_data;
	int ret;
	unsigned int pipe;
	
	PDEBUG("usbtmc_get_capabilities() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_GET_CAPABILITIES,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 0, usbtmc_buffer, 0x18,
		p_device_data->timeout);
			
	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}
			
	PDEBUG("GET_CAPABILITIES returned %x\n", usbtmc_buffer[0]);
	PDEBUG("Interface capabilities are %x\n", usbtmc_buffer[4]);
	PDEBUG("Device capabilities are %x\n", usbtmc_buffer[5]);
	PDEBUG("USB488 interface capabilities are %x\n", usbtmc_buffer[14]);
	PDEBUG("USB488 device capabilities are %x\n", usbtmc_buffer[15]);
		
	if (usbtmc_buffer[0] != USBTMC_STATUS_SUCCESS)
	{
		PDEBUG("GET_CAPABILITIES returned %x\n", usbtmc_buffer[0]);
		return -OPENTMLIB_ERROR_USBTMC_STATUS_UNSUCCESSFUL;
	}

	caps->interface_capabilities = usbtmc_buffer[4];
	caps->device_capabilities = usbtmc_buffer[5];
	caps->usb488_interface_capabilities = usbtmc_buffer[14];
	caps->usb488_device_capabilities = usbtmc_buffer[15];

	return USBTMC_NO_ERROR;

}

/* Turns on the device's activity indicator for identification. */
int usbtmc_indicator_pulse(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	int ret;
	unsigned int pipe;
	
	PDEBUG("usbtmc_indicator_pulse() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_INDICATOR_PULSE,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 0, usbtmc_buffer, 0x01,
		p_device_data->timeout);
		
	if (ret < 0)
	{
		PDEBUG("usb_control_msg() returned %d\n", ret);
		return ret;
	}

	if (usbtmc_buffer[0] != USBTMC_STATUS_SUCCESS)
	{
		PDEBUG("INDICATOR_PULSE returned %x\n", usbtmc_buffer[0]);
		return -OPENTMLIB_ERROR_USBTMC_STATUS_UNSUCCESSFUL;
	}
			
	return USBTMC_NO_ERROR;

}

/* Sets remote enable state (REN or -REN). */
int usbtmc_ren_control(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	int ret;
	unsigned int pipe;

	PDEBUG("usbtmc_ren_control() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Make sure value is in range */
	if (control_message->value > 1)
	{
		return -OPENTMLIB_ERROR_USBTMC_INVALID_PARAMETER;
	}

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_REN_CONTROL,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, control_message->value, 0, usbtmc_buffer, 0x01,
		p_device_data->timeout);

	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}

	if (usbtmc_buffer[0] != USBTMC_STATUS_SUCCESS)
	{
		PDEBUG("REN_CONTROL returned %x\n", usbtmc_buffer[0]);
		return -OPENTMLIB_ERROR_USBTMC_STATUS_UNSUCCESSFUL;
	}

	return USBTMC_NO_ERROR;

}

/* Sends GO_TO_LOCAL. See USBTMC USB488 Subclass Specification pg. 14. */
int usbtmc_go_to_local(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	int ret;
	unsigned int pipe;

	PDEBUG("usbtmc_go_to_local() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_GO_TO_LOCAL,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 0, usbtmc_buffer, 0x01,
		p_device_data->timeout);

	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}

	if (usbtmc_buffer[0] != USBTMC_STATUS_SUCCESS)
	{
		PDEBUG("GO_TO_LOCAL returned %x\n", usbtmc_buffer[0]);
		return -OPENTMLIB_ERROR_USBTMC_STATUS_UNSUCCESSFUL;
	}

	return USBTMC_NO_ERROR;

}

/* Sends LOCAL_LOCKOUT. See USBTMC USB488 Subclass Specification pg. 14. */
int usbtmc_local_lockout(struct usbtmc_io_control *control_message)
{

	struct usbtmc_device_data *p_device_data;
	int ret;
	unsigned int pipe;

	PDEBUG("usbtmc_local_lockout() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Create pipe and send USB request */
	pipe = usb_rcvctrlpipe(p_device_data->usb_dev, 0);
	ret = usb_control_msg(p_device_data->usb_dev, pipe, USBTMC_BREQUEST_LOCAL_LOCKOUT,
		USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0, 0, usbtmc_buffer, 0x01,
		p_device_data->timeout);

	if (ret < 0)
	{
		PDEBUG("usb_control_msg returned %d\n", ret);
		return ret;
	}

	if (usbtmc_buffer[0] != USBTMC_STATUS_SUCCESS)
	{
		PDEBUG("LOCAL_LOCKOUT returned %x\n", usbtmc_buffer[0]);
		return -OPENTMLIB_ERROR_USBTMC_STATUS_UNSUCCESSFUL;
	}

	return USBTMC_NO_ERROR;

}

/* Reinitialize current USB configuration and its interfaces. */
int usbtmc_reset_conf(struct usbtmc_io_control *control_message)
{
	
	struct usbtmc_device_data *p_device_data;
	int ret;
	
	PDEBUG("usbtmc_reset_conf() called\n");

	/* Get pointer to private data structure */
	p_device_data = usbtmc_devs[control_message->minor_number];

	/* Reset configuration */
	ret = usb_reset_configuration(p_device_data->usb_dev);
	
	if (ret < 0)
	{
		PDEBUG("usb_reset_configuration returned %d\n", ret);
		return ret;
	}
	
	return USBTMC_NO_ERROR;

}

/* The probe function is called whenever a device is connected which is serviced by this driver. */
static int usbtmc_probe(struct usb_interface *intf, const struct usb_device_id *id)
{

	int ret, n;
	struct usbtmc_device_data *p_device_data;
	struct usb_device *p_device;

	PDEBUG("usbtmc_probe() called\n");
	
	/* Allocate memory for device specific data */
	if (!(p_device_data = kmalloc(sizeof(struct usbtmc_device_data), GFP_KERNEL)))
	{
		PDEBUG("Unable to allocate kernel memory\n");
		goto exit_kmalloc;
	}
	
	/* Find the first free minor number */
	n = 1;
	while ((n < USBTMC_MAX_DEVICES) && (usbtmc_devs[n] != NULL))
		n++;
	if (n == USBTMC_MAX_DEVICES)
	{
		PDEBUG("No free minor number found\n");
		ret = -ENOMEM;
		goto exit_cdev_add;
	}
	
	/* Minor number is now in use */
	usbtmc_devs[n] = p_device_data;
	PDEBUG("Using minor number %d\n",n);
	
	/* Initialize cdev structure for this character device */
	memset(&p_device_data->cdev, 0, sizeof(struct cdev));
	cdev_init(&p_device_data->cdev, &fops);
	p_device_data->cdev.owner = THIS_MODULE;
	p_device_data->cdev.ops = &fops;
	
	/* Identify instrument */
	p_device = interface_to_usbdev(intf);
	PDEBUG("New device:\n");
	PDEBUG("Product: %s\n", p_device->product);
	PDEBUG("Manufacturer: %s\n", p_device->manufacturer);
	PDEBUG("Serial number: %s\n", p_device->serial);
	PDEBUG("Manufacturer code: %hx\n", p_device->descriptor.idVendor);
	PDEBUG("Product code: %hx\n", p_device->descriptor.idProduct);
	
	/* Combine major and minor numbers */
	p_device_data->devno = MKDEV(MAJOR(dev), n);
		
	/* Add character device to kernel list */
	if ((ret = cdev_add(&p_device_data->cdev, p_device_data->devno, 1)))
	{
		PDEBUG("Unable to add character device\n");
		goto exit_cdev_add;
	}

	/* Store info about USB interface in private data structure */
	p_device_data->intf = intf;
	p_device_data->id = id;

	/* Store pointer to usb device */
	p_device_data->usb_dev = usb_get_dev(interface_to_usbdev(intf));

	/* Associate pointer to private data with this interface */
	usb_set_intfdata(intf, p_device_data);

	/* Initialize bTag and other fields */
	p_device_data->bTag = 1;
	p_device_data->timeout = USBTMC_DEFAULT_TIMEOUT;
	p_device_data->term_char_enabled = 0;
	p_device_data->term_char = '\n';
	p_device_data->driver_state = USBTMC_DRV_STATE_CLOSED;
	return 0;

exit_cdev_add:

	/* Free memory for device specific data */
	kfree(p_device_data);
	return ret;
	
exit_kmalloc:
	return -ENOMEM;

}

/* The disconnect function is called whenever a device serviced by the driver is disconnected. */
static void usbtmc_disconnect(struct usb_interface *intf)
{

	struct usbtmc_device_data *p_device_data;

	PDEBUG("usbtmc_disconnect() called\n");

	/* Get pointer to private data */
	p_device_data = usb_get_intfdata(intf);

	/* Remove character device from kernel list */
	cdev_del(&p_device_data->cdev);
	
	/* Decrease use count */
	usb_get_dev(p_device_data->usb_dev);
	
	/* Update array for minor number usage */
	usbtmc_devs[MINOR(p_device_data->devno)] = NULL;

	/* Free memory allocated for private data */
	kfree(p_device_data);

	return;

}

/* This function initializes and registers the driver when being inserted into the kernel. */
static int usbtmc_init(void)
{

	int ret, n, devno;
		
	PDEBUG("usbtmc_init() called\n");
	
	/* Initialize usbtmc_devs array */
	for (n = 0; n < USBTMC_MAX_DEVICES; n++)
		usbtmc_devs[n] = NULL;
	
	/* Dynamically allocate char driver major/minor numbers */
	if ((ret = alloc_chrdev_region(&dev, 0, USBTMC_MAX_DEVICES, "USBTMCCHR")))
	{
		PDEBUG("Unable to allocate major/minor numbers\n");
		goto exit_alloc_chrdev_region;
	}

	PDEBUG("Using major number %d\n", MAJOR(dev));
	
	/* Allocate I/O buffer */
	if (!(usbtmc_buffer = kmalloc(USBTMC_SIZE_IOBUFFER, GFP_KERNEL)))
	{
		PDEBUG("Unable to allocate kernel memory\n");
		ret = -ENOMEM;
		goto exit_kmalloc;
	}
	
	/* Allocate private data structure for minor number 0 */
	if (!(usbtmc_devs[0] = kmalloc(sizeof(struct usbtmc_device_data), GFP_KERNEL)))
	{
		PDEBUG("Unable to allocate kernel memory\n");
		ret = -ENOMEM;
		goto exit_kmalloc_2;
	}
	
	/* Initialize relevant fields in private data structure */
	usbtmc_devs[0]->driver_state = USBTMC_DRV_STATE_CLOSED;
	
	/* Initialize cdev structure for minor number 0. */
	memset(&usbtmc_devs[0]->cdev, 0, sizeof(struct cdev));
	cdev_init(&usbtmc_devs[0]->cdev, &fops);
	usbtmc_devs[0]->cdev.owner = THIS_MODULE;
	usbtmc_devs[0]->cdev.ops = &fops;
		
	/* Add character device to kernel list */
	devno = MKDEV(MAJOR(dev), 0);
	if ((ret = cdev_add(&usbtmc_devs[0]->cdev, devno, 1)))
	{
		PDEBUG("Unable to add character device\n");
		goto exit_cdev_add;
	}

	PDEBUG("Registering USB driver\n");

	/* Register USB driver with USB core */
	ret = usb_register(&usbtmc_driver);
	if (ret)
	{
		PDEBUG("Unable to register driver\n");
		goto exit_usb_register;
	}
	
	usbtmc_devs[0]->devno = devno;

	return 0; /* So far so good */
	
exit_usb_register:
	/* Remove character device driver from kernel list */
	cdev_del(&usbtmc_devs[0]->cdev);
	
exit_cdev_add:
	/* Free private data area for minor number 0 */
	kfree(usbtmc_devs[0]);
	
exit_kmalloc_2:
	/* Free driver buffer */
	kfree(usbtmc_buffer);
	
exit_kmalloc:
	/* Unregister char driver major/minor numbers */
	unregister_chrdev_region(dev, USBTMC_MAX_DEVICES);
	
exit_alloc_chrdev_region:
	return ret;

}

/* The exit function is called before the driver is unloaded from the kernel. */
static void usbtmc_exit(void)
{

	PDEBUG("usbtmc_exit() called\n");
	
	/* Unregister char driver major/minor numbers */
	unregister_chrdev_region(dev, USBTMC_MAX_DEVICES);
	
	/* Release IO buffer allocated in usbtmc_init */
	if (usbtmc_buffer != NULL)
		kfree(usbtmc_buffer);
	
	/* Free memory for device-specific data */
	if (usbtmc_devs[0] != NULL)
		kfree(usbtmc_devs[0]);
	
	/* Unregister USB driver with USB core */
	usb_deregister(&usbtmc_driver);

}

module_init(usbtmc_init);
module_exit(usbtmc_exit);

MODULE_LICENSE("GPL");
