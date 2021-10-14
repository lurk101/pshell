A tiny pico lfs manager shell. Example log.
```sh
connected on UART

lfstool  Copyright (C) 1883 Thomas Edison
This program comes with ABSOLUTELY NO WARRANTY.
This is free software, and you are welcome to redistribute it
under certain conditions. See LICENSE.md for details.

enter command, hit return for help
/: 

     cd - change directory
 format - format and mount the filesystem
    get - get file
     ls - list directory
  mkdir - create directory
  mount - mount filesystem
    put - put file
      q - quit
     rm - remove file or directory
 status - filesystem status
unmount - unmount filesystem

/: ls
ls: filesystem is not mounted
/: format
are you sure (y/N) ? y
format: ok
/: ls
ls: filesystem is not mounted
/: mount
mount: ok
/: ls

   size name

ls: ok
/: mkdir text
mkdir: ok
/: cd test
cd: not a directory
/: cd text
cd: ok
/text: ls

   size name

ls: ok
/text: put p102_triangles.txt
C                                                                                                                      
file transfered, size: 26496
put: ok
/text: ls

   size name
  26496 p102_triangles.txt

ls: ok
/text: cd ..
cd: ok
/: ls

   size name
      0 [text]

ls: ok
/: put readme
CCsfered, size: 1536                                                                                                   
put: ok
/: ls

   size name
      0 [text]
   1536 readme

ls: ok
/: status

flash base 0x00100000, blocks 256, block size 4096, used 12, total 1048576 bytes, 4.7% used.

status: ok
/: rm text
rm: Can't remove file or directory
/: cd text
cd: ok
/text: ls

   size name
  26496 p102_triangles.txt

ls: ok
/text: rm p102_triangles.txt
rm: ok
/text: ls

   size name

ls: ok
/text: cd ..
cd: ok
/: ls

   size name
      0 [text]
   1536 readme

ls: ok
/: rm text
rm: ok
/: ls

   size name
   1536 readme

ls: ok
/: mount  
mount: filesystem already mounted
    
/: unmount
unmount: ok
/: ls
ls: filesystem is not mounted
/: mount
mount: ok
/: ls

   size name
   1536 readme

ls: ok
/: q
done
```
