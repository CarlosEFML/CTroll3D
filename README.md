# CTroll3D

CTroll3D is a 3ds homebrew that allows you to control the Citra emulator from your 3ds.
For this to work, you need a version of Citra that supports CTroll3D (for now, only my own fork of Citra) and the CTroll3D homebrew on your 3ds.

CTroll3D supports:

- DPAD
- CPAD
- Touch
- Accelerometer
- Gyroscope
- Bottom screen mirroring

One advice: glReadPixels and VSync doesn't perform too well. So, to increase  the performance, please disable the VSync on Citra (and some other heavy stuffs like Accurate Multiplications). You can access these options in Preferences -> Graphics -> Advanced.

In some cases, the bottom screen mirroring can slow down your emulation, and should be disabled by pressing L + R + DOWN + click on Touch (it can be enabled by L + R + UP + click on Touch).



How to use:

1 - Open the CTroll3D homebrew on your 3ds

2 - On Citra, access the menu option: Tools -> Connect CTroll3D, and enter your 3DS IP Address 


You can download the CTroll3D.3dsx from github: https://github.com/CarlosEFML/CTroll3D/releases/download/0.0.1/CTroll3D.3dsx
Or clone/download the source and build it yourself.

If you are on macOS 11 or later (sorry, this is my environment and i can't build for windows/linux), you can download a binary release from: https://github.com/CarlosEFML/citra/releases/download/0.0.1/citra-portable-macOS11.zip
Otherwise, fell free do clone/download the source code and follow the instructions to build the emulator.

Building for windows: https://citra-emu.org/wiki/building-for-windows/
Building for linux: https://citra-emu.org/wiki/building-for-linux/
Building for mac: https://citra-emu.org/wiki/building-for-macos/


CTroll3D github: https://github.com/CarlosEFML/CTroll3D
Citra (with CTroll3D support) github: https://github.com/CarlosEFML/citra


