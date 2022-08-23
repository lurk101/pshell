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
[area]: https://github.com/lurk101/pshell/discussions

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

Discussion [area].

[Getting Started]

Building from source.

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
For UART console, substitute the following cmake command
```
cmake .. -DUSB_CONSOLE=OFF
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
- long: long integer (64-bit) support

If you want to build from a different branch you can switch to it with
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
7:         while (--pos)
8:             printf(" ");
9:         printf("*\n");
10:     }
11:     return 0;
12: }
20038000    b5c0        push    {r6, r7, lr}
20038002    466f        mov     r7, sp
20038004    b084        sub     sp, #16         ; 0x10
20038006    463e        mov     r6, r7
20038008    3e04        subs    r6, #4
2003800a    2010        movs    r0, #16         ; 0x10
2003800c    6030        str     r0, [r6, #0]
2003800e    463e        mov     r6, r7
20038010    2000        movs    r0, #0
20038012    6030        str     r0, [r6, #0]
20038014    f000 f843   bl      2003809e
20038018    4638        mov     r0, r7
2003801a    3808        subs    r0, #8
2003801c    b401        push    {r0}
2003801e    6838        ldr     r0, [r7, #0]
20038020    4e27        ldr     r6, [pc, #156]  ; 0x200380c0
20038022    47b0        blx     r6
20038024    b401        push    {r0}
20038026    4827        ldr     r0, [pc, #156]  ; 0x200380c4
20038028    bc02        pop     {r1}
2003802a    4e27        ldr     r6, [pc, #156]  ; 0x200380c8
2003802c    47b0        blx     r6
2003802e    bc40        pop     {r6}
20038030    6030        str     r0, [r6, #0]
20038032    4638        mov     r0, r7
20038034    380c        subs    r0, #12         ; 0xc
20038036    b401        push    {r0}
20038038    201e        movs    r0, #30         ; 0x1e
2003803a    b401        push    {r0}
2003803c    4638        mov     r0, r7
2003803e    3808        subs    r0, #8
20038040    6800        ldr     r0, [r0, #0]
20038042    4e22        ldr     r6, [pc, #136]  ; 0x200380cc
20038044    47b0        blx     r6
20038046    b401        push    {r0}
20038048    4821        ldr     r0, [pc, #132]  ; 0x200380d0
2003804a    bc02        pop     {r1}
2003804c    4e1e        ldr     r6, [pc, #120]  ; 0x200380c8
2003804e    47b0        blx     r6
20038050    4e20        ldr     r6, [pc, #128]  ; 0x200380d4
20038052    47b0        blx     r6
20038054    bc40        pop     {r6}
20038056    1980        adds    r0, r0, r6
20038058    bc40        pop     {r6}
2003805a    6030        str     r0, [r6, #0]
2003805c    f000 f805   bl      2003806a
20038060    481d        ldr     r0, [pc, #116]  ; 0x200380d8
20038062    b401        push    {r0}
20038064    2001        movs    r0, #1
20038066    4e1d        ldr     r6, [pc, #116]  ; 0x200380dc
20038068    47b0        blx     r6
2003806a    4638        mov     r0, r7
2003806c    380c        subs    r0, #12         ; 0xc
2003806e    b401        push    {r0}
20038070    6806        ldr     r6, [r0, #0]
20038072    2001        movs    r0, #1
20038074    1a30        subs    r0, r6, r0
20038076    bc40        pop     {r6}
20038078    6030        str     r0, [r6, #0]
2003807a    2800        cmp     r0, #0
2003807c    d1f0        bne     20038060
2003807e    4818        ldr     r0, [pc, #96]   ; 0x200380e0
20038080    b401        push    {r0}
20038082    2001        movs    r0, #1
20038084    4e15        ldr     r6, [pc, #84]   ; 0x200380dc
20038086    47b0        blx     r6
20038088    4638        mov     r0, r7
2003808a    b401        push    {r0}
2003808c    6800        ldr     r0, [r0, #0]
2003808e    b401        push    {r0}
20038090    4638        mov     r0, r7
20038092    3804        subs    r0, #4
20038094    6800        ldr     r0, [r0, #0]
20038096    bc40        pop     {r6}
20038098    1980        adds    r0, r0, r6
2003809a    bc40        pop     {r6}
2003809c    6030        str     r0, [r6, #0]
2003809e    6838        ldr     r0, [r7, #0]
200380a0    b401        push    {r0}
200380a2    4810        ldr     r0, [pc, #64]   ; 0x200380e4
200380a4    bc40        pop     {r6}
200380a6    4286        cmp     r6, r0
200380a8    dd01        ble     200380ae
200380aa    2000        movs    r0, #0
200380ac    e000        b       200380b0
200380ae    2001        movs    r0, #1
200380b0    2800        cmp     r0, #0
200380b2    d1b1        bne     20038018
200380b4    2000        movs    r0, #0
200380b6    46bd        mov     sp, r7
200380b8    bdc0        pop     {r6, r7, pc}
200380ba    46bd        mov     sp, r7
200380bc    bdc0        pop     {r6, r7, pc}
200380be    46c0        mov     r8, r8
200380c0    1001 abdd   .word   0x1001abdd      ; i2f
200380c4    3c8e fa35   .word   0x3c8efa35
200380c8    1001 ab13   .word   0x1001ab13      ; fmul
200380cc    1001 ad05   .word   0x1001ad05      ; sinf
200380d0    41c8 0000   .word   0x41c80000
200380d4    1001 ac21   .word   0x1001ac21      ; f2i
200380d8    2003 c000   .word   0x2003c000
200380dc    1000 710f   .word   0x1000710f      ; printf
200380e0    2003 c004   .word   0x2003c004
200380e4    0000 0168   .word   0x00000168
200380e8    0000 0000   .word   0x00000000      ; "\0\0\0\0"
12: }
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

