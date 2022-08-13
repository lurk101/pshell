What's new in version 1.2.3

- rudimentary peep-hole optimization
- display correct line in error message
- fix boolean expression shortcut

What's new in version 1.2.2

- consisent use of long branch to avoid "too far" errors
- predefine PICO_ERROR_TIMEOUT
- detect undefined struct and union

What's new in version 1.2.1

- remove compiler branch distance restriction

What's new in version 1.2.0

- C compiler now generates native arm code
- virtual machine removed

What's new in version 1.1.17

- fix interaction between tab completion and the command history
- make sure to truncate when replacing a file
- wumpus example

What's new in version 1.1.16

- fix broken keyboard BREAK option

What's new in version 1.1.15

- fix handling of . and .. notation for directory names
- allow for(;;) synctax. Passes test 00034.c

What's new in version 1.1.14

- fix USB buffer overflow pasting in Vi
- Remove ESC as BREAK key
- Add getting started section

What's new in version 1.1.13

- fix tar handling of linux tar peculiarities
- revert update that broke test cae 00033
- add 200+ test cases
- remove xget file size limit
- cosistent use of CTL-C or ESC for user break

What's new in version 1.1.12

- fix compiler fail when break statement is used in while loop.

What's new in version 1.1.11

- compiler checks for code segment overflow
- compiler checks for data segment overflow

What's new in version 1.1.10

- forward function declaration support
- delete io.* and update all code to use standard (stdio) functions
- minor dgreadln optimization/cleanup
- compiler readability/maintainability improvements

What's new in version 1.1.9

- moved all source files to their own directory
- Added Unix compatible tar command
- Added recursive rmdir command

What's new in version 1.1.8

- mostly readability reorganization of compiler source
- added missing fmodf function since it's recommended by the compiler
- fixed missed opportunity optimizing modulus operator. (port introduced bug)
- updated functions list
- added lorenz simulation example

What's new in version 1.1.7

- constant expressions for global declarations

What's new in version 1.1.6

- command history and recall added to the shell

What's new in version 1.1.5

- formalized three cmake build options
```
-DUSB_CONSOLE=[ON|OFF]      Build for USB, otherwise UART
-DWITH_IRQ=[ON|OFF]         Build with interrup handling support
-DWITH_KBD_HALT=[ON|OFF]    Build with run-time kbd CTL-C checks
```
Turning off IRQ support and keyboard halt checking can significantly improve performance,

What's new in version 1.1.4

- add strncpy, strncmp, strncat, strrchr, asinf, acosf, atanf, atan2f, sinhf, coshf, tanhf, asinhf, acoshf, atanhf and log10f functions
- append .c to compiler filename if not specified
- add interrupt driven LED fade example
- a few minor instruction optimizations

What's new in version 1.1.3
- fixed compiler bug triggered by mid block declarations
- added exit function
- added popcount function
- added strchr function

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
