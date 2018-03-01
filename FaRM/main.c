#include "common.h"
//#include "hopscotch.h"
#include <signal.h>
#include <stdint.h>


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

void post_read(struct ctrl_blk *cb, int qpn, 
	volatile int *local_addr, int local_key, 
	uint64_t remote_addr, int remote_key, int size)
{
	cb->sgl.addr = (uintptr_t) local_addr;
	cb->sgl.lkey = local_key;
		
	cb->wr.opcode = IBV_WR_RDMA_READ;
	cb->wr.send_flags = 0;
	cb->wr.send_flags = IBV_SEND_SIGNALED;

	cb->wr.sg_list = &cb->sgl;
	cb->wr.sg_list->length = size;
	cb->wr.num_sge = 1;

	cb->wr.wr.rdma.remote_addr = remote_addr;
	cb->wr.wr.rdma.rkey = remote_key;

	
	int ret = ibv_post_send(cb->qp[qpn], &cb->wr, &bad_send_wr);

	
if(ret){	fprintf(stderr, " Error %d \n", ret); fflush(stdout);}
	
	CPE(ret, "ibv_post_send error", ret);

}

void post_write(struct ctrl_blk *cb, int qpn, 
	volatile struct request_s *local_addr, int local_key, 
	uint64_t remote_addr, int remote_key, int size)
{
	cb->sgl.addr = (uintptr_t) local_addr;
	cb->sgl.lkey = local_key;
	
	cb->wr.opcode = IBV_WR_RDMA_WRITE;
	
	cb->wr.send_flags = IBV_SEND_INLINE;
	cb->wr.send_flags |= IBV_SEND_SIGNALED;

	cb->wr.sg_list = &cb->sgl;
	cb->wr.sg_list->length = size;
	cb->wr.num_sge = 1;

	cb->wr.wr.rdma.remote_addr = remote_addr;
	cb->wr.wr.rdma.rkey = remote_key;
	

	int ret = ibv_post_send(cb->qp[qpn], &cb->wr, &bad_send_wr);

	CPE(ret, "ibv_post_send error", ret);
}

void run_server(struct ctrl_blk *cb)
{
    /*  I should add sth*/
	int32_t i=-1;	
				
	while(1){

	  i = (i+1) % NUM_CLIENTS_PER_SERVER;  // Loop among client request (There is not key 0)
	  if( server_req_area[i].key){ 	    	// If there is new request from client i    
		int32_t k=server_req_area[i].key;	
		int32_t j;
		j = hopscotch_update(hhead, k,k);		// Update
		if(!j){printf(" Not found %d\n",k); fflush(stdout); }
		server_req_area[i].key=1;  // send ack to the client 
		post_write(cb, i, 
		server_req_area, server_req_area_mr->lkey, 
		client_req_area_stag[i].buf, client_req_area_stag[i].rkey,
		  sizeof(struct request_s));	
        server_req_area[i].key=0;  // for next request
        poll_cq(cb->cq[i], 1); 
   	  }
	  
	}
	
}

void run_client(struct ctrl_blk *cb)
{
	
	int32_t random_key[4000000];		
	FILE *fp;
    int i,counter=0;
   
      /* open the file  for different distributions  {Zipf,uniform}
		zipf_random_numbers.txt
		uniform_random_numbers.txt
	  */   
    
   fp = fopen("uniform_random_numbers.txt", "r"); 
      if (fp == NULL) {
         printf("I couldn't open distribution file for reading.\n");
         exit(0);
      }


      while (fscanf(fp, "%d,", &i) == 1){  
		  random_key[counter] = i;				  
		  counter++;	  
	  }
	        fclose(fp);
			
  printf("Loading %d is finished ready to Connect\n", counter );
  fflush(stdout);
	
	
	
	struct timespec start;
//	uint64_t fastrand_seed = 0xdeadbeef;

	int iter = 0;
	int num_req = 0; //The number of PUT/GET requests

	int ws_phase[WINDOW_SIZE];		//Status of the window slot
	memset(ws_phase, 0, WINDOW_SIZE * sizeof(int32_t));
		
	int sn = cb->id/NUM_CLIENTS_PER_SERVER;

	clock_gettime(CLOCK_REALTIME, &start);

	int iteration=0;

	
	for(iter = 0; execution_status; iter++) {
		
		int iter_ = iter % WINDOW_SIZE;

		// The indicator of performance is not num_req. It is iteration, because
		// of warmup.

			if (start_counting){
		          iteration++;	
           	 }
				num_req ++;


			//Code for posting a new request
			if(rand() % 100 < PUT_PERCENT) {	//New request is PUT    

				 client_req_area->key =  random_key[num_req%4000000] ;
				post_write(cb, sn, 
					client_req_area, client_req_area_mr->lkey, 
					server_req_area_stag[sn].buf + ( cb->id % NUM_CLIENTS_PER_SERVER ) * sizeof(struct request_s) , server_req_area_stag[sn].rkey,  //+ (cb->id%NUM_CLIENTS_PER_SERVER) * sizeof(int)
					  sizeof(struct request_s) );   
					 poll_cq(cb->cq[sn], 1); 
				client_resp_area[0]=0;
				while (!client_resp_area){};	// Sustain-polling for ack								
				
			} else {							// New request is GET
				   						
						post_read(cb, sn, 
							client_resp_area, client_resp_area_mr->lkey, 
							server_pointer_hash_area_stag[sn].buf + sizeof(struct hopscotch_bucket) * random_key[num_req%4000000]   /*server_req_area_stag[sn].buf*/, server_pointer_hash_area_stag[sn].rkey  /*server_req_area_stag[sn].rkey*/,
							 6 * sizeof(struct hopscotch_bucket) );  
						poll_cq(cb->cq[sn], 1);  						
/*						struct hopscotch_bucket cur_buck;
						int H;
						for(H=0;H<6;H++){                          
						  cur_buck = (struct hopscotch_bucket ) (client_resp_area + H * sizeof(struct hopscotch_bucket));
                      if(cur_buck.data==random_key[num_req%4000000])                          
						  if((!cur_buck.lock) && (cur_buck.header_version==cur_buck.c_ver))	
						    break; 				                        
                      }*/
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
	printf("Finished.........................");
	sleep(100);	
	return;
}


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

		ctx->local_qp_attrs = (struct qp_attr *) malloc(
			NUM_SERVERS * S_QPA);
		ctx->remote_qp_attrs = (struct qp_attr *) malloc(
			NUM_SERVERS * S_QPA);
		
	} else {
		ctx->sock_port = atoi(argv[2]);
		ctx->num_conns = NUM_CLIENTS;

		ctx->local_qp_attrs = (struct qp_attr *) malloc(
			NUM_CLIENTS * S_QPA);
		ctx->remote_qp_attrs = (struct qp_attr *) malloc(
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
		ctx->local_qp_attrs[i].gid_global_interface_id = 
			my_gid.global.interface_id;
		ctx->local_qp_attrs[i].gid_global_subnet_prefix = 
			my_gid.global.subnet_prefix;

		ctx->local_qp_attrs[i].lid = get_local_lid(ctx->context);
		ctx->local_qp_attrs[i].qpn = ctx->qp[i]->qp_num;
		ctx->local_qp_attrs[i].psn = lrand48() & 0xffffff;
		fprintf(stderr, "Local address of RC QP %d: ", i);
		print_qp_attr(ctx->local_qp_attrs[i]);
	}

	if(ctx->is_client) {
		client_exch_dest(ctx);
	} else {
		server_exch_dest(ctx);
		int32_t  k,r;
	
		for( k=0 ; k <  (1<<hhead->exponent); k++) {  // 1<<hhead->exponent  Initialize Table         		
		r=hopscotch_insert(hhead,k,k);		
		if (r){
		printf("not inserteeeedd %d\n",k); fflush(stdout);}
		}
		}

	if (ctx->is_client) {
		for(i = 0; i < NUM_SERVERS; i++) {
			if(connect_ctx(ctx, ctx->local_qp_attrs[i].psn, 
				ctx->remote_qp_attrs[i], ctx->qp[i], 0)) {
				return 1;
			}
		}
	}

	if(ctx->is_client) {
	   signal (SIGALRM,start_count); // Signal to start counting
	   alarm(warmup);		
	   run_client(ctx);
	} else {
		run_server(ctx);
	}

	return 0;
}
