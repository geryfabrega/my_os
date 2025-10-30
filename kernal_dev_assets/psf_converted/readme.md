# Run ObjCopy on the .psf file, it also needs to be on .psf version 2

objcopy -O elf64-x86-64 -B i386 -I binary font.psf font.o
readelf -s font.o

# The output will be a file.o object. This needs to be included in the linking process of our binary.
## We will use the exposed headers to use this font. Running readelf -s font.o will expose contants that will allow
## You to read the object and place into the linear Frame buffer




