#include "common.h"

int connect_ctx(struct ctrl_blk *ctx, int my_psn, struct qp_attr dest,
	struct ibv_qp* qp, int use_uc)
{
	struct ibv_qp_attr conn_attr = {
		.qp_state			= IBV_QPS_RTR,
		.path_mtu			= IBV_MTU_4096,
		.dest_qp_num		= dest.qpn,
		.rq_psn				= dest.psn,
		.ah_attr			= {
			.is_global			= (is_roce() == 1) ? 1 : 0,
			.dlid				= (is_roce() == 1) ? 0 : dest.lid,
			.sl					= 0,
			.src_path_bits		= 0,
			.port_num			= IB_PHYS_PORT
		}
	};

	if(is_roce()) {
		conn_attr.ah_attr.grh.dgid.global.interface_id = 
			dest.gid_global_interface_id;
		conn_attr.ah_attr.grh.dgid.global.subnet_prefix = 
			dest.gid_global_subnet_prefix;
	
		conn_attr.ah_attr.grh.sgid_index = 0;
		conn_attr.ah_attr.grh.hop_limit = 1;
	}

	int rtr_flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN
		| IBV_QP_RQ_PSN;
	if(!use_uc) {
		conn_attr.max_dest_rd_atomic = 16;
		conn_attr.min_rnr_timer = 12;
		rtr_flags |= IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;
	}
	if (ibv_modify_qp(qp, &conn_attr, rtr_flags)) {
		fprintf(stderr, "Failed to modify QP to RTR\n");
		return 1;
	}

	memset(&conn_attr, 0, sizeof(conn_attr));
	conn_attr.qp_state	    = IBV_QPS_RTS;
	conn_attr.sq_psn	    = my_psn;
	int rts_flags = IBV_QP_STATE | IBV_QP_SQ_PSN;
	if(!use_uc) {
		conn_attr.timeout = 14;
		conn_attr.retry_cnt = 7;
		conn_attr.rnr_retry = 7;
		conn_attr.max_rd_atomic = 16;
		conn_attr.max_dest_rd_atomic = 16;
		rts_flags |= IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
			  IBV_QP_MAX_QP_RD_ATOMIC;
	}
	if (ibv_modify_qp(qp, &conn_attr, rts_flags)) {
		fprintf(stderr, "Failed to modify QP to RTS\n");
		return 1;
	}

	return 0;
}

void client_exch_dest(struct ctrl_blk *cb)
{
	int sockfd, i, sock_port;

	struct sockaddr_in serv_addr;
	struct hostent *server;
	char server_name[20],sock_port_str[20];

	for(i = 0; i < NUM_SERVERS; i++) {
		scanf("%s", server_name);
		scanf("%s", sock_port_str);
		fprintf(stderr, "At client %d, server_name = %s, port = %s\n", cb->id, 
			server_name, sock_port_str);
		sock_port = atoi(sock_port_str);

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		CPE(sockfd < 0, "Error opening socket", 0);
	
		server = gethostbyname(server_name);
		CPE(server == NULL, "No such host", 0);
	
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
			server->h_length);
		serv_addr.sin_port = htons(sock_port);
	
		if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) {
			fprintf(stderr, "ERROR connecting");
		}

		if(read(sockfd, &server_req_area_stag[i], S_STG) < 0) {
			fprintf(stderr, "ERROR reading from socket");
		}
		fprintf(stderr, "Client %d <-- Server %d's stag: ", cb->id, i);
		print_stag(server_req_area_stag[i]);
			
		
		if(read(sockfd, &server_pointer_hash_area_stag[i], S_STG) < 0) {  // server_pointer_hash_area_stag keeps the address of server's hash tble address in client side. 
			fprintf(stderr, "ERROR reading from socket");
		}
		fprintf(stderr, "Client %d <-- Server %d's stag: ", cb->id, i);
		print_stag(server_pointer_hash_area_stag[i]);
	
	
	/* write here */
			//Exchange stag information
		client_resp_area_stag[0].buf = (uint64_t) (unsigned long) 
			client_resp_area;
		client_resp_area_stag[0].rkey = client_resp_area_mr->rkey;
		client_resp_area_stag[0].size = FARM_ITEM_SIZE * WINDOW_SIZE;  //REQ_AC * FARM_INLINE_READ_SIZE;
	
		if(write(sockfd, &client_resp_area_stag[0], S_STG) < 0) {
			fprintf(stderr, "ERROR writing server stag to socket");
		}
		fprintf(stderr, "Write Client %d --> Server %d stag: ", cb->id, i);
		print_stag(client_resp_area_stag[0]);
    /*write here */
	
        	
	
		//Exchange attributes for QPs
		if(write(sockfd, &cb->local_qp_attrs[i], S_QPA) < 0) {
			fprintf(stderr, "ERROR writing to socket");
		}
		fprintf(stderr, "Client %d --> Server %d qp_attr: ", cb->id, i);
		print_qp_attr(cb->local_qp_attrs[i]);

		if(read(sockfd, &cb->remote_qp_attrs[i], S_QPA) < 0) {
			fprintf(stderr, "Error writing to socket");
		}
		fprintf(stderr, "Client %d <-- Server %d's qp_attr: ", cb->id, i);
		print_qp_attr(cb->remote_qp_attrs[i]);

		close(sockfd);
	}
}

void server_exch_dest(struct ctrl_blk *cb)
{
	int sockfd, newsockfd, i;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "ERROR opening socket");
	}

	int on = 1, status = -1;
    status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
        		(const char *) &on, sizeof(on));
    if (-1 == status) {
        perror("setsockopt(...,SO_REUSEADDR,...)");
    }

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(cb->sock_port);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		fprintf(stderr, "ERROR on binding");
	}
	printf("Server %d listening on port %d\n", cb->id, cb->sock_port);
	listen(sockfd, NUM_CLIENTS);
	
	for(i = 0; i < NUM_CLIENTS; i++) {

		printf("Server %d trying to accept()\n", cb->id);
		newsockfd = accept(sockfd, NULL, NULL);
		if (newsockfd < 0) {
			fprintf(stderr, "ERROR on accept");
			exit(1);
		}

		//Exchange stag information
		server_req_area_stag[0].buf = (uint64_t) (unsigned long) 
			server_req_area;
		server_req_area_stag[0].rkey = server_req_area_mr->rkey;
		server_req_area_stag[0].size = REQ_AC * sizeof(int);  //REQ_AC * FARM_INLINE_READ_SIZE;
	
		if(write(newsockfd, &server_req_area_stag[0], S_STG) < 0) {
			fprintf(stderr, "ERROR writing server stag to socket");
		}
		fprintf(stderr, "Server %d --> Client %d stag: ", cb->id, i);
		print_stag(server_req_area_stag[0]);

		
		//Exchange hash stag information
		server_hash_area_stag[0].buf = (uint64_t) (unsigned long) 
			server_hash_area;
		server_hash_area_stag[0].rkey = server_hash_area_mr->rkey;
		server_hash_area_stag[0].size = 1<<HOPSCOTCH_INIT_BSIZE_EXPONENT;  //REQ_AC * FARM_INLINE_READ_SIZE;
	
		if(write(newsockfd, &server_hash_area_stag[0], S_STG) < 0) {
			fprintf(stderr, "ERROR writing server stag to socket");
		}
		fprintf(stderr, "Server %d --> Client %d stag: ", cb->id, i);
		print_stag(server_hash_area_stag[0]);
		//End hash stag information		
		
/*  Read here */
		if(read(newsockfd , &client_req_area_stag[i], S_STG) < 0) {
			fprintf(stderr, "ERROR reading from socket");
		}
		fprintf(stderr, "I am Server %d <-- Client %d's stag: ", cb->id, i);
		print_stag(client_req_area_stag[i]);
/*  Read here */	

	
		
		//Exchange attributes for RC QPs
		if(read(newsockfd, &cb->remote_qp_attrs[i], S_QPA) < 0) {
			fprintf(stderr, "ERROR reading qp attrs from socket");
		}
		fprintf(stderr, "Server %d <-- Client %d's qp_attr: ", cb->id, i);
		print_qp_attr(cb->remote_qp_attrs[i]);
		
		if(connect_ctx(cb, cb->local_qp_attrs[i].psn, 
			cb->remote_qp_attrs[i], cb->qp[i], 0)) {
			fprintf(stderr, "Couldn't connect to remote QP\n");
			exit(0);
		}
	
		if(write(newsockfd, &cb->local_qp_attrs[i], S_QPA) < 0 ) {
			fprintf(stderr, "Error writing to socket");
		}
		fprintf(stderr, "Server %d --> Client %d RC conn qp_attr: ", cb->id, i);
		print_qp_attr(cb->local_qp_attrs[i]);

		close(newsockfd);
	}
	close(sockfd);
}
