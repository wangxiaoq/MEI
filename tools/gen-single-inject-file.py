#!/usr/bin/python

import random
import sys

if (len(sys.argv) <= 1):
        print "Usage: %s NUMBER_OF_ERRORS" % sys.argv[0]
        exit(0)

# you should modify this item to the number of errors
# you want.
TOTAL_ITEM=int(sys.argv[1])

MEMSIZE=1024*1024*1024*8
BYTESIZE=8

FILENAME="single-inject-file."+str(TOTAL_ITEM)
fp=open(FILENAME, "w")

i=0

while i < TOTAL_ITEM:
    memaddr=random.randint(0,MEMSIZE)
    index=random.randint(0,7)
    value=(memaddr%2)
    line=""+str(memaddr)+" "+"1"+"  "
    for j in range(0,8):
        if (j!=index):
            line+="0"+" "
        else:
            line+="1"+" "
    line+=" "
    for j in range(0,8):
        if (j!=index):
            line+="0"+" "
        else:
            line+=str(value)+" "
    line+="\n"
    fp.writelines(line)
    i+=1

fp.close()
