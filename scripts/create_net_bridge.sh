#!/bin/bash

# test if this pc has a network bridge named br0 and has a tap name tap0 then 
# use it. if not create. You should ask for outstream NIC name.

NIC="enp0s31f6"

brctl addbr br0
tunctl -t tap0 -u `whoami`

brctl addif br0 tap0 $NIC

ip link set up br0
ip link set up tap0

ip link set promisc on dev br0
ip link set promisc on dev tap0

