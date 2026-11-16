# Switch the ISP to PCI mode (needed by Linux)

The ThinkPad 10's Phoenix BIOS can expose the Atom ISP either as part of the
graphics device ("ACPI mode", `ISPDevSel=1`, Windows' default) or as its own
PCI device 00:03.0 (`ISPDevSel=2`), which Linux `atomisp` needs. The BIOS
setup screen hides this option, so `ispsel` changes it directly in the
firmware's `Setup` variable (offset `0x235`).

**Only for the ThinkPad 10 1st gen (20C1/20C3) with BIOS GWET50WW (1.50).**
The tool reads the variable first and refuses to write unless it has the
expected size (878 bytes) and both the ISP setting and the "ISP enabled"
byte hold known values. It changes only that one byte and reads it back.
Still: changing firmware settings is at your own risk.

## Build

```sh
sudo apt install gnu-efi
make
```

This builds `ispsel-pci.efi` (Linux) and `ispsel-acpi.efi` (restore the
Windows default).

## Run it once with BootNext

From Linux (assuming the EFI system partition is mounted at `/boot/efi`
and is partition 1 of the internal eMMC, `/dev/mmcblk1`; check with `lsblk`):

```sh
sudo mkdir -p /boot/efi/EFI/ispsel
sudo cp ispsel-pci.efi /boot/efi/EFI/ispsel/
sudo efibootmgr -C -d /dev/mmcblk1 -p 1 -L "ISP PCI mode" -l '\EFI\ispsel\ispsel-pci.efi'
sudo efibootmgr            # note the new entry's number, e.g. Boot0005
sudo efibootmgr -n 0005    # boot it once, next boot only
sudo reboot
```

The tool prints the old and new value and waits for a key. Then **power off
fully** and boot Linux. `lspci` should now show
`00:03.0 Multimedia controller: Intel Corporation Atom Processor Z36xxx/Z37xxx Series Camera ISP`.

Remove the boot entry afterwards with `sudo efibootmgr -b 0005 -B`.

To go back to the Windows default, do the same with `ispsel-acpi.efi`.
