#!/bin/bash

# nthreads execution_time warmup Get_ratio
#for threads in `seq 4 4 32`
#do

node_map=(0 1 2 3)
node_map_up_socket0=(0 1)
node_map_up_socket1=(2 3)
node_map_up_socketmix=(0 2 1 3)
cpu_map=(0 8 16 24 1 9 17 25 2 10 18 26 3 11 19 27 4 12 20 28 5 13 21 29 6 14 22 30 7 15 23 31)
cpu_map_up_socket0=(0 9 2 11 4 13 6 15 1 8 3 10 5 12 7 14)
cpu_map_up_socket1=(16 25 18 27 20 29 22 31 17 24 19 26 21 28 23 30)
#cpu_map_up_mix=(0 16 9 25 1 17 10 26 2 18 11 27 3 19 12 28 4 20 13 29 5 21 14 30 6 22 15 31 7 23 8 24)
cpu_map_up_mix=(0 16 9 25 2 18 11 27 4 20 13 29 6 22 15 31 8 24 1 17 10 26 3 19 12 28 5 21 14 30 7 23)
hostname=(10.2.255.246 10.2.255.251 10.2.255.246 10.2.255.251 10.2.255.246 10.2.255.251 10.2.255.246 10.2.255.251)
hi=`expr $1 - 1`
for clients in `seq 0 $hi`
do
numactl -N ${node_map_up_socket0[clients%2]} -m ${node_map_up_socket0[clients%2]} -C  ${cpu_map_up_socket0[clients]} \
./redis_client ${hostname[clients]} 1 50 25 $2 &
done
