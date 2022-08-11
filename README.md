A tiny Raspberry Pico shell with flash file system, Vi, and C compiler.

[busybox]: https://www.busybox.net
[amacc compiler]: https://github.com/jserv/amacc.git
[c4 virtual machine]: https://github.com/rswier/c4.git
[Squint]: https://github.com/HPCguy/Squint.git
[list of implemented functions]: FUNCTIONS.md
[What's new]: WHATSNEW.md
[examples]: c-examples
[thread]: https://forums.raspberrypi.com/viewtopic.php?t=336843
[Getting Started]: GETTING-STARTED.md

Credit where credit is due...

- The vi code is ported from the [BusyBox] source code.
- The compiler code is a remix of the [amacc compiler] parser generator
and the [c4 virtual machine]. Many important amacc enhancements such as
floating point, array, struct and type checking support were taken fom
the [Squint] project. Native code generation by yours truly.

[What's new] in pshell

About the compiler, briefly…

- Data types: integer, float, char and pointer.
- Aggregate types: array, struct and union.
- Flow control: for, while, if then else, break, continue and goto.
- Memory, math and SDK functions. ([list of implemented functions])
- Native code generation for the Pico's cm0+ core
- Predefined stdio, stdlib, and SDK functions.

Raspberry Pi Forums [thread].

[Getting Started]

Building from source.

Edit the root folder CMakeLists.tx file to pick a console device, then:

```
# NOTE: Requires sdk 1.4 or later
git clone https://github.com/lurk101/pshell.git
cd pshell
git submodule update --init
mkdir build
cd build
cmake ..
make
```
Starting with version 1.0.4 all development will occur on the dev branch. To build it:
```
git checkout dev
```
To build for UART (USB is the default):
```
cmake .. -DUSB_CONSOLE=OFF
```

Git branches

- master: stable release branch
- dev: development and fix branch
- vm: legacy virtual machine branch (no longer under active development)

If you want to build the dev or vm branch you can switch to it with
```
git checkout branch-name
```

C source code [examples].

Example console log.

```sh
Pico Shell - Copyright 1883 (c) Thomas Edison
This program comes with ABSOLUTELY NO WARRANTY.
This is free software, and you are welcome to redistribute it
under certain conditions. See LICENSE file for details.

Pico Shell v1.0.8 [master d002f0a], LittleFS v2.5, Vi 0.9.1, SDK 1.4.0

console on UART [120 X 48]

enter command, hit return for help

file system automatically mounted

/: ls

       0 [src]
       9 .exrc
     960 b.c
      39 test.c

/: cd src

changed to /src

/src: ls

     270 blink.c
     319 clocks.c
     103 crash.c
      59 hello.c
    1133 io.c
     203 pi.c
     225 printf.c
     531 sieve.c
     298 sine.c
     584 string.c
     256 tictoc.c

/src: cc sine.c

                              *
                                    *
                                           *
                                                *
                                                    *
                                                      *
                                                      *
                                                     *
                                                 *
                                            *
                                      *
                               *
                         *
                   *
             *
         *
      *
      *
       *
          *
              *
                    *
                           *

CC=0

/src: cat sine.c
/* Math function test. Display a sine wave */

int main() {
        int angle, incr;
        for (angle = incr, incr = 16; angle <= 360; angle += incr) {
                float rad = (float)angle * 0.01745329252;
                int pos = 30 + (int)(sinf(rad) * 25.0);
                while (pos > 0) {
                        printf(" ");
                        pos--;
                }
                printf("*\n");
        }
}

/src: cc -si sine.c
1: /* Math function test. Display a sine wave */
2:
3: int main() {
4:      int angle, incr;
5:      for (angle = incr, incr = 16; angle <= 360; angle += incr) {
6:              float rad = (float)angle * 0.01745329252;
7:              int pos = 30 + (int)(sinf(rad) * 25.0);
8:              while (pos > 0) {
9:                      printf(" ");
10:                     pos--;
11:             }
12:             printf("*\n");
13:     }
14: }
0000: 00000007 00000004  ENT  4
0002: 00000000 ffffffff  LEA  angle (-1)
0004: 0000000a           PSH
0005: 00000000 fffffffe  LEA  incr (-2)
0007: 0000000d           LI
0008: 00000010           SI
0009: 00000000 fffffffe  LEA  incr (-2)
0011: 0000000a           PSH
0012: 00000001 00000010  IMM  16
0014: 00000010           SI
0015: 00000003 20015644  JMP  0104
0017: 00000000 fffffffd  LEA  rad (-3)
0019: 0000000a           PSH
0020: 00000000 ffffffff  LEA  angle (-1)
0022: 0000000d           LI
0023: 00000027           ITOF
0024: 0000000b           PSHF
0025: 00000002 3c8efa35  IMMF 0x3c8efa35
0027: 00000024           MULF
0028: 00000011           SF
0029: 00000000 fffffffc  LEA  pos (-4)
0031: 0000000a           PSH
0032: 00000001 0000001e  IMM  30
0034: 0000000a           PSH
0035: 00000000 fffffffd  LEA  rad (-3)
0037: 0000000e           LF
0038: 0000000b           PSHF
0039: 00000001 00000400  IMM  1024
0041: 0000002e 0000000e  SYSC sinf
0043: 00000008 00000421  ADJ  1
0045: 0000000b           PSHF
0046: 00000002 41c80000  IMMF 0x41c80000
0048: 00000024           MULF
0049: 00000026           FTOI
0050: 0000001d           ADD
0051: 00000010           SI
0052: 00000003 200155d4  JMP  0076
0054: 00000001 2000c868  IMM  0x2000c868
0056: 0000000a           PSH
0057: 00000001 00000000  IMM  0
0059: 0000002e 00000000  SYSC printf
0061: 00000008 00000001  ADJ  1
0063: 00000000 fffffffc  LEA  pos (-4)
0065: 0000000a           PSH
0066: 0000000d           LI
0067: 0000000a           PSH
0068: 00000001 00000001  IMM  1
0070: 0000001e           SUB
0071: 00000010           SI
0072: 0000000a           PSH
0073: 00000001 00000001  IMM  1
0075: 0000001d           ADD
0076: 00000000 fffffffc  LEA  pos (-4)
0078: 0000000d           LI
0079: 0000000a           PSH
0080: 00000001 00000000  IMM  0
0082: 00000019           GT
0083: 00000006 2001557c  BNZ  0054
0085: 00000001 2000c86c  IMM  0x2000c86c
0087: 0000000a           PSH
0088: 00000001 00000000  IMM  0
0090: 0000002e 00000000  SYSC printf
0092: 00000008 00000001  ADJ  1
0094: 00000000 ffffffff  LEA  angle (-1)
0096: 0000000a           PSH
0097: 0000000d           LI
0098: 0000000a           PSH
0099: 00000000 fffffffe  LEA  incr (-2)
0101: 0000000d           LI
0102: 0000001d           ADD
0103: 00000010           SI
0104: 00000000 ffffffff  LEA  angle (-1)
0106: 0000000d           LI
0107: 0000000a           PSH
0108: 00000001 00000168  IMM  360
0110: 0000001a           LE
0111: 00000006 200154e8  BNZ  0017
0113: 00000009           LEV
14:

/src:

    cat - display text file
     cc - compile C source file
     cd - change directory
  clear - clear the screen
     cp - copy a file
 format - format the filesystem
     ls - list directory
  mkdir - create directory
  mount - mount filesystem
     mv - rename file or directory
   quit - shutdown system
 reboot - Restart system
     rm - remove file or directory
 status - filesystem status
unmount - unmount filesystem
     vi - editor
   xget - get file (xmodem)
   xput - put file (xmodem)

/src:
```

