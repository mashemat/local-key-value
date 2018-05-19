# In-memory key-value Stores  (2017)
Experimental analysis of state-of-the-art RDMA-based in-memory key-value stores. 
## Hardware Requirment
- InfiniBand HCAs (Code tested on Mellanox ConnectX DDR 20 Gbps PCIe2)
- X86 Processor
##  Software Requirment
- RDMA drivers (Mellanox OFED or upstream OFED)
- Memcached 1.4.37
- Redis 3.2.9
- gcc version 4.4.7
##  Run scripts
###### RDMA-based systems
All RDMA-based systems contain two scripts `./run-servers.sh` and `./run-machine.sh`. Server shoud run 
`./run-servers.sh` and client runs `./run-machine.sh`. In the `./run-machine.sh`, *num_processes* specifies the number of clients.
###### Redis
- Download Redis from (www.redis.io), then compile and run Redis server 
- find out the InfiniBand IP address (*server_ip_address*) of server and initial the server keys by running  `./run-initial.sh   server_ip_address` from client
- ports are chosen incrementally started from the default Redis listening port
- now the server is initialized 
- It is assumed that system has 4 NUMA nodes with 32 cores. You can modify this by *node_map* and *cpu_map* in the `run-client.sh` script
- Run the script by `./run-client.sh  number_of_clients test_time warmup ratio` to run the experiment 
###### Memcached
- You need to download memcached from (http://memcached.org), then compile and run   
- find out the InfiniBand IP address (*server_ip_address*) of server and initial the server keys by running  `./run-initial.sh   server_ip_address` from client
- Run the script by `./run-client.sh  server_ip_address number_of_clients`
## Contact :e-mail:
Masoud Hemmatpour (mashemat@gmail.com).
