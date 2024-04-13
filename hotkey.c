#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

// Replace this with the device you want to get data from (e.g. your keyboard)
#define INPUT_DEVICE "/dev/input/event7"
// Replace this with the key you want to use for your tablet mode switch
#define NEW_TABLET_MODE_KEY KEY_PROG2

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
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x0;
    uidev.id.product = 0x0;
    uidev.id.version = 0x0;

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
        perror("Failed to open " INPUT_DEVICE);
        ioctl(uinput_fd, UI_DEV_DESTROY);
        close(uinput_fd);
        return -1;
    }

    // Main loop to wait for KEY_PROG2 events from INPUT_DEVICE
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
