#include "common.h"
#include "cucko.h"
#include "ccrand.h"

#include <signal.h>	
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHMSZ   4

int32_t start_counting=0;
int32_t execution_status=1;
int32_t testtime=50;
int32_t warmup=25;

void start_count(int sig)
{
  static int iter = 1;
  if (iter){
	printf("1\n");
	fflush(stdout);
	start_counting = 1;
	iter=0;
    alarm(testtime-warmup);
  }
  else{
	execution_status = 0; 
	printf("0\n");
	fflush(stdout);
  }	
} 
struct ibv_send_wr *bad_send_wr;
struct ibv_wc wc[Q_DEPTH];

static struct ctrl_blk *init_ctx(struct ctrl_blk *ctx, 
	struct ibv_device *ib_dev)
{
	ctx->context = ibv_open_device(ib_dev);
	CPE(!ctx->context, "Couldn't get context", 0);

	ctx->pd = ibv_alloc_pd(ctx->context);
	CPE(!ctx->pd, "Couldn't allocate PD", 0);

	create_qp(ctx);
	modify_qp_to_init(ctx);

	return ctx;
}

// RECVs are posted for PUTs. The server's RECVs are PILAF_ITEM_SIZE in size,
// the client's RECVs are PILAF_TINY_SIZE in size
void post_recvs(struct ctrl_blk *cb, int num_recvs, int qpn,
	volatile int *local_addr, int local_key, int size)
{
	int ret;
	struct ibv_sge list = {
		.addr	= (uintptr_t) local_addr,
		.length = size,
		.lkey	= local_key,
	};
	struct ibv_recv_wr wr = {
		.sg_list    = &list,
		.num_sge    = 1,
	};
	struct ibv_recv_wr *bad_wr;
	int i;
	for (i = 0; i < num_recvs; i++) {
		ret = ibv_post_recv(cb->uc_qp[qpn], &wr, &bad_wr);
		CPE(ret, "Error posting recv\n", ret);
	}
}

// A Pilaf read can be of two sizes: a 32 byte read from the hash
// table, or a PILAF_ITEM_SIZE read from the extents
void post_read(struct ctrl_blk *cb, int iter, int qpn, 
	volatile fix_cucko_bucket *local_addr, int local_key, 
	uint64_t remote_addr, int remote_key, int signal, int size)
{
	cb->sgl.addr = (uintptr_t) local_addr;
	cb->sgl.lkey = local_key;
		
	cb->wr.opcode = IBV_WR_RDMA_READ;
	cb->wr.send_flags = 0;
	if(signal) {
		cb->wr.send_flags = IBV_SEND_SIGNALED;
	}

	cb->wr.sg_list = &cb->sgl;
	cb->wr.sg_list->length = size;
	cb->wr.num_sge = 1;

	cb->wr.wr.rdma.remote_addr = remote_addr;
	cb->wr.wr.rdma.rkey = remote_key;
	
	int ret = ibv_post_send(cb->rc_qp[qpn], &cb->wr, &bad_send_wr);
	CPE(ret, "ibv_post_send error", ret);
}

// SENDs are used for PUTs, and are therefore PILAF_ITEM_SIZE in size
void post_send(struct ctrl_blk *cb, int qpn, 
	volatile int *local_addr, int local_key, int signal, int size)
{
	cb->sgl.addr = (uintptr_t) local_addr;
	cb->sgl.lkey = local_key;
	
	cb->wr.opcode = IBV_WR_SEND;
	
	cb->wr.send_flags = IBV_SEND_INLINE;
	if(signal) {
		cb->wr.send_flags |= IBV_SEND_SIGNALED;
	}

	cb->wr.sg_list = &cb->sgl;
	cb->wr.sg_list->length = size;
	cb->wr.num_sge = 1;
	
	int ret = ibv_post_send(cb->uc_qp[qpn], &cb->wr, &bad_send_wr);
	CPE(ret, "ibv_post_send error", ret);
}

void run_server(struct ctrl_blk *cb)
{


	int tot_sends[NUM_CLIENTS] = {0}, tot_recvs[NUM_CLIENTS] = {0};
	int tot_sends_all_clients = 0;

	// Each server handles PUTs for a range of clients
	int cn_lo = NUM_CLIENTS_PER_SERVER * cb->id;
	int cn_hi = NUM_CLIENTS_PER_SERVER * (cb->id + 1) - 1;
	
	struct timespec start, end; 
 
	int cn , i;
	int64_t iter =0;
	// Post RECVs for all clients that this server is assigned to
	for(cn = cn_lo; cn <= cn_hi; cn ++) {
		post_recvs(cb, Q_DEPTH, cn,  (int *) server_request_area, server_request_area_mr->lkey, sizeof(struct request_s));
		tot_recvs[cn] += Q_DEPTH;
	}

//    poll_cq(cb->RECV_cq[cn], 1);
	//Alert the clients
	//By this time, all clients have connected. Allow them post to recvs
	sleep(10); 
	for(cn = cn_lo; cn <= cn_hi; cn ++) {
		post_send(cb, cn, (int *)server_request_area, server_request_area_mr->lkey, 1, sizeof(struct request_s));
		tot_sends[cn] += 1;
	}
    printf("Here..");
	fflush(stdout);
	clock_gettime(CLOCK_REALTIME, &start);
	volatile int num_polls;
	while(1) {

		iter++; //= iter++%NUM_CLIENTS_PER_SERVER;
		cn = cn_lo + iter % NUM_CLIENTS_PER_SERVER;
		num_polls = ibv_poll_cq(cb->RECV_cq[cn], Q_DEPTH, wc);
		if(num_polls == 0) {
	//		printf("Yesssssss\n");
//fflush(stdout);
			continue;
		}
		tot_recvs[cn] -= num_polls;
		if(tot_recvs[cn] < S_DEPTH) {		//Batched posting of RECVs
			post_recvs(cb, Q_DEPTH - tot_recvs[cn], cn, (int *) server_request_area, 
				server_request_area_mr->lkey, sizeof(struct request_s));
			tot_recvs[cn] = Q_DEPTH;            
		}
        //long j = NUM_SERVERS;  
		
		// The server replies with a very small SEND
		for(i = 0; i < num_polls; i++) {

            //int lookup_place_cost = random0(&j)*100	;	
            int lookup_place_cost=server_request_area->key;		
			if((tot_sends[cn] & WS_SERVER_) == 0) {	// Poll and post signalled SEND
				poll_cq(cb->SEND_cq[cn], 1);	
//				printf("server request area:%d \n", server_request_area->key);
//				fflush(stdout);
				place(lookup_place_cost,1,0,0,3);
				//crc64(0,(unsigned const char *)item,32);
				post_send(cb, cn, (int *) server_req_area, server_req_area_mr->lkey, 1, sizeof(struct request_s));
			} else {				// Post unsignalled SEND
				//crc64(0,(unsigned const char *)item,32);
//				printf("server request area:%d \n", server_request_area->key);
//				fflush(stdout);

				place(lookup_place_cost,1,0,0,3);
				post_send(cb, cn, (int *) server_req_area, server_req_area_mr->lkey, 0, sizeof(struct request_s));
			}
			tot_sends[cn] ++;
			tot_sends_all_clients ++;
			
			if(tot_sends_all_clients % M_1 == 0) {	
				clock_gettime(CLOCK_REALTIME, &end);
				double seconds = (end.tv_sec - start.tv_sec) + 
					(double) (end.tv_nsec - start.tv_nsec) / 1000000000;
				fprintf(stderr, "Server %d RECVs OPS = %f\n", 
					cb->id, M_1 / seconds);
				clock_gettime(CLOCK_REALTIME, &start);
			}
		}
	}
}

void run_client(struct ctrl_blk *cb)
{


	struct timespec start; //, end;		//Sampled once every ITERS_PER_MEASUREMENT
	uint64_t fastrand_seed = 0xdeadbeef;

	int iter = 0;
	int num_req = 0; // old_num_req = -1;	//The number of PUT/GET requests

	int num_recvs = 0, num_sends = 0, num_reads = 0;
	int old_num_reads = 0, old_num_sends = 0;

	int send_signal=0, read_signal=0;

	int ws_phase[WINDOW_SIZE];		//Status of the window slot
	memset(ws_phase, 0, WINDOW_SIZE * sizeof(int));
	
	struct timespec op_start[WINDOW_SIZE], op_end[WINDOW_SIZE];
	
	// Each client only accesses one server: just for simplicity
	int sn = cb->id / NUM_CLIENTS_PER_SERVER;

	clock_gettime(CLOCK_REALTIME, &start);
	long long total_nsec = 0;

/*	post_recvs(cb, Q_DEPTH, sn,(int *) &client_resp_area[0], 
		client_resp_area_mr->lkey, sizeof(fix_cucko_bucket));*/

		
			post_recvs(cb, Q_DEPTH, sn,(int *) client_req_area, 
		client_req_area_mr->lkey, sizeof(struct request_s));
 

	//Wait for the server's message
//	printf("Befire wait..  %d \n",cb->id);
//	fflush(stdout);
//	int pool=0;
//	while(pool<2){pool=ibv_poll_cq(cb->RECV_cq[sn], Q_DEPTH, wc); /*printf("pool:%d-sn:%d\n",pool,sn); fflush(stdout);*/};	
	poll_cq(cb->RECV_cq[sn], 1);
	num_recvs = Q_DEPTH - 1;
//	printf("after wait.. %d \n",cb->id);
//	fflush(stdout);

  int  random_key[4000000];
// here
  FILE *fp;
  int i,counter=0;
//  int  random_key[4000000];
  char workload[21];
//    sprintf(workload,"zip_random_numbers%d.txt",cb->id);
   sprintf(workload,"zip_random_numbers.txt"); 

   fp = fopen(workload, "r"); 
      if (fp == NULL) {
         printf("IIIIIIIIIIIIIII couldn't open distribution file for reading........................\n");
		 fflush(stdout);
         exit(0);
      }
 
 // printf("Loading distribution to test%d\n",cb->id);
 // fflush(stdout); 

  while (fscanf(fp, "%d,", &i) == 1){  
		  random_key[counter] = i;				  
		  counter++;	  
	  }
	        fclose(fp);
  
 //end 
  
printf("Loaded workload %d\n",counter);
fflush(stdout);


 /* 


if (!cb->id){

    printf("I ma heree*********************************Node 00\n");
	fflush(stdout);
    int shmid;
    key_t key;
    int32_t * shm;

    // We'll name our shared memory segment
      key = 5678;

    
    if ((shmid = shmget(key, SHMSZ, IPC_CREAT | 0666)) < 0) {
        printf("Heyyyyyy Vayyyyyyyyy*****************************************\n");
		fflush(stdout);
		sleep(1);
        exit(1);
    }

 
    if ((shm = (int32_t *) shmat(shmid, NULL, 0)) == (int32_t *) -1) {
        printf("Heyyyyyy Vayyyyyyyyy**************************************\n");
		fflush(stdout);
	    sleep(1);
		exit(1);
    }

	
	
	    FILE *fp;
    int i,counter=0;
    
  char workload[21];

     sprintf(workload,"zip_random_numbers.txt");


   fp = fopen(workload, "r"); 
      if (fp == NULL) {
         printf("IIIIIIIIIIIIIII couldn't open distribution file for reading........................\n");
		 fflush(stdout);
         exit(0);
      }
 
//  printf("Loading distribution to test\n");
 // fflush(stdout); 

  while (fscanf(fp, "%d,", &i) == 1){  
		  random_key[counter] = i;				  
		  counter++;	  
	  }
	        fclose(fp);
			
  //printf("Loading %d is finished ready to Connect\n", counter );
  //fflush(stdout);
	 
	
	
	 int32_t turn;
	 memset(shm, 0, 4);
     turn = (int32_t) * shm;	 
//	 while (turn!=cb->id){}
	 __sync_fetch_and_add( shm, (cb->id)+1 );
	 printf("It is added **************************%d---%d\n", turn, (cb->id) );
	 fflush(stdout);
	// sleep(0.5);
	 while (turn!=(NUM_CLIENTS)){turn =  (int32_t)* shm; 	  
//	  printf("It is added %d---%d\n", turn, (cb->id) );
//	  fflush(stdout);	  
	  sleep(0.01);}
 printf("Start  %d\n",cb->id);
	 fflush(stdout);
	// sleep(1);
	  }
else{

	int shmid;
    key_t key;
    int32_t *shm;

    key = 5678;

    if ((shmid = shmget(key, SHMSZ, 0666)) < 0) {
        printf("Heyyyyyy Vayyyyyyyyy****************************\n");
		fflush(stdout);
        exit(1);
    }

    if ((shm = (int32_t *) shmat(shmid, NULL, 0)) == (int32_t *) -1) {
        printf("Heyyyyyy Vayyyyyyyyy**********************************\n");
		fflush(stdout);
        exit(1);
    }


	 
	 
	 int32_t turn;
     turn =  * shm;	 
	 while (turn!=cb->id){turn =  *shm; sleep(0.01);} // becarefull about the sleep time
	 
	 
	 
	 
  FILE *fp;
  int i,counter=0;
//  int  random_key[4000000];
  char workload[21];
//    sprintf(workload,"zip_random_numbers%d.txt",cb->id);
   sprintf(workload,"zip_random_numbers.txt"); 

   fp = fopen(workload, "r"); 
      if (fp == NULL) {
         printf("IIIIIIIIIIIIIII couldn't open distribution file for reading........................\n");
		 fflush(stdout);
         exit(0);
      }
 
 // printf("Loading distribution to test%d\n",cb->id);
 // fflush(stdout); 

  while (fscanf(fp, "%d,", &i) == 1){  
		  random_key[counter] = i;				  
		  counter++;	  
	  }
	        fclose(fp);
			
 // printf("Loading %d is finished ready to Connect\n", counter );
 // fflush(stdout);
	 

	 
	__sync_fetch_and_add(shm, 1 );

	 
	 
	while (turn!=(NUM_CLIENTS)){turn =  * shm; sleep(0.01);} // becarefulll about sleep time
	  
	  	 printf("Start %d\n",cb->id);
	 fflush(stdout);
//	 float w= cb->id / 10;
	// sleep(1);
}	
	
 */	

    int iteration=0;

	for(iter = 0;execution_status; iter++) {
		
	//	printf("iteration%d\n",iteration);
	//	fflush(stdout);
		int iter_ = iter % WINDOW_SIZE;

		// The indicator of performance is not iter. It is num_req, because
		// a request will be completed in multiple iterations.
		//if(num_req % K_256 == 0 && num_req != 0 && num_req != old_num_req) {
		//	clock_gettime(CLOCK_REALTIME, &end);
			//fprintf(stderr, "Client %d completed %d ops\n", cb->id, num_req);
			//double seconds = (end.tv_sec - start.tv_sec) +
			//	(double) (end.tv_nsec - start.tv_nsec) / 1000000000;
			
			//fprintf(stderr, "Client %d OPS = %f, latency = %f us \n", 
			//	cb->id, K_256 / seconds, (double) (total_nsec / 1000) / K_256);
			//fprintf(stdout, "%f\n", K_256 / seconds);
			//fflush(stdout);
			//fflush(stderr);

		//%	clock_gettime(CLOCK_REALTIME, &start);
		//%	total_nsec = 0;
		//	old_num_req = num_req;
		//}
		
		if(num_recvs < S_DEPTH) {		//Batched posting of RECVs
			post_recvs(cb, Q_DEPTH - num_recvs, sn, (int *) client_req_area, 
				client_req_area_mr->lkey, sizeof(struct request_s));
			num_recvs = Q_DEPTH;
		}

		send_signal = 0;
		read_signal = 0;

		if((num_reads & WINDOW_SIZE_) == 0) {
			read_signal = 1;
			if(num_reads != 0 && num_reads != old_num_reads) {
				poll_cq(cb->READ_cq[sn], 1);
				old_num_reads = num_reads;
			}
		}
		if((num_sends & WINDOW_SIZE_) == 0) {
			send_signal = 1;
			if(num_sends != 0 && num_sends != old_num_sends) {
				poll_cq(cb->SEND_cq[sn], 1);
				old_num_sends = num_sends;
			}
		}

		//	printf("read\n");
		//	fflush(stdout);
		int pp = ws_phase[iter_];			//Previous Phase

		if(pp == SEND_1) {		//Complete the previous phase
			poll_cq(cb->RECV_cq[sn], 1);
			num_recvs --;
		} else if(pp == READ_1 || pp == READ_2) {
			while(client_resp_area->offset == 0) {

			//do nothing			
			}
//			fix_cucko_bucket * a = (fix_cucko_bucket*) client_resp_area;
//          printf("client_resp_area->key:%" PRIu64 "\n",a->key);
//		  fflush(stdout);
		} else if(pp == READ_3) {
			while(client_resp_area->offset == 0) {
				//do nothing

				}
 //         printf("client_resp_area->key:%d\n",client_resp_area->offset);
//		  fflush(stdout);
		}

		//If the previous phase was a final phase, issue a new request
		if(pp == READ_3 || pp == SEND_1 || pp == 0) {

			if(pp != 0) {		//If the previous phase was a request completion
				clock_gettime(CLOCK_REALTIME, &op_end[iter_]);
				long long new_nsec = (op_end[iter_].tv_sec - op_start[iter_].tv_sec)* 1000000000 
					+ (op_end[iter_].tv_nsec - op_start[iter_].tv_nsec);
				total_nsec += new_nsec;
				if((fastrand(&fastrand_seed) & 0x1ff) == 0 && CLIENT_PRINT_LAT == 1) {
					printf("%lld\n", new_nsec);
				}
				if(start_counting)
					iteration++;
				num_req ++;
			}

			clock_gettime(CLOCK_REALTIME, &op_start[iter_]);

			//Code for posting a new request
			if(rand() % 100 < PUT_PERCENT) {	//New request is PUT

				ws_phase[iter_] = SEND_1;
				struct  request_s req;
				req.key = random_key[num_req%4000000];
				//req.val = (char )10;
                * client_req_area = req; // [iter_ * PILAF_MAX_SIZE]
//				printf("send:%d\n",client_req_area->key);
//				fflush(stdout);

				post_send(cb, sn, (int *)client_req_area,  // [iter_ * PILAF_MAX_SIZE]
					client_req_area_mr->lkey, send_signal, sizeof(struct request_s));
				num_sends ++;
//								poll_cq(cb->SEND_cq[sn], 1);

				
			} else {				//New request is GET
				if(rand() % 100 < 60) {		//Need 2 READs
						//	printf("real....read\n");
			//fflush(stdout);
					ws_phase[iter_] = READ_1;
				} else {
					ws_phase[iter_] = READ_2;
				}
	
				//memset((fix_cucko_bucket *) client_resp_area, 0, sizeof(fix_cucko_bucket));
				client_resp_area->offset=0;
				client_resp_area->key=0;
	 			
				post_read(cb, iter, sn, 
					client_resp_area, 
					client_resp_area_mr->lkey,  
					server_req_area_stag[sn].buf +random_key[num_req%4000000]*sizeof(fix_cucko_bucket), server_req_area_stag[sn].rkey,
					read_signal, sizeof(fix_cucko_bucket));
				    num_reads ++;
	//				poll_cq(cb->READ_cq[sn], 1);
			} 
			//New Request posted
		} else if(pp == READ_1 || pp == READ_2) {
			ws_phase[iter_] = pp + 1;
			memset((fix_cucko_bucket *) client_resp_area, 0, sizeof(fix_cucko_bucket));				
			post_read(cb, iter, sn, 
				client_resp_area, client_resp_area_mr->lkey, 
				server_req_area_stag[sn].buf+random_key[num_req%4000000]*sizeof(fix_cucko_bucket), server_req_area_stag[sn].rkey,
				read_signal, pp == READ_1 ? sizeof(fix_cucko_bucket) : sizeof(fix_cucko_bucket));
			    num_reads ++;
//				poll_cq(cb->READ_cq[sn], 1);
		}
        
		}
	
	
	
	FILE *out1 = fopen("throughput.txt", "a");  
	FILE *out2 = fopen("latency.txt", "a");  
 printf("iteration:%d\n",iteration);
    fprintf(out1, "%d\n", iteration);  
    fprintf(out2, "%f\n", (double) (testtime-warmup) / iteration);  

    fclose(out1);
    fclose(out2);	
	
	printf("Result is printed to the file\n");
	fflush(stdout);
		
	return;
}

/* Usage:
 * Server: ./rc_pingpong <sock_port> <id>
 * Client: ./rc_pingpong <sock_port> <id> <server_ib0_ip>
 */ 
int main(int argc, char *argv[])
{
	int i;
	struct ibv_device **dev_list;
	struct ibv_device *ib_dev;
	struct ctrl_blk *ctx;

	srand48(getpid() * time(NULL));		//Required for PSN
	ctx = malloc(sizeof(struct ctrl_blk));
	
	ctx->id = atoi(argv[1]);

	if (argc == 2) {
		ctx->is_client = 1;
		ctx->num_conns = NUM_SERVERS;

		ctx->local_rc_qp_attrs = (struct qp_attr *) malloc(
			NUM_SERVERS * S_QPA);
		ctx->remote_rc_qp_attrs = (struct qp_attr *) malloc(
			NUM_SERVERS * S_QPA);

		ctx->local_uc_qp_attrs = (struct qp_attr *) malloc(
			NUM_SERVERS * S_QPA);
		ctx->remote_uc_qp_attrs = (struct qp_attr *) malloc(
			NUM_SERVERS * S_QPA);
		
	} else {
		
		
		
		ctx->sock_port = atoi(argv[2]);
		ctx->num_conns = NUM_CLIENTS;

		ctx->local_rc_qp_attrs = (struct qp_attr *) malloc(
			NUM_CLIENTS * S_QPA);
		ctx->remote_rc_qp_attrs = (struct qp_attr *) malloc(
			NUM_CLIENTS * S_QPA);

		ctx->local_uc_qp_attrs = (struct qp_attr *) malloc(
			NUM_CLIENTS * S_QPA);
		ctx->remote_uc_qp_attrs = (struct qp_attr *) malloc(
			NUM_CLIENTS * S_QPA);
	}
	
	dev_list = ibv_get_device_list(NULL);
	CPE(!dev_list, "Failed to get IB devices list", 0);

	ib_dev = dev_list[is_roce() == 1 ? 1 : 0];
	//ib_dev = dev_list[0];
	CPE(!ib_dev, "IB device not found", 0);

	init_ctx(ctx, ib_dev);
	CPE(!ctx, "Init ctx failed", 0);
	
	setup_buffers(ctx);

	union ibv_gid my_gid= get_gid(ctx->context);

	for(i = 0; i < ctx->num_conns; i++) {
		ctx->local_rc_qp_attrs[i].gid_global_interface_id = 
			my_gid.global.interface_id;
		ctx->local_rc_qp_attrs[i].gid_global_subnet_prefix = 
			my_gid.global.subnet_prefix;

		ctx->local_rc_qp_attrs[i].lid = get_local_lid(ctx->context);
		ctx->local_rc_qp_attrs[i].qpn = ctx->rc_qp[i]->qp_num;
		ctx->local_rc_qp_attrs[i].psn = lrand48() & 0xffffff;
		fprintf(stderr, "Local address of RC QP %d: ", i);
		print_qp_attr(ctx->local_rc_qp_attrs[i]);

		ctx->local_uc_qp_attrs[i].gid_global_interface_id = 
			my_gid.global.interface_id;
		ctx->local_uc_qp_attrs[i].gid_global_subnet_prefix = 
			my_gid.global.subnet_prefix;

		ctx->local_uc_qp_attrs[i].lid = get_local_lid(ctx->context);
		ctx->local_uc_qp_attrs[i].qpn = ctx->uc_qp[i]->qp_num;
		ctx->local_uc_qp_attrs[i].psn = lrand48() & 0xffffff;
		fprintf(stderr, "Local address of UC QP %d: ", i);
		print_qp_attr(ctx->local_uc_qp_attrs[i]);
	}

	if(ctx->is_client) {
		client_exch_dest(ctx);
	} else {
		server_exch_dest(ctx);			
	}

	if (ctx->is_client) {
		for(i = 0; i < NUM_SERVERS; i++) {
			if(connect_ctx(ctx, ctx->local_rc_qp_attrs[i].psn, 
				ctx->remote_rc_qp_attrs[i], ctx->rc_qp[i], 0)) {
				return 1;
			}

			if(connect_ctx(ctx, ctx->local_uc_qp_attrs[i].psn, 
				ctx->remote_uc_qp_attrs[i], ctx->uc_qp[i], 1)) {
				return 1;
			}
		}
	}

	if(ctx->is_client) {
		
						    signal (SIGALRM,start_count); // Signal to start counting
	alarm(warmup);	
		
		run_client(ctx);
	} else {
/*				int k;
		int64_t rkeys[NUM_KEYS];
		printf("It is going on....");
		fflush(stdout);
        for (k=0;k<NUM_KEYS;k++)		
		   rkeys[k]=k; 		
	    cuckoo(rkeys,rkeys,NUM_KEYS); */
		run_server(ctx);  
	}
     
	return 0;
}  
