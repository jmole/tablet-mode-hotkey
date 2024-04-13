# tablet-mode-switch

This program enables you to turn your device into "tablet mode" using the same HID events that the ACPI platform typically provides.

I created this because my 2024 HP Spectre 16" Laptop has a stupid bug where rotating the device 90° to the right or left activates tablet mode, which then goes on to deactivate the keyboard and touchpad. The net result of this is that if you're lying on your side in bed with the laptop on it's side (e.g. to watch a video or something), the keyboard and touchpad stop working.

The solution to this is to disable the `intel_hid` kernel module from loading, which prevents the HID sensor switch device from being created, and thus prevents any asACPI calls to determine if tablet mode is active, and also prevents the associated HID event from being sent to the input system.sociated ACPI calls to check sensor status, and propagation of said tablet mode status events.

However, this necessarily has the side effect of never being able to use tablet mode, which is apparently a problem that's apparently been plaguing 2-in-1 laptops for [years](https://gitlab.gnome.org/GNOME/mutter/-/issues/1686#note_1864387) and [years](https://gitlab.freedesktop.org/libinput/libinput/-/issues/822) and [years](https://github.com/dmitry-s93/MControlCenter/issues/77).

So I created this little program which enables you to turn any HID-type key event into a tablet mode switch. I'm currently using the "app switch" key on my HP Spectre, which doesn't appear to be doing anything else in GNOME at the moment.

## Setup

### Disabling intel_hid

Disable intel_hid if necessary: `sudo rmmod intel_hid`. This will prevent the existing sensors from accidentally triggering or untriggering tablet mode. You can also add an entry to a new file in `/etc/modprobe.d/` that prevents it from loading entirely on boot.

```sh
# /etc/modprobe.d/blacklist.intel.hid
blacklist intel_hid
```

### Finding a key to use as the replacement key

Most laptops these days use HID for every button, switch or sensor that can communicate it's state in a few bytes. That means you can probably repurpose your webcam switch, power button, or in my case, a weird function row key that I don't know the purpose for and which doesn't appear to have any other use.

To identify the key, use libinput in debug mode, and press the key that you want to use:

```sh
sudo libinput debug-events

# my output:
# -event7   KEYBOARD_KEY            +2.790s KEY_PROG2 (149) pressed
# event7    KEYBOARD_KEY            +2.790s KEY_PROG2 (149) released
```

Then, identify the device (`event7`) and key (`KEY_PROG2`) that you want to use. Edit `hotkey.c` and replace `INPUT_DEVICE` with the full name of the device, and `NEW_TABLET_MODE_KEY` with the key code you identified.

```c
// Replace this with the device you want to get data from (e.g. your keyboard)
#define INPUT_DEVICE "/dev/input/event7"
// Replace this with the key you want to use for your tablet mode switch
#define NEW_TABLET_MODE_KEY KEY_PROG2
```

### Building the service

You need GCC, linux-headers, and libevdev installed to compile this.

```sh
gcc -o tablet-mode-switch hotkey.c -levdev -I/usr/include/libevdev-1.0
```

### Installing

Copy the binary somewhere. If you use a different path than I did, you will need to modify the systemd unit file with the path you chose.

```sh
sudo cp tablet-mode-switch /usr/local/sbin
```

Next, copy over the systemd unit and enable it. It needs to happen before the window manager loads,so you'll either have to restart your computer, or start the service then log out and log in again.

```sh
sudo cp tablet-mode-switch.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable tablet-mode-switch.service
sudo systemctl install tablet-mode-switch.service
```

## Debugging

You can use libinput to debug this like any other HID device. Assuming your build was successful:

In terminal 1:

```sh
sudo ./tablet-mode-switch
```

In terminal 2:

```sh
sudo libinput debug-events
```

If it's working as expected, you should see the device in libinput, and you should see an additional event after your chosen key is pressed:

```sh
# event262 is the device the program created  
-event262  DEVICE_ADDED            Fake Intel HID switches           seat0 default group13 cap:S
-event7    DEVICE_ADDED            HP WMI hotkeys                    seat0 default group12 cap:kS

# pressing the KEY_PROG2 key switches the tablet-mode state to enabled
-event7    KEYBOARD_KEY            +5.988s     KEY_PROG2 (149) pressed
 event7    KEYBOARD_KEY            +5.988s     KEY_PROG2 (149) released
-event262  SWITCH_TOGGLE           +5.988s     switch tablet-mode state 1

# pressing the KEY_PROG2 key again switches the tablet-mode state to disabled
-event7    KEYBOARD_KEY            +12.328s    KEY_PROG2 (149) pressed
 event7    KEYBOARD_KEY            +12.328s    KEY_PROG2 (149) released
-event262  SWITCH_TOGGLE           +12.328s    switch tablet-mode state 0
```
