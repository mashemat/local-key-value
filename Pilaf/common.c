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
		if (ibv_destroy_qp(ctx->rc_qp[i])) {
			fprintf(stderr, "Couldn't destroy connected QP\n");
			return 1;
		}
		if (ibv_destroy_qp(ctx->uc_qp[i])) {
			fprintf(stderr, "Couldn't destroy connected QP\n");
			return 1;
		}
		if (ibv_destroy_cq(ctx->RECV_cq[i])) {
			fprintf(stderr, "Couldn't destroy connected CQ\n");
			return 1;
		}
		if (ibv_destroy_cq(ctx->SEND_cq[i])) {
			fprintf(stderr, "Couldn't destroy connected CQ\n");
			return 1;
		}
		if (ibv_destroy_cq(ctx->READ_cq[i])) {
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
	ctx->rc_qp = malloc(sizeof(int *) * ctx->num_conns);
	ctx->uc_qp = malloc(sizeof(int *) * ctx->num_conns);

	ctx->RECV_cq = malloc(sizeof(int *) * ctx->num_conns);
	ctx->SEND_cq = malloc(sizeof(int *) * ctx->num_conns);
	ctx->READ_cq = malloc(sizeof(int *) * ctx->num_conns);

	for(i = 0; i < ctx->num_conns; i++) {
		ctx->RECV_cq[i] = ibv_create_cq(ctx->context, 
			Q_DEPTH + 1, NULL, NULL, 0);
		CPE(!ctx->RECV_cq[i], "Couldn't create recv CQ", 0);

		ctx->SEND_cq[i] = ibv_create_cq(ctx->context, 
			Q_DEPTH + 1, NULL, NULL, 0);
		CPE(!ctx->SEND_cq[i], "Couldn't create send CQ", 0);
	
		ctx->READ_cq[i] = ibv_create_cq(ctx->context, 
			Q_DEPTH + 1, NULL, NULL, 0);
		CPE(!ctx->READ_cq[i], "Couldn't create send CQ", 0);

		struct ibv_qp_init_attr uc_init_attr = {
			.send_cq = ctx->SEND_cq[i],
			.recv_cq = ctx->RECV_cq[i],
			.cap     = {
				.max_send_wr  = Q_DEPTH,
				.max_recv_wr  = Q_DEPTH,
				.max_send_sge = 1,
				.max_recv_sge = 1,
				.max_inline_data =  512 //256
			},
			.qp_type = IBV_QPT_UC
		};

		ctx->uc_qp[i] = ibv_create_qp(ctx->pd, &uc_init_attr);
		//fprintf(stderr,"%s\n",strerror(errno));
		CPE(!ctx->uc_qp[i], "Couldn't create connected QP", 0);

		//Use uc_init_attr for the reliable queues
		uc_init_attr.send_cq = ctx->READ_cq[i];
		uc_init_attr.recv_cq = ctx->READ_cq[i];
		uc_init_attr.qp_type = IBV_QPT_RC;
		
		ctx->rc_qp[i] = ibv_create_qp(ctx->pd, &uc_init_attr);
		//fprintf(stderr,"%s\n",strerror(errno));
		CPE(!ctx->rc_qp[i], "Couldn't create connected QP", 0);
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
		if (ibv_modify_qp(ctx->rc_qp[i], &conn_attr, init_flags)) {
			fprintf(stderr, "Failed to modify QP to INIT\n");
			return;
		}

		if (ibv_modify_qp(ctx->uc_qp[i], &conn_attr, init_flags)) {
			fprintf(stderr, "Failed to modify QP to INIT\n");
			return;
		}
	}
}

int setup_buffers(struct ctrl_blk *cb)
{
	int FLAGS = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | 
				IBV_ACCESS_REMOTE_WRITE;
	// Chunks from client_req_area are written to server_resp_area.
	// Keep them non-zero.
	if (cb->is_client) {
		client_resp_area = memalign(4096, sizeof(fix_cucko_bucket) * WINDOW_SIZE); //PILAF_MAX_SIZE
		memset((fix_cucko_bucket *) client_resp_area, 0,  sizeof(fix_cucko_bucket)  * WINDOW_SIZE); //PILAF_MAX_SIZE
		client_resp_area_mr = ibv_reg_mr(cb->pd, 
			(fix_cucko_bucket *) client_resp_area , sizeof(fix_cucko_bucket) * WINDOW_SIZE, FLAGS);
		//CPE(!client_resp_area_mr, "client_resp_area reg_mr failed", errno);

		client_req_area = memalign(4096, sizeof(struct request_s ) * WINDOW_SIZE);
		memset((struct request_s *) client_req_area, 0,  sizeof(struct request_s )  * WINDOW_SIZE); //PILAF_MAX_SIZE
		client_req_area_mr = ibv_reg_mr(cb->pd, 
			(struct request_s *) client_req_area ,  sizeof(struct request_s )  * WINDOW_SIZE, FLAGS);
		//CPE(!client_req_area_mr, "client_resp_area reg_mr failed", errno);
	} else {
//		server_req_area = memalign(4096, SERVER_AREA * PILAF_MAX_SIZE); // REQ_AC * PILAF_MAX_SIZE


		server_request_area = memalign(4096, NUM_CLIENTS * sizeof(struct request_s)); // REQ_AC * PILAF_MAX_SIZE		
		memset((struct  request_s *) server_request_area,0, NUM_CLIENTS * sizeof(struct request_s) /*PILAF_MAX_SIZE*/ ); // REQ_AC * PILAF_MAX_SIZE
		server_request_area_mr = ibv_reg_mr(cb->pd, 
			(struct request_s*) server_request_area, NUM_CLIENTS * sizeof(struct request_s) , FLAGS); // REQ_AC * PILAF_MAX_SIZE
		
		hashtable = memalign(4096, ver * MAXN * sizeof(fix_cucko_bucket)); // REQ_AC * PILAF_MAX_SIZE
        memset((fix_cucko_bucket *) hashtable, 1, ver * MAXN * sizeof(fix_cucko_bucket) /*PILAF_MAX_SIZE*/ ); // REQ_AC * PILAF_MAX_SIZE
		server_req_area = hashtable;
		//server_req_area = memalign(4096, ver * MAXN * sizeof(fix_cucko_bucket)); // REQ_AC * PILAF_MAX_SIZE		
		//memset((fix_cucko_bucket *) server_req_area, 1, ver * MAXN * sizeof(fix_cucko_bucket) /*PILAF_MAX_SIZE*/ ); // REQ_AC * PILAF_MAX_SIZE
		server_req_area_mr = ibv_reg_mr(cb->pd, 
			(fix_cucko_bucket*) server_req_area, ver * MAXN  * sizeof(fix_cucko_bucket) , FLAGS); // REQ_AC * PILAF_MAX_SIZE
		//CPE(!server_req_area_mr, "server_req_mr reg_mr failed", errno);
	   
	}
	return 0;
}

void print_stag(struct stag st)
{
	fflush(stdout);
	fprintf(stderr, "\t%lu, %u, %u\n", st.buf, st.rkey, st.size); 
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
