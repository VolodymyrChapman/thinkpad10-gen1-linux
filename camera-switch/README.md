# Camera switcher

Both cameras are inputs of the one atomisp video device, so apps see a
single camera ("ATOM ISP"). `tp10-camera` chooses which sensor that is.

```sh
sh install.sh            # command + "Switch Camera" menu entry
sh install.sh --panel    # also a panel button (XFCE)
sh install.sh --remove   # undo
```

- **Click** "Switch Camera" (menu or panel) to swap front/rear; a
  notification says which camera is now in use.
- **Right-click** it for "Use Front Camera" / "Use Rear Camera".
- In a terminal: `tp10-camera front|rear|toggle|status`.

Close apps using the camera first; the switcher refuses and says which app
has it open otherwise. Switching restarts WirePlumber (a second or two)
so PipeWire picks up the new camera's sizes; sound may blip briefly.

Needs `v4l-utils`, `wireplumber`, `psmisc` (for `fuser`) and, for
notifications, `libnotify-bin`. No root.
