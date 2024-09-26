A tiny Raspberry Pico and Pico2 shell with flash file system, Vi, and C compiler.

This project explores to what extent a reasonably competent self-hosted programming environment can be built on a modern embedded SoC.

[busybox]: https://www.busybox.net
[amacc compiler]: https://github.com/jserv/amacc.git
[c4 virtual machine]: https://github.com/rswier/c4.git
[Squint]: https://github.com/HPCguy/Squint.git
[list of implemented functions]: docs/FUNCTIONS.md
[What's new]: docs/WHATSNEW.md
[examples]: c-examples
[thread]: https://forums.raspberrypi.com/viewtopic.php?t=336843
[Getting Started]: docs/GETTING-STARTED.md
[area]: https://github.com/lurk101/pshell/discussions
[here]: docs/GPIO.md

Credit where credit is due...

- The vi code is ported from the [BusyBox] source code.
- The compiler code is a remix of the [amacc compiler] parser generator
and the [c4 virtual machine]. Many important amacc enhancements such as
floating point, array, struct and type checking support were taken fom
the [Squint] project.
- Native machine code generation for CM0 and CM33 by Jean M. Cyr.
- Console command recall and auto completion by Eric Olson.

[What's new] in pshell

About the compiler, briefly:

- Data types: integer, float, char and pointer.
- Aggregate types: array (up to 3 dimensions), struct and union.
- Flow control: for, while, if then else, break, continue, switch and goto.
- Memory, math, newlib and SDK functions. ([list of implemented functions])
- Native code generation for the Pico's cm0+ and pico2 cm33 cores.

Raspberry Pi Forums [thread].

Discussion [area].

[Getting Started]

Building from source for the RP2040.

```
# NOTE: Requires sdk 1.4 or later
git clone --recursive https://github.com/lurk101/pshell.git
cd pshell
mkdir build
cd build
cmake .. -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040
make
```
Building from source for the RP2350.

```
# NOTE: Requires sdk 2.0 or later
git clone --recursive https://github.com/lurk101/pshell.git
cd pshell
mkdir build
cd build
cmake .. -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350
make
```

For UART console, add the following cmake command parameter.
```
-DUSB_CONSOLE=OFF
```

For vgaboard with filesystem on microSD card. See [here] for details on differing GPIO & UART configurations.
```
cmake .. -DPICO_BOARD=vgaboard -DPICO_PLATFORM=rp2040
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

Pico Shell - Copyright (c) 1883 Thomas Edison
This program comes with ABSOLUTELY NO WARRANTY.
This is free software, and you are welcome to redistribute it
under certain conditions. See LICENSE file for details.

platform: pico2, console: UART0 [94 X 31], filesystem: flash

enter command or hit ENTER for help

file system automatically mounted

/: version 

Pico Shell v2.1.2, LittleFS v2.9, Vi 0.9.1, SDK v2.0.0

/: ls

     343 blink.c
     364 clocks.c
     120 crash.c
    1026 crc16.c
    3941 day8.c
   17071 day8.txt
    2308 doughnut.c
     171 exit.c
     976 fade.c
     175 forward.c
      69 hello.c
    1344 io.c
    2221 life.c
    5423 lorenz.c
    1951 penta.c
     215 pi.c
     249 printf.c
    1328 qsort.c
    2631 rndtest.c
     536 sieve.c
     373 sine.c
     673 string.c
    1265 testfloat.c
    1135 testint.c
     297 tictoc.c
   10192 wumpus.c

/: cc sine.c 

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

CC = 0

/: cc -s sine.c 
1: /* Math function test. Display a sine wave */
2: 
3: int main() {
4:     int angle, incr;
5:     for (incr = 16, angle = 0; angle <= 360; angle += incr) {
6:         float rad = (float)angle * 0.01745329252;
7:         printf("%.*s%c\n", 30 + (int)(sinf(rad) * 25.0),
8:                "                                                                          ;
9:     }
10:     return 0;
11: }
20078000    b580        push    {r7, lr}
20078002    466f        mov     r7, sp
20078004    b083        sub     sp, #12         ; 0xc
20078006    463b        mov     r3, r7
20078008    3b08        subs    r3, #8
2007800a    2010        movs    r0, #16         ; 0x10
2007800c    6018        str     r0, [r3, #0]
2007800e    1f38        subs    r0, r7, #4
20078010    b401        push    {r0}
20078012    2000        movs    r0, #0
20078014    bc08        pop     {r3}
20078016    6018        str     r0, [r3, #0]
20078018    f000 f843   bl      200780a2
2007801c    4638        mov     r0, r7
2007801e    380c        subs    r0, #12         ; 0xc
20078020    b401        push    {r0}
20078022    1f38        subs    r0, r7, #4
20078024    edd0 7a00   vldr    s15, [r0, #0]
20078028    eef8 7ae7   vcvt    s15, s15
2007802c    ee17 0a90   vmov    r0, s15
20078030    b401        push    {r0}
20078032    4824        ldr     r0, [pc, #144]  ; 0x200780c4
20078034    ee07 0a90   vmov    s15, r0
20078038    ecbd 7a01   vpop    {s14}
2007803c    ee67 7a27   vmul    s15, s14, s15
20078040    ee17 0a90   vmov    r0, s15
20078044    bc08        pop     {r3}
20078046    6018        str     r0, [r3, #0]
20078048    481f        ldr     r0, [pc, #124]  ; 0x200780c8
2007804a    b401        push    {r0}
2007804c    201e        movs    r0, #30         ; 0x1e
2007804e    b401        push    {r0}
20078050    4638        mov     r0, r7
20078052    380c        subs    r0, #12         ; 0xc
20078054    6800        ldr     r0, [r0, #0]
20078056    4b1d        ldr     r3, [pc, #116]  ; 0x200780cc
20078058    4798        blx     r3
2007805a    b401        push    {r0}
2007805c    481c        ldr     r0, [pc, #112]  ; 0x200780d0
2007805e    ee07 0a90   vmov    s15, r0
20078062    ecbd 7a01   vpop    {s14}
20078066    ee67 7a27   vmul    s15, s14, s15
2007806a    ee17 0a90   vmov    r0, s15
2007806e    eefd 7ae7   vcvt.s32 s15, s15
20078072    ee17 0a90   vmov    r0, s15
20078076    bc08        pop     {r3}
20078078    18c0        adds    r0, r0, r3
2007807a    b401        push    {r0}
2007807c    4815        ldr     r0, [pc, #84]   ; 0x200780d4
2007807e    b401        push    {r0}
20078080    202a        movs    r0, #42         ; 0x2a
20078082    b401        push    {r0}
20078084    2004        movs    r0, #4
20078086    4b14        ldr     r3, [pc, #80]   ; 0x200780d8
20078088    4798        blx     r3
2007808a    b004        add     sp, #16         ; 0x10
2007808c    1f38        subs    r0, r7, #4
2007808e    b401        push    {r0}
20078090    6800        ldr     r0, [r0, #0]
20078092    b401        push    {r0}
20078094    4638        mov     r0, r7
20078096    3808        subs    r0, #8
20078098    6800        ldr     r0, [r0, #0]
2007809a    bc08        pop     {r3}
2007809c    18c0        adds    r0, r0, r3
2007809e    bc08        pop     {r3}
200780a0    6018        str     r0, [r3, #0]
200780a2    1f38        subs    r0, r7, #4
200780a4    6800        ldr     r0, [r0, #0]
200780a6    b401        push    {r0}
200780a8    480c        ldr     r0, [pc, #48]   ; 0x200780dc
200780aa    bc02        pop     {r1}
200780ac    4281        cmp     r1, r0
200780ae    bfcc        ite     gt
200780b0    2000        movgt   r0, #0
200780b2    2001        movle   r0, #1
200780b4    2800        cmp     r0, #0
200780b6    d1b1        bne     2007801c
200780b8    2000        movs    r0, #0
200780ba    46bd        mov     sp, r7
200780bc    bd80        pop     {r7, pc}
200780be    46bd        mov     sp, r7
200780c0    bd80        pop     {r7, pc}
200780c2    46c0        mov     r8, r8
200780c4    3c8e fa35   .word   0x3c8efa35
200780c8    2007 c000   .word   0x2007c000
200780cc    1002 2b31   .word   0x10022b31      ; sinf
200780d0    41c8 0000   .word   0x41c80000
200780d4    2007 c008   .word   0x2007c008
200780d8    1001 520d   .word   0x1001520d      ; printf
200780dc    0000 0168   .word   0x00000168
200780e0    0000 0000   .word   0x00000000      ; "\0\0\0\0"
11: 

/: 

    cat - display a text file, use -p to paginate
     cc - compile & run C source file. cc -h for help
     cd - change directory
  clear - clear the screen
     cp - copy a file
 format - format the filesystem
    hex - simple hexdump, use -p to paginate
     ls - list a directory, -a to show hidden files
  mkdir - create a directory
  mount - mount the filesystem
     mv - rename a file or directory
   quit - shutdown the system
 reboot - restart the system
 resize - establish screen dimensions
     rm - remove a file or directory. -r for recursive
 status - display the filesystem status
    tar - manage tar archives
unmount - unmount the filesystem
version - display pico shell's version
     vi - edit file(s) with vi
   xget - get a file (xmodem, pico->host)
   xput - put a file (xmodem, host->pico)
   yget - get a file (ymodem, pico->host)
   yput - put a file (ymodem, host->pico)

/:
```

