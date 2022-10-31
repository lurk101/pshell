
[here]: https://github.com/lurk101/pshell/releases
[examples]: c-examples

Programming the Pico

There's a couple of ways to get the necessary binary files for
programming the Pico. You can build them yourself from this
repo, or you can download pre-compiled images [here]. The files
are provide for both UART and USB console modes. UF2 files can
be used the standard bootsel button USB way, or the ELF format
files for GDB users.

The pshell console

pshell provides a Unix-like command line interface via a serial
interface. This can be via UART or emulated over USB to a separately
hosted terminal program in VT100 mode. In Linux for example you
could connect via UART wih

```
minicom -c on -b 115200 -o -D /dev/serial0
```
or via USB with
```
minicom -c on -b 115200 -o -D /dev/ttyACM0
```
The screen command can also be used, but does not supoort xmodem
transfers.

Getting files to and from the Pico

Along the line you may need to transfer files back and forth
between your host and the Pico. You'd do this using the xmodem
protocol, supported by both the Pico and minicom.

The xput command intructs pshell to receive a file from the
host. The xget command does the opposite, it tells the Pico
to transmit a file to the host. xmodem transfers single files.
A tar command allows you create Linux compatible tarballs
with multiple files, which can then be transferred as a single
file.
