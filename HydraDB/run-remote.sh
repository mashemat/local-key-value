num_machines=2
mc_hi=`expr $num_machines - 1`
for mc_num in `seq 0 $mc_hi`; do
	mc_name=anuj`expr $mc_num + 2`
	echo "mc_name = $mc_name"
	ssh $mc_name.RDMA.fawn.susitna.pdl.cmu.local "cd pingpong; ./run-machine.sh $mc_num < servers" > out &
	sleep 1
done
