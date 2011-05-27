#!/bin/sh
#
# Copyright (C) 2008, 2011 Stefan Kopp, Gechingen, Germany
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# The GNU General Public License is available at
# http://www.gnu.org/copyleft/gpl.html.

module="usbtmc"

# Remove module from kernel (just in case it is still running)
/sbin/rmmod $module

# Install module
/sbin/insmod ./$module.ko

# Find major number used
major=$(cat /proc/devices | grep USBTMCCHR | awk '{print $1}')
echo Using major number $major

# Remove old device files
rm -f /dev/${module}[0-9]

# Ceate new device files
mknod /dev/${module}0 c $major 0
mknod /dev/${module}1 c $major 1
mknod /dev/${module}2 c $major 2
mknod /dev/${module}3 c $major 3
mknod /dev/${module}4 c $major 4
mknod /dev/${module}5 c $major 5
mknod /dev/${module}6 c $major 6
mknod /dev/${module}7 c $major 7
mknod /dev/${module}8 c $major 8
mknod /dev/${module}9 c $major 9

# Change access mode
chmod 666 /dev/${module}0
chmod 666 /dev/${module}1
chmod 666 /dev/${module}2
chmod 666 /dev/${module}3
chmod 666 /dev/${module}4
chmod 666 /dev/${module}5
chmod 666 /dev/${module}6
chmod 666 /dev/${module}7
chmod 666 /dev/${module}8
chmod 666 /dev/${module}9
