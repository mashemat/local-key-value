# In-memory key-value Stores  (2017)
Experimental analysis of state-of-the-art RDMA-based in-memory key-value stores. 
## Hardware Requirment
- Mellanox ConnectX DDR 20 Gbps PCIe2
- X86 Processor
##  Software Requirment
- RDMA drivers (Mellanox OFED or upstream OFED)
- Memcached 1.4.37
- Redis 3.2.9
- gcc version 4.4.7
##  Run scripts
###### RDMA-based system
- Server 
./run-servers.sh
- Client 
./run-machine.sh 
###### Redis
- You need to download (www.redis.io), compile, and run Redis server 
- modify the IP address in the `run-initial.sh` and put the server IP address of InfiniBand: *./run-initial.sh server_ip_address* 
- ports are chosen incrementally started from the default Redis listening port
- now the server is initialized by the key
- It is assumed you have 4 NUMA nodes with 32 cpu cores (AMD Opteron ). You can modify this by modifying *node_map* and *cpu_map* in the *.run-client.sh* script
- Run the script by *./run-client.sh  number_of_clients test_time warmup ratio*
###### Memcached
- You need to download (http://memcached.org), compile, and run Memcached  
- Modify the IP address in the *run-initial.sh* and put the server IP address of InfiniBand: *./run-initial.sh Server IP address* 
- Ports are chosen incrementally started from the default Memcached listening port
- It is assumed you have 4 NUMA nodes with 32 cpu cores (AMD Opteron ). You can modify in the code **
- Run the script by *./run-client.sh  server_ip_address number_of_clients*
## Contact
Masoud Hemmatpour (mashemat@gmail.com).
