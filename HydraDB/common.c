#include "common.h"



union ibv_gid get_gid(struct ibv_context *context)
{
	union ibv_gid ret_gid;
	ibv_query_gid(context, IB_PHYS_PORT, 0, &ret_gid);

	fprintf(stderr, "GID: Interface id = %lld subnet prefix = %lld\n", 
		(long long) ret_gid.global.interface_id, 
		(long long) ret_gid.global.subnet_prefix);
	
	return ret_gid;
}

uint16_t get_local_lid(struct ibv_context *context)
{
	struct ibv_port_attr attr;

	if (ibv_query_port(context, IB_PHYS_PORT, &attr))
		return 0;

	return attr.lid;
}

int close_ctx(struct ctrl_blk *ctx)
{
	int i;
	for(i = 0; i < ctx->num_conns; i++) {
		if (ibv_destroy_qp(ctx->qp[i])) {
			fprintf(stderr, "Couldn't destroy connected QP\n");
			return 1;
		}
		if (ibv_destroy_cq(ctx->cq[i])) {
			fprintf(stderr, "Couldn't destroy connected CQ\n");
			return 1;
		}
	}
	
	if (ibv_dealloc_pd(ctx->pd)) {
		fprintf(stderr, "Couldn't deallocate PD\n");
		return 1;
	}

	if (ibv_close_device(ctx->context)) {
		fprintf(stderr, "Couldn't release context\n");
		return 1;
	}

	free(ctx);

	return 0;
}

void create_qp(struct ctrl_blk *ctx)
{
	int i;

	//Create connected queue pairs
	ctx->qp = malloc(sizeof(int *) * ctx->num_conns);
	ctx->cq = malloc(sizeof(int *) * ctx->num_conns);

	for(i = 0; i < ctx->num_conns; i++) {
		ctx->cq[i] = ibv_create_cq(ctx->context, 
			Q_DEPTH + 1, NULL, NULL, 0);
		CPE(!ctx->cq[i], "Couldn't create CQ", 0);

		struct ibv_qp_init_attr init_attr = {
			.send_cq = ctx->cq[i],
			.recv_cq = ctx->cq[i],
			.cap     = {
				.max_send_wr  = Q_DEPTH,
				.max_recv_wr  = Q_DEPTH,
				.max_send_sge = 1,
				.max_recv_sge = 1,
				.max_inline_data = 512  //256
			},
			.qp_type = IBV_QPT_RC
		};

		ctx->qp[i] = ibv_create_qp(ctx->pd, &init_attr);
		CPE(!ctx->qp[i], "Couldn't create connected QP", 0);
	}
}

void modify_qp_to_init(struct ctrl_blk *ctx)
{
	int i;

	struct ibv_qp_attr conn_attr = {
		.qp_state		= IBV_QPS_INIT,
		.pkey_index		= 0,
		.port_num		= IB_PHYS_PORT,
		.qp_access_flags= IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE
	};

	int init_flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT |
		IBV_QP_ACCESS_FLAGS;
	for(i = 0; i < ctx->num_conns; i++) {
		if (ibv_modify_qp(ctx->qp[i], &conn_attr, init_flags)) {
			fprintf(stderr, "Failed to modify QP to INIT\n");
			return;
		}
	}
}

int setup_buffers(struct ctrl_blk *cb)
{
	int FLAGS = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | 
				IBV_ACCESS_REMOTE_WRITE;
	if (cb->is_client) {
		// For reading from server. The maximum size that can be read is FARM_INLINE_READ_SIZE
		// because it's larger than FARM_VALUE_SIZE
		client_resp_area = memalign(4096,FARM_INLINE_READ_SIZE * WINDOW_SIZE); // FARM_INLINE_READ_SIZE * WINDOW_SIZE
        memset((int32_t*)client_resp_area , 0 , FARM_INLINE_READ_SIZE * WINDOW_SIZE ); //  (char *)  , 'a'
		client_resp_area_mr = ibv_reg_mr(cb->pd, 
			(int32_t*) client_resp_area ,FARM_INLINE_READ_SIZE * WINDOW_SIZE, FLAGS); // FARM_INLINE_READ_SIZE * WINDOW_SIZE
		//CPE(!client_resp_area_mr, 
			//"client_large_resp_area reg_mr failed", errno);

		// For WRITE-ing to server
		client_req_area = memalign(4096, sizeof(struct request_s) * WINDOW_SIZE ); // 
		memset((struct request_s*) client_req_area, 0 , sizeof(struct request_s) * WINDOW_SIZE ); //  (char *)  , 'a'
		client_req_area_mr = ibv_reg_mr(cb->pd, 
			(struct request_s*) client_req_area , sizeof(struct request_s) * WINDOW_SIZE , FLAGS); //   FARM_ITEM_SIZE
		//CPE(!client_req_area_mr, "client_resp_area reg_mr failed", errno);
	} else {

  	    server_req_area = memalign(4096,  REQ_AC * sizeof(struct request_s) ); // REQ_AC * FARM_INLINE_READ_SIZE
		memset((struct request_s*) server_req_area, 0 ,  REQ_AC * sizeof(struct request_s) ); // REQ_AC * FARM_INLINE_READ_SIZE
		server_req_area_mr = ibv_reg_mr(cb->pd, 
		(struct request_s*) server_req_area,  REQ_AC * sizeof(struct request_s) , FLAGS); // REQ_AC * FARM_INLINE_READ_SIZE
		 
		compact_hash = memalign(4096, 1000000  * sizeof(hashtable_t) ); 
//		local_cache =   memalign(4096, 4000000  * sizeof(struct local_cache_s) );
//        memset((struct local_cache_s*) local_cache, 0 , 4000000  * sizeof(struct local_cache_s) ); 		
	    buckets = memalign(4096, sizeof(hash_item_t) * 4000000 /*(1 << HOPSCOTCH_INIT_BSIZE_EXPONENT)*/ ); 
        //buckets = malloc(sizeof(struct hopscotch_bucket) * (1 << HOPSCOTCH_INIT_BSIZE_EXPONENT));		
		//hhead = hopscotch_init(hasht, 16);
		hashtable_create(compact_hash, 1000000);
		hash_table_initialize(compact_hash, 4000000 );
		//int k,r;
		//printf("Key to insert:%d\n",1<<hhead->exponent);
		//fflush(stdout);
		//int32_t ikey[4000000]; 

//printf("this is the number of buckets :%d\n",1<<hhead->exponent);	  
//fflush(stdout);
//sleep(2); 	 
        server_hash_area = buckets;
		//memset((int*) server_hash_area, 0 ,  SERVER_AREA   ); // REQ_AC * FARM_INLINE_READ_SIZE					
		server_hash_area_mr = ibv_reg_mr(cb->pd, 
			(hash_item_t *) server_hash_area, sizeof(hash_item_t) * 4000000 /*1<<hhead->exponent*/, FLAGS);				

		/*int k,r;
		
 	    for( k=0 ; k <  1<<hhead->exponent; k++) {  // 1<<hhead->exponent 
		  r=hopscotch_insert(hhead,&k,&k);		
		  if (r){printf("not inserteeeedd %d\n",k); fflush(stdout);}
		}			
		*/
		
			/*
		k=10;
		size_t sz = 1ULL << hhead->exponent;
		size_t idx = k & (sz - 1);
		
		int32_t * t = (int32_t *) hhead->buckets[idx].data;
	    printf("found!:%d\n", *t );
	    fflush(stdout);		
		*/	
/*		k=10;
		int *j=NULL;
		j = (int *)hopscotch_lookup(hhead, (void*)&k);		
		if(j){
		  printf(" found %d\n",*j); fflush(stdout); 
		}


		k=10;
		
		j = (int *)hopscotch_lookup(hhead, (void*)&k);		
		if(j){
		  printf(" found %d\n",*j); fflush(stdout); 
		}		
			*/
														
		//CPE(!server_req_area_mr, "server_req_mr reg_mr failed", errno);
	}
	return 0;
}

void print_stag(struct stag st)
{
	fflush(stdout);
	fprintf(stderr, "\t%lu, %u, size===================%u\n", st.buf, st.rkey, st.size); 
}

void print_qp_attr(struct qp_attr dest)
{
	fflush(stdout);
	fprintf(stderr, "\t%d %d %d\n", dest.lid, dest.qpn, dest.psn);
}

void micro_sleep(double microseconds)
{
	struct timespec start, end;
	double sleep_seconds = (double) microseconds / 1500000;
	clock_gettime(CLOCK_REALTIME, &start);
	while(1) {
		clock_gettime(CLOCK_REALTIME, &end);
		double seconds = (end.tv_sec - start.tv_sec) + 
			(double) (end.tv_nsec - start.tv_nsec) / 1000000000;
		if(seconds > sleep_seconds) {
			return;
		}
	}
}

// Only some writes need to be signalled
void set_signal(int num, struct ctrl_blk *cb)
{	
	if(num % S_DEPTH == 0) {
		cb->wr.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
	} else {
		cb->wr.send_flags = IBV_SEND_INLINE;
	}
}

//Poll one of many recv CQs
void poll_cq(struct ibv_cq *cq, int num_completions)
{
	struct ibv_wc *wc = (struct ibv_wc *) malloc(
		num_completions * sizeof(struct ibv_wc));
	int comps= 0, i = 0;
	while(comps < num_completions) {
		int new_comps = ibv_poll_cq(cq, num_completions - comps, wc);
		if(new_comps != 0) {
			comps += new_comps;
			for(i = 0; i < new_comps; i++) {
				if(wc[i].status != 0) {
					fprintf(stderr, "poll_recv_cq error %d\n", wc[i].status);
					exit(0);
				}
			}
		}
	}
}

int is_roce(void)
{
	char *env = getenv("ROCE");
	if(env == NULL) {		// If ROCE environment var is not set
		fprintf(stderr, "ROCE not set\n");
		exit(-1);
	}
	return atoi(env);
}

inline uint32_t fastrand(uint64_t* seed)
{
    *seed = *seed * 1103515245 + 12345;
    return (uint32_t)(*seed >> 32);
}

 /* Initialize the hash table
 */

