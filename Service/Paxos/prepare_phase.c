#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include "message.h"
#include "globals.h"

uint8_t *construct_prepare_ok(uint32_t , int *);

uint8_t *construct_prepare_ok(uint32_t aru, int *buf_len) {

	uint32_t  	total_proposals = 0;
	uint32_t 	total_globally_ordered_updates = 0;
	uint8_t		*buf;

	pthread_mutex_lock(&global_lock);

	Global_History_entry *curr = Global_History.next, *start = NULL;

	while(curr != NULL) {

		if(curr->seq <= aru) {
			curr = curr->next;
			continue;
		}
		if(start == NULL) {
			start = curr;
		}
		if(curr->ordered != NULL) {
			total_globally_ordered_updates++;
			curr = curr->next;
			continue;
		}
		if(curr->proposal != NULL) {
			total_proposals++;
			curr = curr->next;
			continue;
		}
		curr = curr->next;
	}

	*buf_len = (sizeof(Prepare_OK) + sizeof(uint32_t) * 2) + (sizeof(Globally_Ordered_Update) * total_globally_ordered_updates) +
	                                                                  (sizeof(Proposal) * total_proposals);
	buf = NULL;
	while(buf == NULL) {
		buf = (uint8_t *)malloc(*buf_len);
	}

	Prepare_OK *pok_ptr = (Prepare_OK *)buf;

	pok_ptr->type = TYPE_PREPARE_OK;
	pok_ptr->server_id = my_server_id;
	pok_ptr->view = Last_Installed;

	uint32_t *t_p, *t_g;
	t_p = (uint32_t *)(pok_ptr + 1);

	*t_p = htonl(total_proposals);

	Proposal *p_ptr;
	Globally_Ordered_Update *g_ptr;

	p_ptr = (Proposal *)(t_p + 1);

	curr = start;
	while(curr != NULL) {
		if(curr->ordered != NULL) {
			curr = curr->next;
			continue;
		}
		if(curr->proposal != NULL) {
			p_ptr->type = htonl(TYPE_PROPOSAL);
			p_ptr->server_id = htonl(curr->proposal->server_id);
			p_ptr->view = htonl(curr->proposal->view);
			p_ptr->seq = htonl(curr->proposal->seq);
			p_ptr->update.type = htonl(curr->proposal->update.type);
			p_ptr->update.client_id = htonl(curr->proposal->update.client_id);
			p_ptr->update.server_id = htonl(curr->proposal->update.server_id);
			p_ptr->update.timestamp = htonl(curr->proposal->update.timestamp);
			p_ptr->update.update = htonl(curr->proposal->update.update);

			p_ptr += 1;
			curr = curr->next;
			continue;
		}
		curr = curr->next;
	}

	t_g = (uint32_t *)p_ptr;
	*t_g = htonl(total_globally_ordered_updates);

	g_ptr = (Globally_Ordered_Update *)(t_g + 1);

	curr = start;
	while(curr != NULL) {
		if(curr->ordered != NULL) {
			g_ptr->type = htonl(TYPE_GOU);
			g_ptr->server_id = htonl(curr->ordered->server_id);
			g_ptr->seq = htonl(curr->ordered->seq);
			g_ptr->update.type = htonl(curr->ordered->update.type);
			g_ptr->update.client_id = htonl(curr->ordered->update.client_id);
			g_ptr->update.server_id = htonl(curr->ordered->update.server_id);
			g_ptr->update.timestamp = htonl(curr->ordered->update.timestamp);
			g_ptr->update.update = htonl(curr->ordered->update.update);

			g_ptr += 1;
			curr = curr->next;
			continue;
		}
		curr = curr->next;
	}
	pthread_mutex_unlock(&global_lock);

	return buf;
}

int length_prepare_ok(Prepare_OK *pok) {

	int	len;
	int	total_p, total_g;
	uint32_t *ptr;
	Proposal *p;
	Globally_Ordered_Update *g;

	ptr = (uint32_t *)(pok + 1);

	total_p = ntohl(*ptr);
	p = (Proposal *)(ptr + 1);

	p += total_p;

	ptr = (uint32_t *)p;
	total_g = ntohl(*ptr);

	len = 5 * sizeof(uint32_t) + total_p * sizeof(Proposal) + total_g * sizeof(Globally_Ordered_Update);

	return len;

}

void shift_to_prepare_phase() {

	Prepare		*p_msg;
	VC_Proof	*vcp_msg;
	uint8_t		*send_buf;
	int		buf_len, i, retval;

	pthread_mutex_lock(&global_lock);

	Last_Installed = Last_Attempted;

	p_msg = NULL;
	while(p_msg == NULL) {
		p_msg = (Prepare *)malloc(sizeof(Prepare));
	}

	p_msg->type = (TYPE_PREPARE);
	p_msg->server_id = (my_server_id);
	p_msg->view = (Last_Installed);
	p_msg->local_aru = (Local_Aru);

	if(Prepare_g != NULL) {
		free(Prepare_g);
	}
	Prepare_g = p_msg;

	send_buf = (uint8_t *)construct_prepare_ok(Local_Aru, &buf_len);

	#ifdef DEBUG
	printf("in stpp pok len %d, aru %d\n", buf_len, Local_Aru);
	#endif

	if(Prepare_OK_g[my_server_id] != NULL) {
		free(Prepare_OK_g[my_server_id]);
	}
	Prepare_OK_g[my_server_id] = (Prepare_OK *)send_buf;

	Last_Exe_Enq_entry *xentry = Last_Exe_Enq.next;
	while(xentry != NULL) {
		xentry->last_enqueued = 0;
		xentry = xentry->next;
	}
	//memset(Last_Enqueued, 0, sizeof(uint32_t) * CLIENT_MAX);

	vcp_msg = NULL;
	while(vcp_msg == NULL) {
		vcp_msg = malloc(sizeof(VC_Proof));
	}

	vcp_msg->type = htonl(TYPE_VC_PROOF);
	vcp_msg->server_id = htonl(my_server_id);
	vcp_msg->installed = htonl(Last_Installed);

	//Send Prepare to all servers
	p_msg->type = htonl(p_msg->type);
	p_msg->server_id = htonl(p_msg->server_id);
	p_msg->view = htonl(p_msg->view);
	p_msg->local_aru = htonl(p_msg->local_aru);

	for(i = 1; i <= num_servers_g; i++) {

		if(i == my_server_id) {
			continue;
		}
		/*
		retval = sendto(peers_sockets[i], vcp_msg, sizeof(VC_Proof), 0, &peers_sockaddr[i], sizeof(struct sockaddr));
		if(retval == -1) {
			printf("send error %d while sending to %d\n", errno, i);
			fflush(NULL);
		}
		*/
		retval = sendto(peers_sockets[i], p_msg, sizeof(Prepare), 0, &peers_sockaddr[i], sizeof(struct sockaddr));
		if(retval == -1) {
			printf("send error %d while sending to %d\n", errno, i);
			fflush(NULL);
		}
	}

	p_msg->type = htonl(p_msg->type);
	p_msg->server_id = htonl(p_msg->server_id);
	p_msg->view = htonl(p_msg->view);
	p_msg->local_aru = htonl(p_msg->local_aru);

	pthread_mutex_unlock(&global_lock);
	free(vcp_msg);
}

void process_incoming_prepare(Prepare *p) {

	uint8_t 	*buf;
	int		buf_len, retval;
	Prepare_OK	*pok;
	uint32_t	leader;

	if(conflict_prepare(p) == 1) {
		return;
	}

	pthread_mutex_lock(&global_lock);
	#ifdef DEBUG
	printf("inc prepare, no conflict view %d, state %d\n", p->view, state);
	#endif

	if(state == STATE_LEADER_ELECTION) {

		fflush(NULL);
		if(Prepare_g != NULL) {
			free(Prepare_g);
			Prepare_g = NULL;
		}
		while(Prepare_g == NULL) {
			Prepare_g = malloc(sizeof(Prepare));
		}

		fflush(NULL);
		*Prepare_g = *p;

		buf = construct_prepare_ok(p->local_aru, &buf_len);

		#ifdef DEBUG
		printf("constructing pok, len %d, aru %d\n", buf_len, Local_Aru);
		#endif

		pok = (Prepare_OK *)buf;

		pok->view = p->view;

		if(Prepare_OK_g[my_server_id] != NULL) {
			free(Prepare_OK_g[my_server_id]);
		}
		Prepare_OK_g[my_server_id] = pok;


		if(progress_timer_ctr <= 0) {
			progress_timer *= 2;
			if(progress_timer > 5) {
				progress_timer = 5;
			}
			progress_timer_ctr = progress_timer;
			if( ( ((Last_Attempted + 1) % num_servers_g) + 1) == my_server_id) {
				progress_timer_ctr -= 2;
			}
		}

		shift_to_reg_non_leader();
	
		//SEND to Prepare_OK to leader

		pok->type = htonl(pok->type);
		pok->server_id = htonl(pok->server_id);
		pok->view = htonl(pok->view);

		leader = (p->view % num_servers_g) + 1;

		#ifdef DEBUG
		printf("Sending p ok to %d\n", leader);
		#endif

		retval = sendto(peers_sockets[leader], pok, buf_len, 0, &peers_sockaddr[leader], sizeof(struct sockaddr));
		if(retval == -1) {
			printf("send error %d while sending to %d\n", errno, leader);
			fflush(NULL);
		}

		pok->type = htonl(pok->type);
		pok->server_id = htonl(pok->server_id);
		pok->view = htonl(pok->view);
	}
	else {

		pok = Prepare_OK_g[my_server_id];

		if(pok == NULL) {
			pok = (Prepare_OK *)construct_prepare_ok(p->local_aru, &buf_len);
			pok->view = p->view;

			Prepare_OK_g[my_server_id] = pok;
		}

		leader = (pok->view % num_servers_g) + 1;
		//SEND prepare_ok to leader 
		pok->type = htonl(pok->type);
		pok->server_id = htonl(pok->server_id);
		pok->view = htonl(pok->view);

		retval = sendto(peers_sockets[leader], pok, length_prepare_ok(pok), 0, &peers_sockaddr[leader], sizeof(struct sockaddr));
		if(retval == -1) {
			printf("- send error %d while sending to %d\n", errno, leader);
			fflush(NULL);
		}

		pok->type = htonl(pok->type);
		pok->server_id = htonl(pok->server_id);
		pok->view = htonl(pok->view);
	}

	pthread_mutex_unlock(&global_lock);
}

int view_prepared_ready(uint32_t view) {

	int	num_poks = 0, i;

	pthread_mutex_lock(&global_lock);

	if(Prepare_OK_g == NULL) {
		pthread_mutex_unlock(&global_lock);
		return -1;
	}

	for(i = 1; i <= num_servers_g; i++) {

		if(Prepare_OK_g[i] == NULL) {
			continue;
		}

		if(Prepare_OK_g[i]->view != view) {
			continue;
		}

		num_poks++;
	}

	pthread_mutex_unlock(&global_lock);

	if(num_poks > (num_servers_g/2)) {
		return 1;
	}
	else {
		return -1;
	}
}

void process_incoming_prepare_ok(Prepare_OK *pok) {

	int	i;

	if(conflict_prepare_ok(pok) == 1) {
		return;
	}

	pthread_mutex_lock(&global_lock);

	if(Prepare_OK_g[pok->server_id] != NULL) {

		pthread_mutex_unlock(&global_lock);
		return;
	}

	while(Prepare_OK_g[pok->server_id] == NULL) {
		Prepare_OK_g[pok->server_id] = malloc(sizeof(Prepare_OK));
	}
	//TODO copy entire prepare_ok
	*Prepare_OK_g[pok->server_id] = *pok;

	uint32_t *uint32_ptr;
	uint32_t total_p, total_g;
	Proposal *p;
	Globally_Ordered_Update *g;

	uint32_ptr = (uint32_t *)(pok + 1);
	total_p = ntohl(*uint32_ptr);

	p = (Proposal *)(uint32_ptr + 1);

	for(i = 0; i < total_p; i++) {

		p->type = ntohl(p->type);
		p->server_id = ntohl(p->server_id);
		p->view = ntohl(p->view);
		p->seq = ntohl(p->seq);
		p->update.type = ntohl(p->update.type);
		p->update.client_id = ntohl(p->update.client_id);
		p->update.server_id = ntohl(p->update.server_id);
		p->update.timestamp = ntohl(p->update.timestamp);
		p->update.update = ntohl(p->update.update);

		global_history_insert_proposal(&Global_History, p);
		#ifdef DEBUG
		traverse_global_history(&Global_History);
		#endif

		p += 1;
	}

	uint32_ptr = (uint32_t *)p;
	total_g = ntohl(*uint32_ptr);

	g = (Globally_Ordered_Update *)(uint32_ptr + 1);

	for(i = 0; i < total_g; i++) {

		g->type = ntohl(g->type);
		g->server_id = ntohl(g->type);
		g->seq = ntohl(g->seq);
		g->update.type = ntohl(g->update.type);
		g->update.client_id = ntohl(g->update.client_id);
		g->update.server_id = ntohl(g->update.server_id);
		g->update.timestamp = ntohl(g->update.timestamp);
		g->update.update = ntohl(g->update.update);

		global_history_insert_gou(&Global_History, g);
		#ifdef DEBUG
		traverse_global_history(&Global_History);
		#endif

		g+= 1;
	}

	if(view_prepared_ready(pok->view) == 1) {
		#ifdef DEBUG
		printf("shifting to reg leader\n");
		#endif

		shift_to_reg_leader();
	}

	pthread_mutex_unlock(&global_lock);

}
