// SPDX-License-Identifier: GPL-2.0-only
/*
 * ispsel.efi - switch the Bay Trail ISP between ACPI mode (ISPDevSel=1,
 * "subdevice of B0D2F0 (IGD)", Windows default) and PCI mode (ISPDevSel=2,
 * "ISP PCI Device as B0D3F0", needed by Linux atomisp).
 *
 * Target: Lenovo ThinkPad 10 (20C1/20C3), BIOS GWET50WW (1.50), Phoenix SCT.
 * Offsets come from the PlatformHiiAdvancedDxe IFR in that BIOS:
 *   Setup (EC87D643-EBA4-4BB5-A1E5-3F3E36B20DA9), size 878
 *   0x235 "ISP PCI Device Selection" 0=Disable 1=B0D2F0(IGD) 2=B0D3F0
 *   0x236 "ISP Enable/Disable"       0=Disable 1=Enable
 *
 * Build with -DTARGET=2 (PCI/Linux) or -DTARGET=1 (ACPI/Windows, restore).
 * Refuses to write unless the variable has the expected size and both
 * bytes hold a known value, and reads the variable back after writing.
 */
#include <efi.h>
#include <efilib.h>

#ifndef TARGET
#error "define TARGET=1 or TARGET=2"
#endif

#define SETUP_SIZE   878
#define OFF_DEVSEL   0x235
#define OFF_ISPEN    0x236

static EFI_GUID SetupGuid =
    { 0xec87d643, 0xeba4, 0x4bb5, { 0xa1, 0xe5, 0x3f, 0x3e, 0x36, 0xb2, 0x0d, 0xa9 } };

static void wait_key(void)
{
    EFI_INPUT_KEY k;
    UINTN idx;
    Print(L"\nPress any key to continue booting...\n");
    uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
    uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &idx);
    uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &k);
}

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
    UINT8 buf[SETUP_SIZE + 16];
    UINTN size = sizeof(buf);
    UINT32 attr = 0;
    EFI_STATUS st;

    InitializeLib(image, systab);
    Print(L"ispsel: target ISPDevSel=%d\n", TARGET);

    st = uefi_call_wrapper(RT->GetVariable, 5, L"Setup", &SetupGuid, &attr, &size, buf);
    if (EFI_ERROR(st)) {
        Print(L"GetVariable(Setup) failed: %r\n", st);
        goto out;
    }
    Print(L"Setup: size=%d attr=0x%x ISPDevSel=%d ISPEn=%d\n",
          size, attr, buf[OFF_DEVSEL], buf[OFF_ISPEN]);

    if (size != SETUP_SIZE || buf[OFF_ISPEN] != 1 ||
        (buf[OFF_DEVSEL] != 1 && buf[OFF_DEVSEL] != 2)) {
        Print(L"Unexpected layout/values - refusing to write.\n");
        goto out;
    }
    if (buf[OFF_DEVSEL] == TARGET) {
        Print(L"Already set, nothing to do.\n");
        goto out;
    }

    buf[OFF_DEVSEL] = TARGET;
    st = uefi_call_wrapper(RT->SetVariable, 5, L"Setup", &SetupGuid, attr, size, buf);
    if (EFI_ERROR(st)) {
        Print(L"SetVariable(Setup) failed: %r (nothing changed)\n", st);
        goto out;
    }

    size = sizeof(buf);
    st = uefi_call_wrapper(RT->GetVariable, 5, L"Setup", &SetupGuid, &attr, &size, buf);
    Print(L"Read back: %r ISPDevSel=%d ISPEn=%d\n", st, buf[OFF_DEVSEL], buf[OFF_ISPEN]);
    Print(L"%s\n", buf[OFF_DEVSEL] == TARGET ? L"OK - power off fully, then boot."
                                             : L"MISMATCH - value did not stick.");
out:
    wait_key();
    return EFI_SUCCESS;
}
