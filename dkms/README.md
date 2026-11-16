# atomisp-tp10 (DKMS)

Out-of-tree build of the camera drivers for the ThinkPad 10 1st gen, taken
from Kali's `linux-source-7.2` with these changes (all being submitted
upstream):

- **atomisp-ov2722**: report the Bayer order (SGRBG10) consistently, so
  colours are right.
- **atomisp csi2-bridge**: don't cycle the PMC clock when its rate is
  already right (fixes the rear camera failing to probe on some boots);
  add the IMX175 (`INTCF1A`, 4 CSI lanes).
- **atomisp**: only list frame sizes the preview pipeline can output, and
  report a frame rate for sensors without frame-interval support (needed
  by PipeWire and Firefox).
- **ipu-bridge**: add the IMX175 (`INTCF1A`).
- **imx175**: new driver for the Sony IMX175 rear sensor.
- **leds-lm3554**: new driver for the LM3554 rear flash LED, plus
  `led-class-flash` (the LED flash class), which the Kali 7.2 kernel does
  not build.

`BUILD_EXCLUSIVE_KERNEL` limits it to 7.2.x kernels. Newer kernels need a
rebuilt package (or, once merged, nothing).

## Install

```sh
sudo apt install dkms linux-headers-$(uname -r) v4l-utils
sudo sh install.sh
sudo reboot
```

`install.sh` copies the source to `/usr/src/atomisp-tp10-<version>`, builds
and installs it, and blacklists `intel_atomisp2_pm` (a power-off-only driver
which would otherwise claim the ISP). After the reboot,
`v4l2-ctl -d /dev/video0 --list-inputs` should list `ov2722` and `imx175`,
and `/sys/class/leds/white:flash` should exist.

Remove with `sudo sh install.sh --remove`.
