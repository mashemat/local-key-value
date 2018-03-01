export ROCE=0
export APT=1
NUM_SERVERS=1

node_map=(0 1 2 3)
node_map_up_socket0=(0 1)
node_map_up_socket1=(2 3)
node_map_up_socketmix=(0 2 1 3)
cpu_map=(0 8 16 24 1 9 17 25 2 10 18 26 3 11 19 27 4 12 20 28 5 13 21 29 6 14 22 30 7 15 23 31) 
cpu_map_up_socket0=(0 9 2 11 4 13 6 15 1 8 3 10 5 12 7 14)
cpu_map_up_socket1=(16 25 18 27 20 29 22 31 17 24 19 26 21 28 23 30)
#cpu_map_up_mix=(0 16 9 25 1 17 10 26 2 18 11 27 3 19 12 28 4 20 13 29 5 21 14 30 6 22 15 31 7 23 8 24)
cpu_map_up_mix=(0 16 9 25 2 18 11 27 4 20 13 29 6 22 15 31 8 24 1 17 10 26 3 19 12 28 5 21 14 30 7 23)

rm -rf client-tput
mkdir client-tput

for i in `seq 1 $NUM_SERVERS`; do
	id=`expr $i - 1`
	sock_port=`expr 5500 + $i - 1`

	if [ $APT -eq 1 ]
	then
		#./main $id $sock_port &
           if [ $1 -eq 0 ] 
           then
	    val=$(($id%2)) 
            val1=$(($id%16))
            echo "Socket OnEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE" echo ${node_map_up_socket0[val]}
	     numactl -N ${node_map_up_socket0[val]} -m ${node_map_up_socket0[val]} -C  ${cpu_map_up_socket0[val1]}  ./main $id $sock_port &
           else
	    val=$(($id%4)) 
            val1=$(($id%32))
            echo "Socket MiXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" echo  ${node_map_up_socketmix[val]} 
	    numactl -N ${node_map_up_socketmix[val]} -m ${node_map_up_socketmix[val]} -C  ${cpu_map_up_mix[val1]}  ./main $id $sock_port &
           fi
	else
		if [ $ROCE -eq 1 ]
		then
			core=`expr 0 + $id`
			 numactl --physcpubind $core --interleave 0,1 ./main $id $sock_port &
		else
			core=`expr 32 + $id`
			 numactl --physcpubind $core --interleave 4,5 ./main $id $sock_port &
		fi
	fi
	
	sleep .1
done
