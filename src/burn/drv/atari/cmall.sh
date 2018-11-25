#!/bin/bash
# compile everything!
shopt -s nullglob
for f in *.cpp
do
	
	echo "file $f"
        echo "cm drv/atari/"${f%.*} >> ~/cm_atari.sh
done
