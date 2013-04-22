#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include "message.h"
#include "globals.h"
//#define DEBUG
void *paxos_socket_handler(void *arg) {

	int 	udp_listen_socket = *((int *)arg);
	struct 	sockaddr_in peer;
	int	peer_addr_len, retval;

	uint32_t	type;

	#ifdef DEBUG
	printf("Paxos thread - socket %d\n", udp_listen_socket);
	#endif

	uint8_t *buf;

	buf = NULL;
	while(buf == NULL) {
		buf = (uint8_t *)malloc(5000);
	}

	while(1) {

		peer_addr_len = sizeof(struct sockaddr_in);
		retval = recvfrom(udp_listen_socket, buf, 1000, 0, (struct sockaddr *)&peer, &peer_addr_len);
		if(retval == -1) {
			printf("recvfrom retval = -1\n");
			continue;
		}

		type = *((uint32_t *)buf);

		#ifdef DEBUG
		if(ntohl(type) != 3)
			printf("recvd type %d\n", ntohl(type));
		#endif

		type = ntohl(type);

		Client_Update	*cu_msg;
		View_Change 	*vc_msg;
		VC_Proof	*vcp_msg;
		Prepare		*p_msg;
		Prepare_OK	*pok_msg;
		Proposal	*prop_msg;
		Accept		*a_msg;

		switch(type) {

			case TYPE_CLIENT_UPDATE:
				cu_msg = (Client_Update *)buf;

				cu_msg->type = ntohl(cu_msg->type);
				cu_msg->server_id = ntohl(cu_msg->server_id);
				cu_msg->client_id = ntohl(cu_msg->client_id);
				cu_msg->timestamp = ntohl(cu_msg->timestamp);
				cu_msg->update = ntohl(cu_msg->update);

				client_update_handler(cu_msg);
				break;

			case TYPE_VIEW_CHANGE:
				
				vc_msg = (View_Change *)buf;

				vc_msg->type = ntohl(vc_msg->type);
				vc_msg->server_id = ntohl(vc_msg->server_id);
				vc_msg->attempted = ntohl(vc_msg->attempted);

				process_incoming_view_change(vc_msg);

				break;

			case TYPE_VC_PROOF:

				vcp_msg = (VC_Proof *)buf;

				vcp_msg->type = ntohl(vcp_msg->type);
				vcp_msg->server_id = ntohl(vcp_msg->server_id);
				vcp_msg->installed = ntohl(vcp_msg->installed);

				process_incoming_vc_proof(vcp_msg);

				break;

			case TYPE_PREPARE:

				p_msg = (Prepare *)buf;

				p_msg->type = ntohl(p_msg->type);
				p_msg->server_id = ntohl(p_msg->server_id);
				p_msg->view = ntohl(p_msg->view);
				p_msg->local_aru = ntohl(p_msg->local_aru);

				process_incoming_prepare(p_msg);

				break;

			case TYPE_PREPARE_OK:

				pok_msg = (Prepare_OK *)buf;

				pok_msg->type = ntohl(pok_msg->type);
				pok_msg->server_id = ntohl(pok_msg->server_id);
				pok_msg->view = ntohl(pok_msg->view);

				process_incoming_prepare_ok(pok_msg);

				break;

			case TYPE_PROPOSAL:

				prop_msg = (Proposal *)buf;

				prop_msg->type = ntohl(prop_msg->type);
				prop_msg->server_id = ntohl(prop_msg->server_id);
				prop_msg->view = ntohl(prop_msg->view);
				prop_msg->seq = ntohl(prop_msg->seq);
				prop_msg->update.type = ntohl(prop_msg->update.type);
				prop_msg->update.client_id = ntohl(prop_msg->update.client_id);
				prop_msg->update.server_id = ntohl(prop_msg->update.server_id);
				prop_msg->update.timestamp = ntohl(prop_msg->update.timestamp);
				prop_msg->update.update = ntohl(prop_msg->update.update);

				process_incoming_proposal(prop_msg);
				break;

			case TYPE_ACCEPT:

				a_msg = (Accept *)buf;

				a_msg->type = ntohl(a_msg->type);
				a_msg->server_id = ntohl(a_msg->server_id);
				a_msg->view = ntohl(a_msg->view);
				a_msg->seq = ntohl(a_msg->seq);

				process_incoming_accept(a_msg);
				break;

		}
	}
}
