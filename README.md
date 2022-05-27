# pcspkr-midi
Turn your pc's speaker into an ALSA MIDI device.

To get write perms on the pc speaker device, add to ```/usr/lib/udev/rules.d/70-pcspkr-beep.rules``` the following line:
```
ACTION=="add", SUBSYSTEM=="input", ATTRS{name}=="PC Speaker", ENV{DEVNAME}!="", GROUP="audio", MODE="0620"
```
followed by:
```bash
groups # Check that you are in the audio group.
sudo modprobe pcspkr
sudo udevadm control --reload # reload udev rules
```

Next, clone, build, install and run ```pcspkr-midi``` as follows:
```bash
git clone https://github.com/koppi/pcspkr-midi.git
cd pcspkr-midi/
make
sudo make install
pcspkr-midi
```

– Tested on Ubuntu 22.04.

## Authors

* **Jakob Flierl** - [koppi](https://github.com/koppi)
