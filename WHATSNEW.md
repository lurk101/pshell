What's new in version 1.1.2

- added Conway's game of life
- polished a few knobs.
- hopefully fixed the last of the old compiler induced syntax errors?

What's new in version 1.1.1

- fix USB startup failures
- fix compile errors with older version of GCC

What's new in version 1.1.0

- Xmodem code consolidated.
- Interrupt handling.
- Removed stdinit-lib submodule dependency
- compiler support for taking address of function (needed for interrupt support)
- file IO to file system
- Added compiler help


What's new in version 1.0.8

- proper handling of system calls with variable number of arguments. (remove printf hack)
- argument type and number checking for all system calls.
- removed stdlib-init submodule dependency.
- consolidated xmodem code.
- implemented move and copy to directory

What's new in version 1.0.7

- SDK ADC function support
- SDK SPI function support
- SDK I2C function support
- Predefined SDK constants for SDK function parameters
- Vi uses secondary screen buffer such that prior screen restored on exit
- Fix build errors under SDK 1.3.x
- fix editor bug when more than one file name used in a command.

What's new in version 1.0.6

- file io functions open close read write lseek rename remove

What's new in version 1.0.5

- use float functions sqrtf sinf cosf tanf logf powf instead of double versions
- new string function strdup
- added string example
- improve compiler error diadnostics
- added flash boot to usb loader command
- improved vi keyboard handling

What's new in version 1.0.4

- function calls to Pico SDK gpio_* and pwm_* functions added
- C code examples
- sprintf and atoi function support
- string functions strcmp, strcpy, strcat, strlen, memcmp
- reboot command
- reboot on crash
