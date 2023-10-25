#include <asm-generic/errno-base.h>
#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define die(str, args...) \
  do { perror(str); exit(EXIT_FAILURE); } while (0)

#ifndef DEBUG
#define DEBUG 0
#endif
#define debug(fmt, ...) \
  do { if (DEBUG) fprintf(stderr, fmt "\n", __VA_ARGS__); } while (0)

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s /dev/input/eventX\n", argv[0]);
    return 1;
  }

  // Initialize libevdev with the controller
  // https://www.freedesktop.org/software/libevdev/doc/latest/index.html
  int fd = open(argv[1], O_RDONLY);
  struct libevdev *dev;
  if (fd < 0 || libevdev_new_from_fd(fd, &dev) < 0) {
    die("Failed to initialize libevdev");
  }

  // Create a virtual uninput device
  // https://www.kernel.org/doc/html/v4.12/input/uinput.html
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

  // Query the controller for available inputs and configure them in the virtual uinput device
  // https://www.freedesktop.org/software/libevdev/doc/latest/group__bits.html
  int types[] = { EV_KEY, EV_ABS };
  for (int i = 0; i < 2; i++ ) {
    int type = types[i];

    if (libevdev_has_event_type(dev, type)) {
      debug("Event type %d (%s)", type, libevdev_event_type_get_name(type));

      if (ioctl(uinput_fd, UI_SET_EVBIT, type) < 0) {
        die("Error setting event type in uinput device");
      }

      for (int code = 0; code <= libevdev_event_type_get_max(type); code++) {
        debug("\tEvent code %d (%s)", code, libevdev_event_code_get_name(type, code));

        if (type == EV_KEY) {
          if (ioctl(uinput_fd, UI_SET_KEYBIT, code) < 0) {
            die("Unable to configure uinput button");
          }
        } else if (type == EV_ABS) {
          if (ioctl(uinput_fd, UI_SET_ABSBIT, code) < 0) {
            die("Unable to configure uinput analog");
          }

          struct input_absinfo absinfo;
          if (ioctl(fd, EVIOCGABS(code), &absinfo) < 0) {
            die("Error fetching analog information from controller");
          }

          uidev.absmin[code] = absinfo.minimum;
          uidev.absmax[code] = absinfo.maximum;

          if (absinfo.fuzz != 0)
            uidev.absfuzz[code] = absinfo.fuzz;

          if (absinfo.flat != 0)
            uidev.absflat[code] = absinfo.flat;

          debug("\t\t(min: %d, max: %d, fuzz: %d, flat: %d)",
            uidev.absmin[code], uidev.absmax[code], uidev.absfuzz[code], uidev.absflat[code]);
        }
      }
    }
  }

  // Write the controller uninput configuration
  if (write(uinput_fd, &uidev, sizeof(uidev)) < 0)
    die("Error writing the uninput configuration");

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

    debug("Event: %d, Code: %d, Value: %d", ev.type, ev.code, ev.value);

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
