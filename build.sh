#!/bin/bash

kernel="hd.img"
hd="hd80M.img"

make clean-all && 
cp ../hd.img . &&
cp ../hd80M.img . &&

if [[ ! -f "$kernel" ]]; then
    echo "$kernel does not exist!"
    exit 1
fi

if [[ ! -f "$hd" ]]; then
    echo "$hd does not exist!"
    exit 1
fi

make mount && make run
