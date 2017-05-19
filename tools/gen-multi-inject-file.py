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

FILENAME="multi-inject-file."+str(TOTAL_ITEM)
fp=open(FILENAME, "w")

i=0

while i < TOTAL_ITEM:
    flag=[]
    memaddr=random.randint(0,MEMSIZE)
    err_bit_num=random.randint(1,8)
    for j in range(0, err_bit_num):
        index=random.randint(0,7)
        while index in flag:
            index=random.randint(0,7)
        flag.append(index)

    line=""+str(memaddr)+" "+str(err_bit_num)+"  "
    for j in range(0,8):
        k = 0
        while k < err_bit_num:
            if (j==flag[k]):
                line+="1"+" "
                break
            k+=1
        if (k==err_bit_num):
            line+="0"+" "

    line+=" "

    for j in range(0,8):
        k=0
        while k < err_bit_num:
            if j==flag[k]:
                value=random.randint(0,MEMSIZE)%2
                line+=str(value)+" "
                break
            k+=1
        if k==err_bit_num:
            line+="0"+" "

    line+="\n"
    fp.writelines(line)
    i+=1

fp.close()
