# Lenovo ThinkPad 10 (1st gen) on Linux

Notes, drivers and tools for running Linux on the **Lenovo ThinkPad 10, 1st
generation** (machine types **20C1 / 20C3**, Intel Atom Z3795 "Bay Trail").
Tested on Kali Linux rolling with its 7.2 kernel, BIOS GWET50WW (1.50).

The main focus so far is the **cameras**: both the front (OV2722, 2 MP) and
the rear (IMX175, 8 MP) camera work, including in Firefox, with the steps
below.

> The 2nd generation ThinkPad 10 (20E3/20E4, Cherry Trail) is different
> hardware and is not covered here.

## Status

| Hardware | Status |
|---|---|
| Touchscreen, pen, buttons | Works out of the box |
| Wi-Fi, Bluetooth | Works |
| Audio (speakers, headphones) | Works (if silent, check the WirePlumber profile isn't "off") |
| Accelerometer and other sensors, battery | Works |
| Suspend (s2idle) | Works, also with the cameras loaded |
| **Front camera** (OV2722) | **Works** with this repo |
| **Rear camera** (IMX175) | **Works** with this repo (needs the new IMX175 driver, included) |
| **Rear flash / white LED** (LM3554) | **Works** with this repo (new driver, included): torch and flash |

## Camera setup

The cameras use Intel's Atom ISP (`atomisp`, in the kernel's staging area).
Getting them working takes the steps below. Steps 1 and 2 are needed once.

### 1. Switch the ISP to PCI mode in the firmware

The BIOS can expose the ISP in two ways: "ACPI mode" (Windows' default) or as
a PCI device. Linux `atomisp` needs PCI mode, which the BIOS setup screen
doesn't offer. [`bios-isp-mode/`](bios-isp-mode/) has a small, checked EFI
tool that flips that one setting, and a tool to switch back for Windows.

### 2. Install the ISP firmware

`atomisp` needs `shisp_2400b0_v21.bin`, which is part of
[linux-firmware](https://gitlab.com/kernel-firmware/linux-firmware/-/blob/main/intel/ipu/shisp_2400b0_v21.bin).
Install it as `/lib/firmware/intel/ipu/shisp_2400b0_v21.bin` if your
distribution's firmware package doesn't already contain it.

### 3. Install the camera drivers (DKMS)

[`dkms/`](dkms/) contains `atomisp-tp10`: the atomisp, OV2722 and ipu-bridge
drivers from the 7.2 kernel plus the fixes this machine needs, and the new
IMX175 driver. See [dkms/README.md](dkms/README.md).

### 4. Use a browser that handles the ISP's frames

atomisp pads each image row (e.g. 1624-pixel rows are stored 1664 bytes
apart). Browsers must handle that:

- **Firefox 160 or newer** works, through PipeWire. Firefox ESR 153 (Kali's
  default) does not; it shows streaks or never starts the camera.
  [`firefox/`](firefox/) installs current Firefox from Mozilla's own APT
  repository and enables its PipeWire camera support.
- GStreamer-based apps (e.g. Cheese, Snapshot) handle padded rows.

### 5. Rear flash LED (torch)

The DKMS package also contains a new driver for the LM3554 flash chip, which
loads by itself and shows the white LED as `/sys/class/leds/white:flash`:

```sh
echo 1 | sudo tee /sys/class/leds/white:flash/brightness   # torch on (levels 1-8)
echo 0 | sudo tee /sys/class/leds/white:flash/brightness   # off
```

Flash (`flash_brightness`, `flash_timeout`, `flash_strobe`) is available
too; camera apps can't trigger it yet.

### 6. Switch between the front and rear camera

Apps see one camera, "ATOM ISP". Which sensor it shows is chosen with
[`camera-switch/`](camera-switch/): a **Switch Camera** menu entry and
optional panel button (right-click for Front/Rear), or `tp10-camera
front|rear|toggle` in a terminal. Close apps using the camera before
switching.

On Windows each sensor appears as its own camera instead. Doing the same on
Linux would need PipeWire to create one camera per V4L2 input, which it
doesn't do yet.

## Upstream status

The kernel changes were sent upstream on 2026-11-16 and are under review;
once merged, step 3 and the 1 fps workaround won't be needed on new kernels:

| Change | Status |
|---|---|
| ov2722: Report SGRBG10 consistently (front camera colours) | [sent to linux-media](https://lore.kernel.org/linux-media/20261116173143.13584-1-volodymyrchapman@gmail.com/) |
| csi2-bridge: Don't cycle the PMC clock if its rate is correct (rear camera failing to probe on some boots) | [sent to linux-media](https://lore.kernel.org/linux-media/20261116173307.14169-1-volodymyrchapman@gmail.com/) |
| Sony IMX175 sensor driver, plus ipu-bridge and atomisp entries | [sent to linux-media](https://lore.kernel.org/linux-media/20261116175151.20150-1-volodymyrchapman@gmail.com/T/) |
| atomisp: frame sizes and frame intervals for PipeWire / Firefox | [sent to linux-media](https://lore.kernel.org/linux-media/20261116175750.21905-1-volodymyrchapman@gmail.com/T/) |
| LM3554 flash LED driver (rear white LED) | [sent to linux-leds](https://lore.kernel.org/linux-leds/20261116180809.26946-1-volodymyrchapman@gmail.com/) |


Userspace issues found along the way: Firefox ESR 153 requires unpadded
camera buffers (fixed in Firefox 160), and libwebrtc takes the *minimum* of
a PipeWire framerate range as the camera's maximum frame rate.

## How this was made

This work was done by the repository owner on their own ThinkPad 10, with
the help of an AI coding assistant (Claude) for investigation, code and
documentation. All changes were tested on the hardware.

## License

GPL-2.0-only (see [LICENSE](LICENSE)). The DKMS sources are Linux kernel
code under the same license.
