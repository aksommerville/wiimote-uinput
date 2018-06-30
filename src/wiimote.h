#ifndef WIIMOTE_H
#define WIIMOTE_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <linux/input.h>
#include <linux/uinput.h>

// Debian package "libbluetooth-dev".
// You may also need "bluez-utils" to get the devices working.
// Update (for Pi at least, 7 April 2018): sudo apt-get install bluetooth bluez libbluetooth-dev
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>

#endif
