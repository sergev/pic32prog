#!/usr/bin/python
#
# Convert Microchip PE hex file to C data.
#
import sys, string

if len(sys.argv) != 2:
    print "Usage: %s file" % sys.argv[0]
    sys.exit (1)

addr0 = 0
nbytes = 0

def convert (addr, len, data):
    global addr0, nbytes
    if addr0 == 0:
        addr0 = addr
    addr -= addr0
    if nbytes != addr:
        print "*** non-continuous data! ***"
    nbytes = addr + len
    print "/*%04x*/" % addr,
    for i in range(0, len*2, 8):
        b0 = data[i:i+2]
        b1 = data[i+2:i+4]
        b2 = data[i+4:i+6]
        b3 = data[i+6:i+8]
        print "0x"+b3+b2+b1+b0+",",
    print

f = open (sys.argv[1])

for line in f.readlines():
    if line[0] != ':':
        continue
    len = int (line[1:3], 16)
    addr = int (line[3:7], 16)
    type = int (line[7:9], 16)
    #print len, type, line

    if type == 0:
        convert (addr, len, line[9:9+len*2])

print "#define PIC32_PE_NWORDS %d" % ((nbytes + 3) / 4)
