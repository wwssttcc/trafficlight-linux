#!/bin/bash
cp /mnt/hgfs/share/trafficlight-linux/app/* ./
make clean
make
sudo cp trafficlight.out ~/nfs/myir_hostapd/home/root/
