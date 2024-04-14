#include <fcntl.h>
#include <libudev.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>


int main(void)
{
    int uinput_fd, ret;
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

    for (int i=0; i<20; ++i)
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
        sleep(1);
    }

    // Cleanup (not reached in this example due to the infinite loop)
    ioctl(uinput_fd, UI_DEV_DESTROY);
    close(uinput_fd);

    return 0;
}
