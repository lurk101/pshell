What's new in version 1.2.29

- update functions document
- fix extern table out of order
- add simple hexdump command
- add -p (paginate) option to cat and hex commands
- cat terminate on EOF (0x1A) char

What's new in version 1.2.28

- further optimization of C compile memory management
- Vi recognizes and understands the EOF (0x1a) character. This eliminates the need do delete CTL-Z chars
    at the end of files transferred by xmodem which transfer files in 256 byte chunks regardless of actual file size.

What's new in version 1.2.27

- fix crash comppiling to executable

What's new in version 1.2.26

- Update disassembler module
- Update littlefs to version 2.7
- Implement standard calloc function

What's new in version 1.2.25

- rebuilt with SDK 1.5.1

What's new in version 1.2.24

- optimize memory management
- fix compiler memory leak

What's new in version 1.2.23

- fix cat command crash
- Add support for get_rand_32 SDK function

What's new in version 1.2.22

- Recompiled with gcc 12.2, SDK 1.5, littlefs to 2.5.1

What's new in version 1.2.21

- support builing for vgaboard with filesystem on microSD

What's new in version 1.2.20

- fix stack allocation for local multi dimensional arrays
- fix sizeof for multi dimensional arrays, was limited to 1 dimension
- fix small global data limit for strings literals
- add strtol string function and compiler version flag in executable's header
- suppress gcc12 newlib linker warnings

What's new in version 1.2.19

- fix compound assignment bug
- implement #pragma uchar, tells compiler to use unsigned char variables
- grow symbol table dynamically
- fix global greater than 4K bug

What's new in version 1.2.18

- a few more peep hole optimizations
- hide hidden files, add -a option to ls command
- update tar to preserve executable attribute
- restore missing instruction at end of function disassembly (cosmetic)
- add cmd line option to disable peep hole optimizer
- Fix bugs with dgreadln

What's new in version 1.2.17

- fix command line parameter setup, broken by new invocation code
- fix missing source line in error msg
- update examples
- fix potential vi memory leak
- drop peep hole, does more harm than good
- fix forward function declaration disassembly
- robust user code invocation
- add compiler source comments

What's new in version 1.2.16

- optimize function calls. save/restore fewer registers
- adjust exit function
- update peep-hole optimizer, add array indexing optimization
- add quicksort example
- line buffering serves no purpose, remove it.

What's new in version 1.2.15

- Another critical printf fix
- optimize float relational operators
- trig function optimization

What's new in version 1.2.14

- Merge CC and VI uninitialized globals to save memory.
- Fix printf bug. prevent uncontrolled stack growth
- Check executable attributes on tab completion.
- Fix executable attribute on copy and refactor tab completion.

What's new in version 1.2.13

- optimize integer compare operators
- fix broken  /= *= += -= for floats

What's new in version 1.2.12

- fix load data init bug preventing older code execution
- save history on quit and usb reboot
- add pentamino example
- more peep hole optimizations
- allow constant integer or float expressions as #define values

What's new in version 1.2.11

- command history survives reboots
- integer arrays were not always word aligned, causing crashes on CM0
- allow negative values in simple defines
- fix executable search path misbehavior

What's new in version 1.2.10

- binary executable now pshell version independent
- fix search path bug

What's new in version 1.2.9

- executable binaries invoked directly by name, remove run command
- executable search order is current directory followed by /bin directory
- verify executable is compatible with current version of pshell, suggest recompile if not

What's new in version 1.2.8

- compiler supports the -o option to create binary executable.
- run command added to run binary executable, till further intgration

What's new in version 1.2.7

- for compatibility use Unix values for O_RDONLY, O_WRONLY, etc, and translate them automatically to LFS mode values.
- char type now defaults to signed, use -u option for unsigned chars

What's new in version 1.2.6

- fix bug loading small negative constants.
- support octal and hexadecimal character representations in strings and char constants
- revert banner update and add size detect command

What's new in version 1.2.5

- internal cleanup of branching logic
- fix c-style multi line comment crash
- check for line buffer and symbol table overflow
- source file no longer needs to be memory resident for compilation.
- fix bad branch at bootom of long for loops

What's new in version 1.2.4

- fix parameter mismatch crash
- check for AST overflow
- increase AST to 32K
- remove a few redundant instructions

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
