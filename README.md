# wiimote-uinput
30 June 2018

## Requirements
* <bluetooth/bluetooth.h>. Debian: apt-get install libbluetooth-dev
* BlueZ utils. Debian: apt-get install bluez
* Read/write access to /dev/uinput. (see below)

## Build and install

```shell
$ make
$ sudo cp out/wiimote /usr/local/bin/wiimote
$ sudo mkdir /etc/wiimote
$ sudo cp etc/config /etc/wiimote/config
```

Then edit `etc/wiimote/config`, fill in your preferences and your device names and addresses.

## Acquiring device addresses
Every Bluetooth device has a unique ID assigned at manufacture, just like a MAC address.
To discover this ID, run `hcitool scan` and make the device discoverable (press '1' and '2' together).
You should see something like this:

```
Scanning ...
  00:1E:35:72:07:CF  Nintendo RVL-CNT-01
```

## Normal operation
To connect to a wiimote:

```shell
$ wiimote NameOfDevice
```

And you'll see something like this:

```
wiimote:INFO: Connecting to device (PSM 0x13)...
wiimote:INFO: Connecting to device (PSM 0x11)...
wiimote:INFO: Connected
wiimote:INFO: Launched daemon process 23544. Terminating foreground.
```

At that point, it's ready to use.
To disconnect, you can kill the process (23544 in that example), or hold the wiimote's power button for a few seconds.

## Enabling access to /dev/uinput
If you want to run the driver as root, go right ahead.
Otherwise (recommended), you'll want to make /dev/uinput accessible.
I did this with a new file `/etc/udev/rules.d/99-wiimote.rules`, containing this one line:

```
ACTION=="add", KERNEL=="uinput", MODE:="0660", GROUP:="plugdev"
```

The `plugdev` group is one you should be in by default.

This will take effect at the next restart.
To make it effective without restarting:

```shell
$ sudo udevadm control --reload
$ sudo udevadm trigger
$ sudo modprobe -r uinput
$ sudo modprobe uinput
```
