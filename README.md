A tiny Raspberry Pico shell with flash file system, Vi, and C compiler.

This project explores to what extent a reasonably competent self-hosted programming environment can be built on a modern embedded SoC.

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

Git branches

- master: stable release branch
- dev: development and fix branch
- vm: legacy virtual machine branch (no longer under active development)

If you want to build from a different branch you can switch to it with
```
git checkout branch-name
```

C source code [examples].

Example console log.

```sh
Pico Shell - Copyright © 1883 Thomas Edison
This program comes with ABSOLUTELY NO WARRANTY.
This is free software, and you are welcome to redistribute it
under certain conditions. See LICENSE file for details.

Pico Shell v1.2.11, LittleFS v2.5, Vi 0.9.1, SDK v1.4.0

console on UART [100 X 40]

enter command or hit ENTER for help

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

/src/: cc -s sine
1: /* Math function test. Display a sine wave */
2:
3: int main() {
4:     int angle, incr;
5:     for (incr = 16, angle = 0; angle <= 360; angle += incr) {
6:         float rad = (float)angle * 0.01745329252;
7:         int pos = 30 + (int)(sinf(rad) * 25.0);
8:         while (--pos)
9:             printf(" ");
10:         printf("*\n");
11:     }
12:     return 0;
13: }
20038000    b580        push    {r7, lr}
20038002    466f        mov     r7, sp
20038004    b084        sub     sp, #16         ; 0x10
20038006    463b        mov     r3, r7
20038008    3b08        subs    r3, #8
2003800a    2010        movs    r0, #16         ; 0x10
2003800c    6018        str     r0, [r3, #0]
2003800e    1f3b        subs    r3, r7, #4
20038010    2000        movs    r0, #0
20038012    6018        str     r0, [r3, #0]
20038014    f000 f848   bl      200380a8
20038018    4638        mov     r0, r7
2003801a    380c        subs    r0, #12         ; 0xc
2003801c    b401        push    {r0}
2003801e    4638        mov     r0, r7
20038020    3804        subs    r0, #4
20038022    6800        ldr     r0, [r0, #0]
20038024    4b29        ldr     r3, [pc, #164]  ; 0x200380cc
20038026    4798        blx     r3
20038028    b401        push    {r0}
2003802a    4829        ldr     r0, [pc, #164]  ; 0x200380d0
2003802c    bc02        pop     {r1}
2003802e    4b29        ldr     r3, [pc, #164]  ; 0x200380d4
20038030    4798        blx     r3
20038032    bc08        pop     {r3}
20038034    6018        str     r0, [r3, #0]
20038036    4638        mov     r0, r7
20038038    3810        subs    r0, #16         ; 0x10
2003803a    b401        push    {r0}
2003803c    201e        movs    r0, #30         ; 0x1e
2003803e    b401        push    {r0}
20038040    4638        mov     r0, r7
20038042    380c        subs    r0, #12         ; 0xc
20038044    6800        ldr     r0, [r0, #0]
20038046    4b24        ldr     r3, [pc, #144]  ; 0x200380d8
20038048    4798        blx     r3
2003804a    b401        push    {r0}
2003804c    4823        ldr     r0, [pc, #140]  ; 0x200380dc
2003804e    bc02        pop     {r1}
20038050    4b20        ldr     r3, [pc, #128]  ; 0x200380d4
20038052    4798        blx     r3
20038054    4b22        ldr     r3, [pc, #136]  ; 0x200380e0
20038056    4798        blx     r3
20038058    bc08        pop     {r3}
2003805a    18c0        adds    r0, r0, r3
2003805c    bc08        pop     {r3}
2003805e    6018        str     r0, [r3, #0]
20038060    f000 f806   bl      20038070
20038064    481f        ldr     r0, [pc, #124]  ; 0x200380e4
20038066    b401        push    {r0}
20038068    2001        movs    r0, #1
2003806a    4b1f        ldr     r3, [pc, #124]  ; 0x200380e8
2003806c    4798        blx     r3
2003806e    b001        add     sp, #4
20038070    4638        mov     r0, r7
20038072    3810        subs    r0, #16         ; 0x10
20038074    b401        push    {r0}
20038076    6803        ldr     r3, [r0, #0]
20038078    2001        movs    r0, #1
2003807a    1a18        subs    r0, r3, r0
2003807c    bc08        pop     {r3}
2003807e    6018        str     r0, [r3, #0]
20038080    2800        cmp     r0, #0
20038082    d1ef        bne     20038064
20038084    4819        ldr     r0, [pc, #100]  ; 0x200380ec
20038086    b401        push    {r0}
20038088    2001        movs    r0, #1
2003808a    4b17        ldr     r3, [pc, #92]   ; 0x200380e8
2003808c    4798        blx     r3
2003808e    b001        add     sp, #4
20038090    4638        mov     r0, r7
20038092    3804        subs    r0, #4
20038094    b401        push    {r0}
20038096    6800        ldr     r0, [r0, #0]
20038098    b401        push    {r0}
2003809a    4638        mov     r0, r7
2003809c    3808        subs    r0, #8
2003809e    6800        ldr     r0, [r0, #0]
200380a0    bc08        pop     {r3}
200380a2    18c0        adds    r0, r0, r3
200380a4    bc08        pop     {r3}
200380a6    6018        str     r0, [r3, #0]
200380a8    4638        mov     r0, r7
200380aa    3804        subs    r0, #4
200380ac    6800        ldr     r0, [r0, #0]
200380ae    b401        push    {r0}
200380b0    480f        ldr     r0, [pc, #60]   ; 0x200380f0
200380b2    bc02        pop     {r1}
200380b4    0003        movs    r3, r0
200380b6    0fc8        lsrs    r0, r1, #31
200380b8    17da        asrs    r2, r3, #31
200380ba    428b        cmp     r3, r1
200380bc    4150        adcs    r0, r2
200380be    2800        cmp     r0, #0
200380c0    d1aa        bne     20038018
200380c2    2000        movs    r0, #0
200380c4    46bd        mov     sp, r7
200380c6    bd80        pop     {r7, pc}
200380c8    46bd        mov     sp, r7
200380ca    bd80        pop     {r7, pc}
200380cc    1001 a815   .word   0x1001a815      ; i2f
200380d0    3c8e fa35   .word   0x3c8efa35
200380d4    1001 a74b   .word   0x1001a74b      ; fmul
200380d8    1001 a93d   .word   0x1001a93d      ; sinf
200380dc    41c8 0000   .word   0x41c80000
200380e0    1001 a859   .word   0x1001a859      ; f2i
200380e4    2003 c000   .word   0x2003c000
200380e8    1000 6f9f   .word   0x10006f9f      ; printf
200380ec    2003 c004   .word   0x2003c004
200380f0    0000 0168   .word   0x00000168
200380f4    0000 0000   .word   0x00000000      ; "\0\0\0\0"
13:

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

