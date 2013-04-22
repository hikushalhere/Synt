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

void shift_to_leader_election(uint32_t view) {

	int		i, retval;
	View_Change	*vc_msg;

	#ifdef DEBUG
	printf("stle with %d\n", view);
	#endif

	pthread_mutex_lock(&global_lock);

	if(VC_g != NULL) {

		for(i = 1; i <= num_servers_g; i++) {
			if(VC_g[i] != NULL) {
				free(VC_g[i]);
				VC_g[i] = NULL;
			}
		}
	}
	else {
		while(VC_g == NULL) {
			VC_g = (View_Change **)calloc(sizeof(View_Change *), num_servers_g + 1);
		}
	}

	if(Prepare_g != NULL) {
		free(Prepare_g);
		Prepare_g = NULL;
	}

	if(Prepare_OK_g != NULL) {
		for(i = 1; i <= num_servers_g; i++) {

			if(Prepare_OK_g[i] == NULL) {
				continue;
			}

			free(Prepare_OK_g[i]);
			Prepare_OK_g[i] = NULL;
		}
	}

	Last_Exe_Enq_entry *xentry = Last_Exe_Enq.next;
	while(xentry != NULL) {
		xentry->last_enqueued = 0;
		xentry = xentry->next;
	}
	/*
	for(i = 0; i < CLIENT_MAX; i++) {
		Last_Enqueued[i] = 0;
	}
	*/
	Last_Attempted = view;

	state = STATE_LEADER_ELECTION;

	vc_msg = NULL;
	while(vc_msg == NULL) {
		vc_msg = (View_Change *)malloc(sizeof(View_Change));
	}

	vc_msg->type = (TYPE_VIEW_CHANGE);
	vc_msg->server_id = (my_server_id);
	vc_msg->attempted = (view);

	VC_g[my_server_id] = vc_msg;
	
	//TODO Send to all servers, change to network byte order

	vc_msg->type = htonl(vc_msg->type);
	vc_msg->server_id = htonl(vc_msg->server_id);
	vc_msg->attempted = htonl(vc_msg->attempted);

	for(i = 1; i <= num_servers_g; i++) {

		if(i == my_server_id) {
			continue;
		}

		#ifdef DEBUG
		printf("in stle, sending to %d\n", i);
		#endif
		retval = sendto(peers_sockets[i], vc_msg, sizeof(View_Change), 0, &peers_sockaddr[i], sizeof(struct sockaddr));
		if(retval == -1) {
			printf("send error %d while sending to %d\n", errno, i);
			fflush(NULL);
		}
	}

	vc_msg->type = htonl(vc_msg->type);
	vc_msg->server_id = htonl(vc_msg->server_id);
	vc_msg->attempted = htonl(vc_msg->attempted);

	pthread_mutex_unlock(&global_lock);
}

/* Assumes lock */
int preinstall_ready(uint32_t view) {

	int	num_vcs = 0, i;


	if(VC_g == NULL) {
		return -1;
	}

	for(i = 1; i <= num_servers_g; i++) {

		if(VC_g[i] == NULL) {
			continue;
		}

		if(VC_g[i]->attempted != view) {
			continue;
		}

		num_vcs++;
	}

	if(num_vcs > (num_servers_g/2)) {
		return 1;
	}
	else {
		return -1;
	}
}

void process_incoming_view_change(View_Change *vc) {

	if(conflict_view_change(vc) == 1) {
		return;
	}

	pthread_mutex_lock(&global_lock);
	#ifdef DEBUG
	printf("inc vc, no conflicts, %d, %d\n", Last_Attempted, vc->attempted);
	#endif

	if( (vc->attempted > Last_Attempted) && (progress_timer_ctr <= 0) ) { //TODO Progress timer

		shift_to_leader_election(vc->attempted);

		if(VC_g != NULL) {
			if(VC_g[vc->server_id] == NULL) {

				while(VC_g[vc->server_id] == NULL) {
					VC_g[vc->server_id] = (View_Change *)malloc(sizeof(View_Change));
				}

				VC_g[vc->server_id]->type = TYPE_VIEW_CHANGE;
				VC_g[vc->server_id]->server_id = vc->server_id;
				VC_g[vc->server_id]->attempted = vc->attempted;
			}
		}
		else {
			while(VC_g == NULL) {
				VC_g = calloc(sizeof(View_Change *), num_servers_g + 1);
			}

			while(VC_g[vc->server_id] == NULL) {
				VC_g[vc->server_id] = malloc(sizeof(View_Change));
			}

			*VC_g[vc->server_id] = *vc;
		}
	}
	else if(vc->attempted == Last_Attempted) {

		if(VC_g != NULL) {
			if(VC_g[vc->server_id] == NULL) {

				while(VC_g[vc->server_id] == NULL) {
					VC_g[vc->server_id] = (View_Change *)malloc(sizeof(View_Change));
				}

				VC_g[vc->server_id]->type = TYPE_VIEW_CHANGE;
				VC_g[vc->server_id]->server_id = vc->server_id;
				VC_g[vc->server_id]->attempted = vc->attempted;
			}
		}
		else {
			while(VC_g == NULL) {
				VC_g = calloc(sizeof(View_Change *), num_servers_g + 1);
			}

			while(VC_g[vc->server_id] == NULL) {
				VC_g[vc->server_id] = malloc(sizeof(View_Change));
			}

			*VC_g[vc->server_id] = *vc;
		}

		if(preinstall_ready(vc->attempted) == 1) {
			#ifdef DEBUG
			printf("preinstall ready for view %d\n", vc->attempted);
			#endif
			//TODO Progress timer
			progress_timer *= 2;
			if(progress_timer > 5) {
				progress_timer = 5;
			}
			progress_timer_ctr = progress_timer;
			if( ( ((Last_Attempted + 1) % num_servers_g) + 1) == my_server_id) {
				progress_timer_ctr -= 2;
			}
			#ifdef DEBUG
			printf("progress timer set at %d\n", progress_timer_ctr);
			#endif

			// Shift to prepare phase
			if( ((Last_Attempted % num_servers_g) + 1) == my_server_id ) {
				shift_to_prepare_phase();
			}
		}
	}

	pthread_mutex_unlock(&global_lock);
}

void shift_to_reg_non_leader() {

	pthread_mutex_lock(&global_lock);

	state = STATE_REG_NONLEADER;
	
	Last_Installed = Last_Attempted;

	printf("%d: Server %d is the new leader of the view %d\n", my_server_id, (Last_Installed % num_servers_g) + 1, Last_Installed);
	//TODO Clear the update queue
	
	Update_Queue_entry *curr, *next;
	curr = Update_Queue.next;
	while(curr != NULL) {

		next = curr->next;
		if(curr->client_update) {
			free(curr->client_update);
		}
		free(curr);

		curr = next;
	}
	Update_Queue.next = NULL;

	pthread_mutex_unlock(&global_lock);
}

void shift_to_reg_leader() {

	pthread_mutex_lock(&global_lock);

	state = STATE_REG_LEADER;

	printf("%d: Server %d is the new leader of the view %d\n", my_server_id, (Last_Installed % num_servers_g) + 1, Last_Installed);
	//TODO enqueue_unbound_pending_updates()
	//TODO remove_bound_updates_from_queue()
	enqueue_unbound_pending_updates();

	#ifdef DEBUG
	printf("done enq pend upd\n");
	#endif

	remove_bound_updates_from_queue();

	#ifdef DEBUG
	printf("done remv bound upd\n");
	#endif

	advance_aru();

	Last_Proposed = Local_Aru;

	//TODO Send_Proposal()
	send_proposal();
	
	pthread_mutex_unlock(&global_lock);
}

void process_incoming_vc_proof(VC_Proof *vcp) {

	int	i, retval;

	if(conflict_vc_proof(vcp) == 1) {
		return;
	}

	pthread_mutex_lock(&global_lock);

	if(vcp->installed > Last_Installed) {

		Last_Attempted = vcp->installed;

		progress_timer *= 2;
		if(progress_timer > 5) {
			progress_timer = 5;
		}
		progress_timer_ctr = progress_timer;
		if( ( ((Last_Attempted + 1) % num_servers_g) + 1) == my_server_id) {
			progress_timer_ctr -= 2;
		}

		if( ((Last_Attempted % num_servers_g) + 1) == my_server_id) {
			shift_to_prepare_phase();
		}
		else {
			shift_to_reg_non_leader();
		}
	

		vcp->type = htonl(vcp->type);
		vcp->server_id = htonl(my_server_id);
		vcp->installed = htonl(vcp->installed);
		
		for(i = 1; i <= num_servers_g; i++) {

			if(i == my_server_id) {
				continue;
			}

			retval = sendto(peers_sockets[i], vcp, sizeof(VC_Proof), 0, &peers_sockaddr[i], sizeof(struct sockaddr));
			if(retval == -1) {
				printf("send error %d while sending to %d\n", errno, i);
				fflush(NULL);
			}
		}
	}

	pthread_mutex_unlock(&global_lock);
}
