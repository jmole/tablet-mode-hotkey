#include <fcntl.h>
#include <libudev.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>


/*
    Replace this with the device you want to get data from (e.g. your keyboard)
    Examples:
    
    #define INPUT_DEVICE "/dev/input/event10"
    #define INPUT_DEVICE "/dev/input/by-id/usb-GENERIC_keyboard-event-kbd"
*/ 
#define INPUT_DEVICE first_child_event_device("HP WMI hotkeys")

/*
    Replace this with the key you want to use for your tablet mode switch.
    Other codes can be found in [input-event-codes.h] 
*/ 
#define NEW_TABLET_MODE_KEY KEY_PROG2

/* Looks for the first input event device that is a child of the device
   with name "device_name" and returns its device node string. */
const char* first_child_event_device(const char* device_name) {
    const char* device_path = NULL;
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev, *parent_dev;
    int found = 0;

    // Create the udev object
    udev = udev_new();
    if (!udev) {
        fprintf(stderr, "Cannot create udev context.\n");
        return NULL;
    }

    // Enumerate devices of the "input" subsystem
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);

    // Iterate over the enumerated devices to find the target device based on name property
    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *syspath, *devnode, *name;

        syspath = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, syspath);

        // Access the 'name' attribute directly from syspath
        name = udev_device_get_sysattr_value(dev, "name");
        if (name && strcmp(name, device_name) == 0) {
            // We've found our target device. Now let's find the child eventX device.
            struct udev_enumerate *enumerate_child = udev_enumerate_new(udev);
            udev_enumerate_add_match_parent(enumerate_child, dev);
            udev_enumerate_scan_devices(enumerate_child);

            struct udev_list_entry *children, *child_entry;
            children = udev_enumerate_get_list_entry(enumerate_child);

            udev_list_entry_foreach(child_entry, children) {
                const char *child_syspath = udev_list_entry_get_name(child_entry);
                struct udev_device *child_dev = udev_device_new_from_syspath(udev, child_syspath);
                devnode = udev_device_get_devnode(child_dev);

                if (devnode) {
                    printf("Found event device: %s\n", devnode);
                    found = 1;
                    device_path = strdup(devnode);
                    udev_device_unref(child_dev);
                    break;

                }

                udev_device_unref(child_dev);
            }

            udev_enumerate_unref(enumerate_child);
            udev_device_unref(dev);
            break;
        }

        udev_device_unref(dev);
    }

    if (!found) {
        printf("Event device not found.\n");
    }

    // Cleanup
    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    return device_path;
}

int main(void)
{
    int uinput_fd, event_fd, ret;
    struct uinput_user_dev uidev;
    struct input_event ev;
    int tablet_mode_state = 0;

    // Open the uinput device for writing and non-blocking mode
    uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd < 0)
    {
        perror("Failed to open /dev/uinput");
        return -1;
    }

    // Setup the virtual device
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "Fake Intel HID switches");
    uidev.id.bustype = BUS_HOST;
    uidev.id.vendor = 0x1234;
    uidev.id.product = 0x1234;
    uidev.id.version = 0x1234;

    // Enable switch events
    ioctl(uinput_fd, UI_SET_EVBIT, EV_SW);
    // Enable the tablet mode switch
    ioctl(uinput_fd, UI_SET_SWBIT, SW_TABLET_MODE);

    // Create the virtual device
    write(uinput_fd, &uidev, sizeof(uidev));
    if (ioctl(uinput_fd, UI_DEV_CREATE) < 0)
    {
        perror("Failed to create uinput device");
        close(uinput_fd);
        return -1;
    }

    // Open /dev/input/event7 for reading
    event_fd = open(INPUT_DEVICE, O_RDONLY | O_NONBLOCK);
    if (event_fd < 0)
    {
        perror("Failed to open device:");
        perror(INPUT_DEVICE);
        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
        return -1;
    }

    // Main loop to wait for events from INPUT_DEVICE
    // TODO: Add signal handler to stop gracefully
    while (1)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(event_fd, &fds);

        // Use select() to wait for an event
        ret = select(event_fd + 1, &fds, NULL, NULL, NULL);
        if (ret > 0 && FD_ISSET(event_fd, &fds))
        {
            // Read the event
            ret = read(event_fd, &ev, sizeof(struct input_event));
            if (ret == sizeof(struct input_event))
            {
                if (ev.type == EV_KEY && ev.code == KEY_PROG2 && ev.value == 1)
                {
                    // Toggle the SW_TABLET_MODE state
                    tablet_mode_state = !tablet_mode_state;

                    // Emit SW_TABLET_MODE event to the virtual device
                    memset(&ev, 0, sizeof(struct input_event));
                    ev.type = EV_SW;
                    ev.code = SW_TABLET_MODE;
                    ev.value = tablet_mode_state;
                    write(uinput_fd, &ev, sizeof(struct input_event));

                    // Always send an EV_SYN event after emitting other events
                    memset(&ev, 0, sizeof(struct input_event));
                    ev.type = EV_SYN;
                    ev.code = SYN_REPORT;
                    ev.value = 0;
                    write(uinput_fd, &ev, sizeof(struct input_event));
                }
            }
        }
    }

    // Cleanup (not reached in this example due to the infinite loop)
    close(event_fd);
    ioctl(uinput_fd, UI_DEV_DESTROY);
    close(uinput_fd);

    return 0;
}
