#include <asm-generic/errno-base.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define die(str, args...)                                                      \
  do {                                                                         \
    perror(str);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s /dev/input/eventX\n", argv[0]);
    return 1;
  }

  // Initialize libevdev
  int fd = open(argv[1], O_RDONLY);
  struct libevdev *dev;
  if (fd < 0 || libevdev_new_from_fd(fd, &dev) < 0) {
    die("Failed to initialize libevdev");
  }

  // Open uinput for creating a virtual input device
  int uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (uinput_fd < 0)
    die("Could not open uinput");

  // Configure the uinput device
  struct uinput_user_dev uidev;
  memset(&uidev, 0, sizeof(uidev));
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "Nintendo-XInput");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1;
  uidev.id.product = 0x1;
  uidev.id.version = 1;

  // Write the uinput device configuration
  if (write(uinput_fd, &uidev, sizeof(uidev)) < 0)
    die("write");

  // Create a virtual xbox controller and send it remapped inputs
  if (ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY) < 0)
      die("Error setting EV_BTN on uinput device");
  
  // Set up ABXY uinput from the Nintendo controller
  if (ioctl(uinput_fd, UI_SET_KEYBIT, BTN_EAST) < 0 ||
      ioctl(uinput_fd, UI_SET_KEYBIT, BTN_SOUTH) < 0 ||
      ioctl(uinput_fd, UI_SET_KEYBIT, BTN_NORTH) < 0 ||
      ioctl(uinput_fd, UI_SET_KEYBIT, BTN_C) < 0)
      die("Unable to configure the uinput device");

  // TODO set up the rest of the Nintedo inputs for passthrough
  
  // Create uinput device
  if (ioctl(uinput_fd, UI_DEV_CREATE) < 0)
      die("Error creating uinput device");
  
  while (1) {
    struct input_event ev;
    int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

    if (rc == -ENODEV) // Controller is disconnected
      break;

    if (rc < 0)
      continue;

    if (ev.type == EV_KEY) {
      // Remap ABXY buttons
    	switch (ev.code) {
    	    case BTN_EAST: // A
    	        ev.code = BTN_SOUTH;
    	        break;
    	    case BTN_SOUTH: // B
    	        ev.code = BTN_EAST;
    	        break;
    	    case BTN_NORTH: // X
    	        ev.code = BTN_C;
    	        break;
    	    case BTN_C: // Y
    	        ev.code = BTN_NORTH;
    	        break;
    	    default:
    	        break;
			}
    }

		// Write remapped event to uinput device
		if (write(uinput_fd, &ev, sizeof(ev)) < 0)
    		die("Error writing event to uinput device");

    // Flush the event buffer to commit the mapped key change
    struct input_event syn_ev = {.type = EV_SYN, .code = SYN_REPORT, .value = 0};
    if (write(uinput_fd, &syn_ev, sizeof(syn_ev)) < 0)
      die("Error writing SYN event to uinput device");
  }

  if (ioctl(uinput_fd, UI_DEV_DESTROY) < 0)
      die("Error destroying uinput device");

  close(fd);
  close(uinput_fd);

  return 0;
}
