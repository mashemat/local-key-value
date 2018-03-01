num_processes=24
export ROCE=0
export APT=1
node_map=(0 1 2 3)
cpu_map=(0 8 16 24 1 9 17 25 2 10 18 26 3 11 19 27 4 12 20 28 5 13 21 29 6 14 22 30 7 15 23 31) 


hi=`expr $num_processes - 1`
for i in `seq 0 $hi`; do
	id=`expr $@ \* $hi + $i`
	echo "Running client id $id"

	if [ $APT -eq 1 ]
	then
	     val=$(($i%4)) 
         val1=$(($i%32))	
		 numactl -N ${node_map[val]} -m ${node_map[val]} -C  ${cpu_map[val1]} stdbuf -o0 ./main $id < servers & #>client-tput/client-$id &
		#sudo -E ./main $id < servers &
	else
		if [ $ROCE -eq 1 ]
		then
			core=`expr 0 + $id`
			 numactl --physcpubind $core --interleave 0,1 ./main $id < servers &
		else
			core=`expr 32 + $id`
			numactl --physcpubind $core --interleave 4,5 ./main $id < servers &
		fi
	fi
	
	sleep .1
done

# When we run this script remotely, the client processes die when this script dies
# So, sleep.
