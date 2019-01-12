make clean
make
avrdude -cusbasp -pm328p -Uflash:w:ppm2spi.hex:i
pause:
