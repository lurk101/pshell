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

/src/: cc -s sine.c

0: /* Math function test. Display a sine wave */
1:
2: int main() {
3:     int angle, incr;
4:     for (incr = 16, angle = 0; angle <= 360; angle += incr) {
5:         float rad = (float)angle * 0.01745329252;
6:         int pos = 30 + (int)(sinf(rad) * 25.0);
7:         while (pos > 0) {
8:             printf(" ");
9:             pos--;
10:         }
11:         printf("*\n");
12:     }
13:     return 0;
14: }
2001b4a0    b5c0        push    {r6, r7, lr}
2001b4a2    466f        mov     r7, sp
2001b4a4    b084        sub     sp, #16         ; 0x10
2001b4a6    4638        mov     r0, r7
2001b4a8    3804        subs    r0, #4
2001b4aa    b401        push    {r0}
2001b4ac    2010        movs    r0, #16         ; 0x10
2001b4ae    bc02        pop     {r1}
2001b4b0    6008        str     r0, [r1, #0]
2001b4b2    4639        mov     r1, r7
2001b4b4    2000        movs    r0, #0
2001b4b6    6008        str     r0, [r1, #0]
2001b4b8    f000 f855   bl      2001b566
2001b4bc    4638        mov     r0, r7
2001b4be    3808        subs    r0, #8
2001b4c0    b401        push    {r0}
2001b4c2    4638        mov     r0, r7
2001b4c4    6800        ldr     r0, [r0, #0]
2001b4c6    4a30        ldr     r2, [pc, #192]  ; 0x2001b588
2001b4c8    4790        blx     r2
2001b4ca    b401        push    {r0}
2001b4cc    482f        ldr     r0, [pc, #188]  ; 0x2001b58c
2001b4ce    4601        mov     r1, r0
2001b4d0    bc01        pop     {r0}
2001b4d2    4a2f        ldr     r2, [pc, #188]  ; 0x2001b590
2001b4d4    4790        blx     r2
2001b4d6    bc02        pop     {r1}
2001b4d8    6008        str     r0, [r1, #0]
2001b4da    4638        mov     r0, r7
2001b4dc    380c        subs    r0, #12         ; 0xc
2001b4de    b401        push    {r0}
2001b4e0    201e        movs    r0, #30         ; 0x1e
2001b4e2    b401        push    {r0}
2001b4e4    4638        mov     r0, r7
2001b4e6    3808        subs    r0, #8
2001b4e8    6800        ldr     r0, [r0, #0]
2001b4ea    b401        push    {r0}
2001b4ec    bc01        pop     {r0}
2001b4ee    4e29        ldr     r6, [pc, #164]  ; 0x2001b594
2001b4f0    47b0        blx     r6
2001b4f2    b401        push    {r0}
2001b4f4    4828        ldr     r0, [pc, #160]  ; 0x2001b598
2001b4f6    4601        mov     r1, r0
2001b4f8    bc01        pop     {r0}
2001b4fa    4a25        ldr     r2, [pc, #148]  ; 0x2001b590
2001b4fc    4790        blx     r2
2001b4fe    4a27        ldr     r2, [pc, #156]  ; 0x2001b59c
2001b500    4790        blx     r2
2001b502    bc02        pop     {r1}
2001b504    1840        adds    r0, r0, r1
2001b506    bc02        pop     {r1}
2001b508    6008        str     r0, [r1, #0]
2001b50a    f000 f811   bl      2001b530
2001b50e    4824        ldr     r0, [pc, #144]  ; 0x2001b5a0
2001b510    b401        push    {r0}
2001b512    2001        movs    r0, #1
2001b514    4e23        ldr     r6, [pc, #140]  ; 0x2001b5a4
2001b516    47b0        blx     r6
2001b518    4638        mov     r0, r7
2001b51a    380c        subs    r0, #12         ; 0xc
2001b51c    b401        push    {r0}
2001b51e    6801        ldr     r1, [r0, #0]
2001b520    2001        movs    r0, #1
2001b522    1a08        subs    r0, r1, r0
2001b524    bc02        pop     {r1}
2001b526    6008        str     r0, [r1, #0]
2001b528    b401        push    {r0}
2001b52a    2001        movs    r0, #1
2001b52c    bc02        pop     {r1}
2001b52e    1840        adds    r0, r0, r1
2001b530    4638        mov     r0, r7
2001b532    380c        subs    r0, #12         ; 0xc
2001b534    6801        ldr     r1, [r0, #0]
2001b536    2000        movs    r0, #0
2001b538    4281        cmp     r1, r0
2001b53a    dc01        bgt     2001b540
2001b53c    2000        movs    r0, #0
2001b53e    e000        b       2001b542
2001b540    2001        movs    r0, #1
2001b542    2800        cmp     r0, #0
2001b544    d1e3        bne     2001b50e
2001b546    4818        ldr     r0, [pc, #96]   ; 0x2001b5a8
2001b548    b401        push    {r0}
2001b54a    2001        movs    r0, #1
2001b54c    4e15        ldr     r6, [pc, #84]   ; 0x2001b5a4
2001b54e    47b0        blx     r6
2001b550    4638        mov     r0, r7
2001b552    b401        push    {r0}
2001b554    6800        ldr     r0, [r0, #0]
2001b556    b401        push    {r0}
2001b558    4638        mov     r0, r7
2001b55a    3804        subs    r0, #4
2001b55c    6800        ldr     r0, [r0, #0]
2001b55e    bc02        pop     {r1}
2001b560    1840        adds    r0, r0, r1
2001b562    bc02        pop     {r1}
2001b564    6008        str     r0, [r1, #0]
2001b566    4638        mov     r0, r7
2001b568    6800        ldr     r0, [r0, #0]
2001b56a    b401        push    {r0}
2001b56c    480f        ldr     r0, [pc, #60]   ; 0x2001b5ac
2001b56e    bc02        pop     {r1}
2001b570    4281        cmp     r1, r0
2001b572    dd01        ble     2001b578
2001b574    2000        movs    r0, #0
2001b576    e000        b       2001b57a
2001b578    2001        movs    r0, #1
2001b57a    2800        cmp     r0, #0
2001b57c    d19e        bne     2001b4bc
2001b57e    2000        movs    r0, #0
2001b580    46bd        mov     sp, r7
2001b582    bdc0        pop     {r6, r7, pc}
2001b584    46bd        mov     sp, r7
2001b586    bdc0        pop     {r6, r7, pc}
2001b588    1002 2451   .word   0x10022451      ; aeabi_i2f
2001b58c    3c8e fa35   .word   0x3c8efa35
2001b590    1002 238b   .word   0x1002238b      ; aeabi_fmul
2001b594    1002 2569   .word   0x10022569      ; sinf
2001b598    41c8 0000   .word   0x41c80000
2001b59c    1002 2495   .word   0x10022495      ; aeabi_f2iz
2001b5a0    2000 e5c8   .word   0x2000e5c8
2001b5a4    1000 b5d5   .word   0x1000b5d5      ; printf
2001b5a8    2000 e5cc   .word   0x2000e5cc
2001b5ac    0000 0168   .word   0x00000168
2001b5b0    0000 0000   .word   0x00000000      ; "\0\0\0\0"
14: }

/src/:

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

