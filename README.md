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
200163c0    b5c0        push    {r6, r7, lr}
200163c2    466f        mov     r7, sp
200163c4    b084        sub     sp, #16         ; 0x10
200163c6    2004        movs    r0, #4
200163c8    4240        rsbs    r0, r0
200163ca    4438        add     r0, r7                                                              
200163cc    b401        push    {r0}                                                                
200163ce    2010        movs    r0, #16         ; 0x10                                              
200163d0    bc02        pop     {r1}                                                                
200163d2    6008        str     r0, [r1, #0]                                                        
200163d4    4638        mov     r0, r7                                                              
200163d6    b401        push    {r0}                                                                
200163d8    2000        movs    r0, #0                                                              
200163da    bc02        pop     {r1}                                                                
200163dc    6008        str     r0, [r1, #0]                                                        
200163de    f000 f85f   bl      200164a0                                                            
200163e2    2008        movs    r0, #8                                                              
200163e4    4240        rsbs    r0, r0                                                              
200163e6    4438        add     r0, r7                                                              
200163e8    b401        push    {r0}                                                                
200163ea    4638        mov     r0, r7                                                              
200163ec    6800        ldr     r0, [r0, #0]                                                        
200163ee    4a35        ldr     r2, [pc, #212]  ; 0x200164c4                                        
200163f0    4790        blx     r2                                                                  
200163f2    b401        push    {r0}                                                                
200163f4    4834        ldr     r0, [pc, #208]  ; 0x200164c8                                        
200163f6    4601        mov     r1, r0                                                              
200163f8    bc01        pop     {r0}                                                                
200163fa    4a34        ldr     r2, [pc, #208]  ; 0x200164cc                                        
200163fc    4790        blx     r2                                                                  
200163fe    bc02        pop     {r1}                                                                
20016400    6008        str     r0, [r1, #0]                                                        
20016402    200c        movs    r0, #12         ; 0xc                                               
20016404    4240        rsbs    r0, r0                                                              
20016406    4438        add     r0, r7                                                              
20016408    b401        push    {r0}                                                                
2001640a    201e        movs    r0, #30         ; 0x1e                                              
2001640c    b401        push    {r0}                                                                
2001640e    2008        movs    r0, #8                                                              
20016410    4240        rsbs    r0, r0                                                              
20016412    4438        add     r0, r7                                                              
20016414    6800        ldr     r0, [r0, #0]                                                        
20016416    b401        push    {r0}                                                                
20016418    bc01        pop     {r0}                                                                
2001641a    4e2d        ldr     r6, [pc, #180]  ; 0x200164d0                                        
2001641c    47b0        blx     r6                                                                  
2001641e    b401        push    {r0}                                                                
20016420    482c        ldr     r0, [pc, #176]  ; 0x200164d4                                        
20016422    4601        mov     r1, r0                                                              
20016424    bc01        pop     {r0}                                                                
20016426    4a29        ldr     r2, [pc, #164]  ; 0x200164cc                                        
20016428    4790        blx     r2                                                                  
2001642a    4a2b        ldr     r2, [pc, #172]  ; 0x200164d8                                        
2001642c    4790        blx     r2                                                                  
2001642e    bc02        pop     {r1}                                                                
20016430    1840        adds    r0, r0, r1                                                          
20016432    bc02        pop     {r1}                                                                
20016434    6008        str     r0, [r1, #0]                                                        
20016436    f000 f814   bl      20016462                                                            
2001643a    4828        ldr     r0, [pc, #160]  ; 0x200164dc                                        
2001643c    b401        push    {r0}                                                                
2001643e    2001        movs    r0, #1                                                              
20016440    4e27        ldr     r6, [pc, #156]  ; 0x200164e0                                        
20016442    47b0        blx     r6                                                                  
20016444    200c        movs    r0, #12         ; 0xc                                               
20016446    4240        rsbs    r0, r0                                                              
20016448    4438        add     r0, r7                                                              
2001644a    b401        push    {r0}                                                                
2001644c    6800        ldr     r0, [r0, #0]                                                        
2001644e    b401        push    {r0}                                                                
20016450    2001        movs    r0, #1                                                              
20016452    bc02        pop     {r1}                                                                
20016454    1a08        subs    r0, r1, r0                                                          
20016456    bc02        pop     {r1}                                                                
20016458    6008        str     r0, [r1, #0]                                                        
2001645a    b401        push    {r0}                                                                
2001645c    2001        movs    r0, #1                                                              
2001645e    bc02        pop     {r1}                                                                
20016460    1840        adds    r0, r0, r1                                                          
20016462    200c        movs    r0, #12         ; 0xc                                               
20016464    4240        rsbs    r0, r0                                                              
20016466    4438        add     r0, r7                                                              
20016468    6800        ldr     r0, [r0, #0]                                                        
2001646a    b401        push    {r0}                                                                
2001646c    2000        movs    r0, #0                                                              
2001646e    bc02        pop     {r1}                                                                
20016470    4281        cmp     r1, r0                                                              
20016472    dc01        bgt     20016478                                                            
20016474    2000        movs    r0, #0                                                              
20016476    e000        b       2001647a                                                            
20016478    2001        movs    r0, #1                                                              
2001647a    2800        cmp     r0, #0                                                              
2001647c    d1dd        bne     2001643a                                                            
2001647e    4819        ldr     r0, [pc, #100]  ; 0x200164e4                                        
20016480    b401        push    {r0}                                                                
20016482    2001        movs    r0, #1                                                              
20016484    4e16        ldr     r6, [pc, #88]   ; 0x200164e0                                        
20016486    47b0        blx     r6                                                                  
20016488    4638        mov     r0, r7                                                              
2001648a    b401        push    {r0}                                                                
2001648c    6800        ldr     r0, [r0, #0]                                                        
2001648e    b401        push    {r0}                                                                
20016490    2004        movs    r0, #4                                                              
20016492    4240        rsbs    r0, r0                                                              
20016494    4438        add     r0, r7                                                              
20016496    6800        ldr     r0, [r0, #0]                                                        
20016498    bc02        pop     {r1}                                                                
2001649a    1840        adds    r0, r0, r1                                                          
2001649c    bc02        pop     {r1}                                                                
2001649e    6008        str     r0, [r1, #0]                                                        
200164a0    4638        mov     r0, r7                                                              
200164a2    6800        ldr     r0, [r0, #0]                                                        
200164a4    b401        push    {r0}                                                                
200164a6    4810        ldr     r0, [pc, #64]   ; 0x200164e8                                        
200164a8    bc02        pop     {r1}                                                                
200164aa    4281        cmp     r1, r0                                                              
200164ac    dd01        ble     200164b2                                                            
200164ae    2000        movs    r0, #0                                                              
200164b0    e000        b       200164b4                                                            
200164b2    2001        movs    r0, #1                                                              
200164b4    2800        cmp     r0, #0                                                              
200164b6    d194        bne     200163e2                                                            
200164b8    2000        movs    r0, #0                                                              
200164ba    46bd        mov     sp, r7                                                              
200164bc    bdc0        pop     {r6, r7, pc}                                                        
200164be    46c0        mov     r8, r8                                                              
200164c0    46bd        mov     sp, r7                                                              
200164c2    bdc0        pop     {r6, r7, pc}                                                        
200164c4    1002 24e1   .word   0x100224e1      ; aeabi_i2f                                         
200164c8    3c8e fa35   .word   0x3c8efa35                                                          
200164cc    1002 241b   .word   0x1002241b      ; aeabi_fmul                                        
200164d0    1002 25f9   .word   0x100225f9      ; sinf                                              
200164d4    41c8 0000   .word   0x41c80000                                                          
200164d8    1002 2525   .word   0x10022525      ; aeabi_f2iz                                        
200164dc    2000 d4e8   .word   0x2000d4e8                                                          
200164e0    1000 b125   .word   0x1000b125      ; printf                                            
200164e4    2000 d4ec   .word   0x2000d4ec                                                          
200164e8    0000 0168   .word   0x00000168                                                          
200164ec    0000 0000   .word   0x00000000      ; "\0\0\0\0"                                        
14:                                                                                                 
                                                                                                    
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

