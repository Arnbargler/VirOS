# VirOS

A modular, **native UEFI text-console** environment. No Linux kernel, GOP graphics renderer, or framebuffer drawing.

## First milestone: launcher

- Requests **80x25 explicitly** via UEFI \`QueryMode\` / \`SetMode\`.
- Native UEFI foreground/background colours, cursor positioning and full-width bars.
- Four palettes (F2), help panel (F1), rescan (F3).
- The right panel shows a live UEFI RTC clock and the current month with today's date highlighted. Firmware time may be UTC rather than local time.
- Normal selection changes paint two rows; scrolling paints only the application list, not the entire screen.
- Sixteen visible-in-menu placeholder titles (scroll to browse all).
- Scans \`\\APPS\` on the **same FAT32 filesystem** from which the launcher booted for \`.EFI\` files; real files appear after placeholder entries.
- Loads apps with UEFI \`LoadImage\` / \`StartImage\`. When apps return, the launcher redraws its menu.
- Disables the firmware boot-services watchdog with `SetWatchdogTimer(0, 0, 0, NULL)` so the launcher can remain running.
- No networking or drivers. Stays inside UEFI Boot Services.

**Important:** Placeholder entries do not launch anything. Only actual \`.efi\` files under \`\\APPS\` are executable. Secure Boot may reject unsigned EFI applications.

## Build on Ubuntu/Debian

Install GNU-EFI and build tools:

\`\`\`sh
sudo apt update
sudo apt install build-essential gnu-efi binutils
make
\`\`\`

Output: \`dist/EFI/BOOT/BOOTX64.EFI\`.

## Prepare a dedicated USB key

**Back up your USB key first. Formatting destroys its contents.** Create a FAT32 partition (preferably GPT with an EFI System Partition) on the *correct* USB device using your preferred partition manager. Do not format your desktop drive.

Mount the FAT32 partition and run:

\`\`\`sh
make usb USB=/path/to/mounted/VIROS
\`\`\`

On the FAT32 volume, the layout becomes:

\`\`\`text
EFI/BOOT/BOOTX64.EFI
APPS/
\`\`\`

Boot the stick using the system's **UEFI removable media** boot menu, not legacy/CSM mode. Disable Secure Boot for unsigned development binaries if needed.

Add future programs by copying \`APPNAME.EFI\` to \`APPS/\`, then press F3. The launcher discovers it without recompilation.

## Controls

- Up/Down: change selection
- Page Up/Page Down, Home/End: faster movement
- Enter: launch selected real app / show placeholder status
- F1: help
- F2: change colour theme
- F3: rescan the USB's APPS directory
- Esc: repaint

## Architecture notes

UEFI \`SimpleTextOutput\` has firmware-controlled fonts: no guaranteed bold, italics, font sizes or direct VGA text memory. Palettes use standard 16-colour UEFI attributes, so actual shades and console rendering depend on firmware. No calls to GOP are made by VirOS.

The launcher stays active in its input loop; do **not** call \`ExitBootServices\` in child applications. Installed apps should use the firmware console and restore any changed mode when they return. \`LoadImage\` can reject incorrect-architecture binaries.

This is a first implementation and has **not yet been hardware tested**. Next: sample app, shared UI code, reader and editor.

## BLOCKS.EFI game

Native UEFI 80x25 text-mode falling-block puzzle, with coloured cells, score, cleared lines, and accelerating gravity. Does not use GOP or framebuffer graphics.

- Left/Right move, Up rotate, Down soft drop, Space hard drop.
- P pause, R restart, Esc exits to the VirOS launcher.
- Runs its own UEFI timer event, closes that event before returning.

The game is a first implementation and needs real-hardware testing. A game board repaint may be visible on slower firmware consoles; differential cell rendering is a future optimisation.

## CHESS.EFI Unicode display test

`CHESS.EFI` is currently a **font compatibility test**, not a playable chess game. It shows ASCII controls, single/double box-drawing characters, block/shading characters, arrows, and all twelve Unicode chess pieces, together with UEFI `SimpleTextOutput.TestString` results for each group. Firmware may claim a glyph is supported yet still draw it incorrectly. The app requests 80x25 and exits on any key. No GOP usage or bundled fonts.
