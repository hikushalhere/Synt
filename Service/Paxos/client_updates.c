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

void insert_into_update_queue(Update_Queue_entry *head, Client_Update *cu) {

	if( (head == NULL) || (cu == NULL) ) {
		return;
	}

	pthread_mutex_lock(&global_lock);

	Update_Queue_entry *curr = head->next, *prev = head, *new;

	new = NULL;
	while(new == NULL) {
		new = malloc(sizeof(Update_Queue_entry));
	}

	new->client_update = NULL;
	while(new->client_update == NULL) {
		new->client_update = malloc(sizeof(Client_Update));
	}
	*new->client_update = *cu;

	while(curr != NULL) {
		curr = curr->next;
		prev = prev->next;
	}

	prev->next = new;
	new->next = NULL;

	pthread_mutex_unlock(&global_lock);
}

Pending_Update_entry *find_pending_update(Pending_Update_entry *head, uint32_t client_id) {

	Pending_Update_entry *entry;

	if(head == NULL) {
		return;
	}

	pthread_mutex_lock(&global_lock);

	entry = head->next;
	while(entry != NULL) {
		if(entry->client_update == NULL) {
			entry = entry->next;
			continue;
		}

		if(entry->client_update->client_id == client_id) {
			break;
		}

		entry = entry->next;
	}

	pthread_mutex_unlock(&global_lock);

	return entry;
}

void add_to_pending_updates(Client_Update *cu, int client_socket) {

	pthread_mutex_lock(&global_lock);

	Pending_Update_entry *entry = NULL;
	Client_Update *new = NULL;

	entry = Pending_Updates.next;
	while(entry != NULL) {
		if(entry->client_update == NULL) {
			entry = entry->next;
			continue;
		}
		if(entry->client_update->client_id == cu->client_id) {
			break;
		}
		entry = entry->next;
	}

	if(entry != NULL) {
		*entry->client_update = *cu;
	}
	else {
		while(entry == NULL) {
			entry = malloc(sizeof(Pending_Update_entry));
		}

		new = NULL;
		while(new == NULL) {
			new = malloc(sizeof(Client_Update));
		}
		*new = *cu;

		entry->client_update = new;

		entry->next = Pending_Updates.next;
		Pending_Updates.next = entry;
	}

	entry->update_timer = 1;
	entry->client_socket = client_socket;
/*	
	while(new == NULL) {
		new = malloc(sizeof(Client_Update));
	}
	*new = *cu;

	if(Pending_Updates[cu->client_id] != NULL) {
		free(Pending_Updates[cu->client_id]);
	}
	Pending_Updates[cu->client_id] = new;

	update_timer_ctr[cu->client_id] = 1;
*/
	pthread_mutex_unlock(&global_lock);
}

int enqueue_update(Client_Update *cu) {

	pthread_mutex_lock(&global_lock);

	
	if(is_update_bound(cu) == 1) {
		pthread_mutex_unlock(&global_lock);
		return -1;
	}
	

	Last_Exe_Enq_entry *xentry = Last_Exe_Enq.next;
	while(xentry != NULL) {

		if(xentry->client_id == cu->client_id) {
			break;
		}
		xentry = xentry->next;
		continue;

	}
	if(xentry == NULL) {
		while(xentry == NULL) {
			xentry = malloc(sizeof(Last_Exe_Enq_entry));
		}
		xentry->client_id = cu->client_id;
		xentry->last_executed = 0;
		xentry->last_enqueued = 0;

		xentry->next = Last_Exe_Enq.next;
		Last_Exe_Enq.next = xentry;

		#ifdef DEBUG
		printf("new lst for cid %d\n", cu->client_id);
		#endif
	}

	if(cu->timestamp <= xentry->last_executed) {
		pthread_mutex_unlock(&global_lock);
		return -1;
	}

	if(cu->timestamp <= xentry->last_enqueued) {
		pthread_mutex_unlock(&global_lock);
		return -1;
	}

	/*
	if(cu->timestamp <= Last_Executed[cu->client_id]) {
		pthread_mutex_unlock(&global_lock);
		return -1;
	}

	if(cu->timestamp <= Last_Enqueued[cu->client_id]) {
		pthread_mutex_unlock(&global_lock);
		return -1;
	}
	*/

	insert_into_update_queue(&Update_Queue, cu);

	//Last_Enqueued[cu->client_id] = cu->timestamp;
	#ifdef DEBUG
	printf("enq old %d new %d\n", xentry->last_enqueued, cu->timestamp);
	#endif
	xentry->last_enqueued = cu->timestamp;

	pthread_mutex_unlock(&global_lock);

	return 1;
}

void client_update_handler(Client_Update *cu, int client_socket) {

	int leader, retval;
	Client_Update cu_to_send;
	
	pthread_mutex_lock(&global_lock);

	if(state == STATE_LEADER_ELECTION) {

		if(cu->server_id != my_server_id) {
			pthread_mutex_unlock(&global_lock);
			return;
		}
		if(enqueue_update(cu) == 1) {
			add_to_pending_updates(cu, client_socket);
		}
	}

	if(state == STATE_REG_NONLEADER) {

		if(cu->server_id == my_server_id) {
			#ifdef DEBUG
			printf("adding to pending updates\n");
			#endif

			add_to_pending_updates(cu, client_socket);

			#ifdef DEBUG
			printf("added to pending updates\n");
			#endif
			//TODO cu send to leader 
			leader = (Last_Installed % num_servers_g) + 1;

			cu_to_send.type = ntohl(cu->type);
			cu_to_send.server_id = htonl(cu->server_id);
			cu_to_send.client_id = htonl(cu->client_id);
			cu_to_send.timestamp = htonl(cu->timestamp);
			cu_to_send.update = htonl(cu->update);

			#ifdef DEBUG
			printf("sending client update to leader %d\n", leader);
			#endif

			retval = sendto(peers_sockets[leader], &cu_to_send, sizeof(Client_Update), 0, &peers_sockaddr[leader], sizeof(struct sockaddr));
			if(retval == -1) {
				printf("send error %d while sending to %d\n", errno, leader);
				fflush(NULL);
			}
		}
	}

	if(state == STATE_REG_LEADER) {

		if(enqueue_update(cu) == 1) {
			if(cu->server_id == my_server_id) {
				add_to_pending_updates(cu, client_socket);
			}
			
			send_proposal();
		}
	}

	pthread_mutex_unlock(&global_lock);
}

void execute_client_update(Client_Update *cu, uint32_t seq) {

	char	reply[] = "OK";
	int	retval;
	Client_Update reply_u;
	Pending_Update_entry *pentry;

	pthread_mutex_lock(&global_lock);

	if(cu->server_id == my_server_id) {

		/*
		if(client_open_sockets[cu->client_id] != -1) {

			//TODO check retval
			retval = send(client_open_sockets[cu->client_id], reply, 2, 0);

			close(client_open_sockets[cu->client_id]);
			client_open_sockets[cu->client_id] = -1;
		}
		*/

		
		pentry = find_pending_update(&Pending_Updates, cu->client_id);
		if(pentry != NULL) {
			if(pentry->client_update->update == cu->update) {
				pentry->update_timer = 0;
			}

			//Commented later, since we do not need to reply here.
			/*
			retval = send(pentry->client_socket, reply, 2, 0);
			close(pentry->client_socket);
			*/
		}
		
		/*
		if(Pending_Updates[cu->client_id] != NULL) {
			if(Pending_Updates[cu->client_id]->update == cu->update) {

				//TODO Cancel update timer
				update_timer_ctr[cu->client_id] = 0;
				free(Pending_Updates[cu->client_id]);
				Pending_Updates[cu->client_id] = NULL;
			}
		}
		*/
	}

	printf("%d: Executed update %d from client %d with seq %d and view %d\n", my_server_id, cu->update, cu->client_id, seq, Last_Installed);
	reply_u.type = htonl(TYPE_CLIENT_UPDATE);
	reply_u.client_id = htonl(cu->client_id);
	reply_u.server_id = htonl(cu->server_id);
	reply_u.timestamp = htonl(cu->timestamp);
	reply_u.update = htonl(cu->update);

	sleep(5);
	send(zookeeper_socket, &reply_u, sizeof(Client_Update), 0);

	//Last_Executed[cu->client_id] = cu->timestamp;
	Last_Exe_Enq_entry *xentry = Last_Exe_Enq.next;
	while(xentry != NULL) {
		if(xentry->client_id == cu->client_id) {
			break;
		}

		xentry = xentry->next;
	}
	if(xentry == NULL) {
		while(xentry == NULL) {
			xentry = malloc(sizeof(Last_Exe_Enq_entry));
		}

		xentry->client_id = cu->client_id;
		xentry->last_executed = 0;
		xentry->last_enqueued = 0;
		
		xentry->next = Last_Exe_Enq.next;
		Last_Exe_Enq.next = xentry;
	}

	xentry->last_executed = cu->timestamp;

	if(state != STATE_LEADER_ELECTION) {
		//TODO restart progress timer
		progress_timer_ctr = progress_timer;
		if( (((Last_Installed + 1) % num_servers_g) + 1) == my_server_id) {
			progress_timer_ctr -= 2;
		}
		#ifdef DEBUG
		printf("progress timer reset at %d\n", progress_timer_ctr);
		#endif
	}
	
	if(state == STATE_REG_LEADER) {
		send_proposal();
	}

	pthread_mutex_unlock(&global_lock);
}

int are_updates_same(Client_Update *u1, Client_Update *u2) {

	if(u1->type != u2->type) {
		return -1;
	}
	
	if(u1->client_id != u2->client_id) {
		return -1;
	}

	if(u1->server_id != u2->server_id) {
		return -1;
	}

	if(u1->timestamp != u2->timestamp) {
		return -1;
	}

	if(u1->update != u2->update) {
		return -1;
	}

	return 1;
}

int is_update_bound(Client_Update *u) {

	Global_History_entry *g_entry;

	pthread_mutex_lock(&global_lock);

	g_entry = Global_History.next;

	while(g_entry != NULL) {

		if( (g_entry->proposal == NULL) && (g_entry->ordered == NULL) ) {
			g_entry = g_entry->next;
			continue;
		}

		if(g_entry->proposal != NULL) {
			if(are_updates_same(&g_entry->proposal->update, u) == 1) {
				break;
			}
			else {
				g_entry = g_entry->next;
				continue;
			}
		}

		if(g_entry->ordered != NULL) {
			if(are_updates_same(&g_entry->ordered->update, u) == 1) {
				break;
			}
			else {
				g_entry = g_entry->next;
				continue;
			}
		}
	}

	pthread_mutex_unlock(&global_lock);

	if(g_entry == NULL) {
		return -1;
	}
	else {
		return 1;
	}
}

void enqueue_unbound_pending_updates() {

	int	i;
	Global_History_entry 	*g_entry;
	Update_Queue_entry	*u_entry;
	Pending_Update_entry	*p_entry;

	pthread_mutex_lock(&global_lock);

	p_entry = Pending_Updates.next;
	while(p_entry != NULL) {

		if(p_entry->client_update == NULL) {
			p_entry = p_entry->next;
			continue;
		}

		if(is_update_bound(p_entry->client_update) == 1) {
			p_entry = p_entry->next;
			continue;
		}

		u_entry = Update_Queue.next;
		while(u_entry != NULL) {

			if(u_entry->client_update == NULL) {
				u_entry = u_entry->next;
				continue;
			}

			if( are_updates_same(u_entry->client_update, p_entry->client_update) == 1 ) {
				break;
			}

			u_entry = u_entry->next;
		}

		if(u_entry != NULL) {
			p_entry = p_entry->next;
			continue;
		}

		enqueue_update(p_entry->client_update);
		p_entry = p_entry->next;
	}

	/*
	for(i = 0; i < CLIENT_MAX; i++) {

		if(Pending_Updates[i] == NULL) {
			continue;
		}

		if(is_update_bound(Pending_Updates[i]) == 1) {
			continue;
		}

		u_entry = Update_Queue.next;
		while(u_entry != NULL) {

			if(u_entry->client_update == NULL) {
				u_entry = u_entry->next;
				continue;
			}

			if( are_updates_same(u_entry->client_update, Pending_Updates[i]) == 1 ) {
				break;
			}

			u_entry = u_entry->next;
		}

		if(u_entry != NULL) {
			continue;
		}

		enqueue_update(Pending_Updates[i]);
	}
	*/

	pthread_mutex_unlock(&global_lock);
}

void remove_bound_updates_from_queue() {

	Update_Queue_entry *u_entry, *u_prev;
	Last_Exe_Enq_entry *xentry;

	pthread_mutex_lock(&global_lock);

	u_entry = Update_Queue.next;
	u_prev = &Update_Queue;

	while(u_entry != NULL) {

		if(u_entry->client_update == NULL) {
			u_prev = u_prev->next;
			u_entry = u_entry->next;
			continue;
		}

		xentry = Last_Exe_Enq.next;
		while(xentry != NULL) {
			if(xentry->client_id == u_entry->client_update->client_id) {
				break;
			}
			xentry = xentry->next;
		}
		if(xentry == NULL) {
			#ifdef DEBUG
			printf("Something is wrong in xentry\n");
			#endif
			u_prev = u_prev->next;
			u_entry = u_entry->next;
			continue;
		}

		if( (is_update_bound(u_entry->client_update) == 1) ||
			(u_entry->client_update->timestamp <= xentry->last_executed) ||
				((u_entry->client_update->timestamp <= xentry->last_enqueued) &&
				 (u_entry->client_update->server_id != my_server_id)) ) {

			u_prev->next = u_entry->next;
			if(u_entry->client_update->timestamp > xentry->last_enqueued) {
				xentry->last_enqueued = u_entry->client_update->timestamp;
			}

			free(u_entry->client_update);
			free(u_entry);

			u_entry = u_prev->next;
			continue;
		}

		/*
		if( (is_update_bound(u_entry->client_update) == 1) || 
			(u_entry->client_update->timestamp <= Last_Executed[u_entry->client_update->client_id]) ||
			(u_entry->client_update->timestamp <= Last_Enqueued[u_entry->client_update->client_id]) &&
				(u_entry->client_update->server_id != my_server_id) ) {

			u_prev->next = u_entry->next;
			if(u_entry->client_update->timestamp > Last_Enqueued[u_entry->client_update->client_id]) {
				Last_Enqueued[u_entry->client_update->client_id] = u_entry->client_update->timestamp;
			}

			free(u_entry->client_update);
			free(u_entry);

			u_entry = u_prev->next;
			continue;
		}
		*/
		u_prev = u_prev->next;
		u_entry = u_entry->next;
	}

	pthread_mutex_unlock(&global_lock);
}
