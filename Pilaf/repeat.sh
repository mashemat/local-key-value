a=$1  # num server
b=$2  # put percent
c=$3  # num clients per server
d=$4  # num clients
pr=`pgrep main`
kill -9 $pr

sed -i -e 's/#define NUM_SERVERS.*/#define NUM_SERVERS '$a'/' size.h
sed -i -e 's/#define PUT_PERCENT.*/#define PUT_PERCENT '$b'/' common.h
sed -i -e 's/#define NUM_CLIENTS\ .*/#define NUM_CLIENTS '$d'/' size.h
sed -i -e 's/num_processes=.*/num_processes='$d'/' run-machine.sh
sed -i -e 's/NUM_SERVERS=.*/NUM_SERVERS='$a'/' run-servers.sh
sed -i -e 's/#define NUM_CLIENTS_PER_SERVER\ .*/#define NUM_CLIENTS_PER_SERVER '$c'/' size.h
# Give time to Server...
make clean
make  main
#./run-machine.sh 0
#ssh -t mhemmatpour@compute-0-9 "cd HydraDB-server;  nohup ./run-server.sh & " 
