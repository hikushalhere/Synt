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

void process_incoming_proposal(Proposal *p) {

	int	i, retval;

	if(conflict_proposal(p) == 1) {
		return;
	}

	#ifdef DEBUG
	printf("inc proposal,  seq %d, view %d\n", p->seq, p->view);
	#endif

	pthread_mutex_lock(&global_lock);

	global_history_insert_proposal(&Global_History, p);

	#ifdef DEBUG
	traverse_global_history(&Global_History);
	#endif

	Accept *a_msg;

	a_msg = NULL;
	while(a_msg == NULL) {
		a_msg = (Accept *)malloc(sizeof(Accept));
	}

	a_msg->type = htonl(TYPE_ACCEPT);
	a_msg->server_id = htonl(my_server_id);
	a_msg->view = htonl(p->view);
	a_msg->seq = htonl(p->seq);

	for(i = 1; i <= num_servers_g; i++) {

		if(i == my_server_id) {
			continue;
		}

		#ifdef DEBUG
		printf("sending accept to %d\n", i);
		#endif
		retval = sendto(peers_sockets[i], a_msg, sizeof(Accept), 0, &peers_sockaddr[i], sizeof(struct sockaddr));
		if(retval == -1) {
			printf("send error %d while sending to %d\n", errno, i);
			fflush(NULL);
		}
	}

	pthread_mutex_unlock(&global_lock);

	free(a_msg);
}

int globally_ordered_ready(uint32_t seq) {

	Global_History_entry *entry;
	uint32_t	view;
	int		num_accepts, i;

	pthread_mutex_lock(&global_lock);

	entry = global_history_exists(&Global_History, seq);
	if(entry == NULL) {
		pthread_mutex_unlock(&global_lock);
		return -1;
	}

	if(entry->proposal == NULL) {
		pthread_mutex_unlock(&global_lock);
		return -1;
	}

	view = entry->proposal->view;

	if(entry->accepts == NULL) {
		pthread_mutex_unlock(&global_lock);
		return -1;
	}

	num_accepts = 0;
	for(i = 1; i <= num_servers_g; i++) {

		if(entry->accepts[i] == NULL) {
			continue;
		}

		if(entry->accepts[i]->view != view) {
			continue;
		}

		num_accepts++;
	}

	pthread_mutex_unlock(&global_lock);

	if(num_accepts >= (num_servers_g/2)) {
		return 1;
	}
	else {
		return -1;
	}
}

//TODO
void advance_aru() {

	uint32_t	i;
	Global_History_entry *entry;
	pthread_mutex_lock(&global_lock);

	i = Local_Aru + 1;
	while(1) {

		entry = global_history_exists(&Global_History, i);
		if(entry == NULL) {
			pthread_mutex_unlock(&global_lock);
			return;
		}

		if(entry->ordered == NULL) {
			pthread_mutex_unlock(&global_lock);
			return;
		}

		i++;
		Local_Aru++;
		execute_client_update(&entry->ordered->update, i-1);
	}

	pthread_mutex_unlock(&global_lock);
}

void process_incoming_accept(Accept *a) {

	Globally_Ordered_Update *gou = NULL;
	Global_History_entry	*entry;

	if(conflict_accept(a) == 1) {
		return;
	}

	pthread_mutex_lock(&global_lock);

	global_history_insert_accept(&Global_History, a);
	#ifdef DEBUG
	traverse_global_history(&Global_History);
	#endif
	entry = global_history_exists(&Global_History, a->seq);

	if(entry == NULL) {
		pthread_mutex_unlock(&global_lock);
		return;
	}

	if(entry->proposal == NULL) {
		pthread_mutex_unlock(&global_lock);
		return;
	}

	if(globally_ordered_ready(a->seq) == 1) {

		gou = NULL;
		while(gou == NULL) {
			gou = malloc(sizeof(Globally_Ordered_Update));
		}
		gou->type = TYPE_GOU;
		gou->server_id = my_server_id;
		gou->seq = a->seq;
		gou->update = entry->proposal->update;

		global_history_insert_gou(&Global_History, gou);
		#ifdef DEBUG
		traverse_global_history(&Global_History);
		#endif

		#ifdef DEBUG
		printf("Advancing aru\n");
		#endif

		advance_aru();
	}

	pthread_mutex_unlock(&global_lock);
}

void send_proposal() {

	uint32_t	seq;
	int		i, retval;
	Global_History_entry 	*g_entry;
	Update_Queue_entry	*c_entry;
	Proposal		*p_msg;
	Client_Update		u;

	pthread_mutex_lock(&global_lock);

	seq = Last_Proposed + 1;

	g_entry = global_history_exists(&Global_History, seq);

	if(g_entry != NULL) {

		if(g_entry->ordered != NULL) {
			Last_Proposed++;
			send_proposal();
			pthread_mutex_unlock(&global_lock);
			return;
		}

		if(g_entry->proposal != NULL) {
			u = g_entry->proposal->update;
		}
		else if(Update_Queue.next == NULL) {
			pthread_mutex_unlock(&global_lock);
			return;
		}
		else {
			c_entry = Update_Queue.next;
			Update_Queue.next = c_entry->next;
			if(c_entry->client_update == NULL) {
				pthread_mutex_unlock(&global_lock);
				return;
			}
			u = *c_entry->client_update;
			free(c_entry->client_update);
			free(c_entry);
		}
	}
	else {
		if(Update_Queue.next == NULL) {
			pthread_mutex_unlock(&global_lock);
			return;
		}
		else {
			c_entry = Update_Queue.next;
			Update_Queue.next = c_entry->next;
			if(c_entry->client_update == NULL) {
				pthread_mutex_unlock(&global_lock);
				return;
			}
			u = *c_entry->client_update;
			free(c_entry->client_update);
			free(c_entry);
		}
	}

	p_msg = NULL;
	while(p_msg == NULL) {
		p_msg = malloc(sizeof(Proposal));
	}

	p_msg->type = TYPE_PROPOSAL;
	p_msg->server_id = my_server_id;
	p_msg->view = Last_Installed;
	p_msg->seq = seq;
	p_msg->update = u;

	global_history_insert_proposal(&Global_History, p_msg);
	#ifdef DEBUG
	traverse_global_history(&Global_History);
	#endif

	Last_Proposed = seq;

	p_msg->type = htonl(p_msg->type);
	p_msg->server_id = htonl(p_msg->server_id);
	p_msg->view = htonl(p_msg->view);
	p_msg->seq = htonl(p_msg->seq);
	p_msg->update.type = htonl(p_msg->update.type);
	p_msg->update.client_id = htonl(p_msg->update.client_id);
	p_msg->update.server_id = htonl(p_msg->update.server_id);
	p_msg->update.timestamp = htonl(p_msg->update.timestamp);
	p_msg->update.update = htonl(p_msg->update.update);

	#ifdef DEBUG
	printf("sending proposal, seq %d, update %d\n", seq, u.update);
	#endif

	for(i = 1; i <= num_servers_g; i++) {

		if(i == my_server_id) {
			continue;
		}

		retval = sendto(peers_sockets[i], p_msg, sizeof(Proposal), 0, &peers_sockaddr[i], sizeof(struct sockaddr));
		if(retval == -1) {
			printf("send error %d while sending to %d\n", errno, i);
			fflush(NULL);
		}
	}

	pthread_mutex_unlock(&global_lock);
}
