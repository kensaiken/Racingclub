"""The music decoder as the game's sound mixer has it in SPU memory -> input of the SPU recompiler.

usage: python tools/atrac_image.py <dev_flash/sys/external/flashATRAC.pic> <out: re/spu_atrac_32980.bin>

flashATRAC.pic is a file of the PS3 system software (the user's own copy, e.g. from an RPCS3 installation). The mixer
loads its code (0x7d40 bytes from file offset 0x100) at local-store address 0x32980 and the decoder keeps four state
words at +0x53d0; the recompiled code is recognised at run time by the checksum of exactly this image.
Tested with the file of system software 4.92 (35131 bytes); another version still plays, but interpreted and slowly.
"""
import hashlib, struct, sys

KNOWN_SHA1 = 'ed2e46e9947c191e27b3b25a6be67ba1901a8810'


def main():
    pic = open(sys.argv[1], 'rb').read()
    if hashlib.sha1(pic).hexdigest() != KNOWN_SHA1:
        print('warning: this flashATRAC.pic is not the one of system software 4.92; music may run slowly (interpreted)')
    img = bytearray(pic[0x100:0x100 + 0x7d40])
    if len(img) != 0x7d40: sys.exit('flashATRAC.pic is too short')
    for o in range(0x53d0, 0x53e0, 4): struct.pack_into('>I', img, o, 0xf750)
    open(sys.argv[2], 'wb').write(img)
    print('music decoder image written (%d bytes)' % len(img))


if __name__ == '__main__':
    main()
