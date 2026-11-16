# SPDX-License-Identifier: GPL-2.0-only
# Build ispsel-pci.efi (Linux, ISP as PCI device 00:03.0) and
# ispsel-acpi.efi (Windows default, restore). Needs gnu-efi:
#   sudo apt install gnu-efi     (or set GNUEFI to an extracted gnu-efi .deb)
GNUEFI ?=
INC := $(GNUEFI)/usr/include/efi
LIB := $(dir $(firstword $(wildcard $(GNUEFI)/usr/lib/elf_x86_64_efi.lds $(GNUEFI)/usr/lib/*/elf_x86_64_efi.lds)))
CFLAGS := -I$(INC) -I$(INC)/x86_64 -DEFI_FUNCTION_WRAPPER -fpic -ffreestanding \
	-fno-stack-protector -fno-strict-aliasing -fshort-wchar -mno-red-zone \
	-maccumulate-outgoing-args -Wall
SECTIONS := -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym \
	-j .rel -j .rela -j .reloc

all: ispsel-pci.efi ispsel-acpi.efi

ispsel-pci.o: ispsel.c
	$(CC) $(CFLAGS) -DTARGET=2 -c $< -o $@
ispsel-acpi.o: ispsel.c
	$(CC) $(CFLAGS) -DTARGET=1 -c $< -o $@

%.so: %.o
	$(LD) -shared -Bsymbolic -L$(LIB) -T$(LIB)elf_x86_64_efi.lds \
		$(LIB)crt0-efi-x86_64.o $< -o $@ -lefi -lgnuefi

%.efi: %.so
	objcopy $(SECTIONS) --target efi-app-x86_64 --subsystem=10 $< $@

clean:
	rm -f *.o *.so *.efi

.PHONY: all clean
.SECONDARY:
