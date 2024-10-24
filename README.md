# SF100Linux
Linux software for Dediprog SF100 and SF600 SPI flash programmers

## Building
To compile the project, first install required dependencies:
  - libusb-1.0
  - pkg-config - won't link to libusb without this package

Change to the directory where the sources are located and build using make:
```bash
$ cd SF100Linux
$ make
```

The resulting binary should be called `dpcmd` and located in the root of the
source tree. There is no install target at the moment.

## Usage
Most basic usage is writing a whole image file to a flash chip:
```bash
$ ./dpcmd --auto image.bin --verify
```

This will automatically detect the chip, read out the chip contents, replace
the ones that differ and perform a read and verification after writing.

For more advanced usage see `dpcmd --help`.
