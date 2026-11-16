# Firefox with the ThinkPad 10 cameras

atomisp pads each image row, and the camera only reports one frame rate per
mode. Two browser problems follow:

- **Firefox ESR 153** (Kali's default) asks for unpadded buffers, both on
  its direct V4L2 path (the picture turns into streaks) and through
  PipeWire (the camera never starts). **Firefox 160+** reads the row
  stride from each PipeWire buffer and works.
- Firefox must use **PipeWire** for the camera; its direct V4L2 path ignores
  the row padding in every version.

## Setup

1. Install current Firefox from Mozilla's APT repository (keeps ESR):

   ```sh
   sh install-firefox-release.sh
   ```

2. Start Firefox once and close it, then enable PipeWire cameras in its
   profile, either in `about:config`
   (`media.webrtc.camera.allow-pipewire` = `true`) or with:

   ```sh
   echo 'user_pref("media.webrtc.camera.allow-pipewire", true);' >> ~/.mozilla/firefox/*.default-release/user.js
   ```

3. Restart Firefox and allow camera access when asked; the camera is called
   "ATOM ISP (V4L2)".

Close Firefox before switching between the front and rear camera: Firefox
160 can crash if the camera disappears and returns while a page uses it.
