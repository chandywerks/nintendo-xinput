# Nintendo to XInput Remap

Remap Nintendo controller layout to Xbox

## Supported Controllers

- 8Bitdo SN30 Pro
- Nintendo Pro Controller (TODO)

## Install

Requires `libevdev-devel`

```sh
make
# sudo make install
```

Add udev rule to trigger the remapper when a controller is added

Create `/etc/udev/rules.d/99-8bitdo.rules`

```
ACTION=="add", SUBSYSTEM=="input", ATTRS{name}=="8Bitdo SN30 Pro", RUN+="/usr/bin/nintendo-xinput"
```

Reload the udev rules

```
sudo udevadm control --reload-rules && sudo udevadm trigger
```
