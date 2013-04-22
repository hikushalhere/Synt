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

void *timer(void *arg) {

	int	i, retval, leader;
	VC_Proof	vcp_msg;
	Client_Update	u;

	vcp_msg.type = htonl(TYPE_VC_PROOF);
	vcp_msg.server_id = htonl(my_server_id);

	Pending_Update_entry *p_entry;

	while(1) {

		sleep(1);

		pthread_mutex_lock(&global_lock);
		if(progress_timer_ctr > 0) {
			progress_timer_ctr--;
			if(progress_timer_ctr == 0) {
				#ifdef DEBUG
				printf("calling stle with view %d\n", Last_Attempted + 1);
				#endif
				shift_to_leader_election(Last_Attempted + 1);
			}
		}

		p_entry = Pending_Updates.next;
		while(p_entry != NULL) {
			if(p_entry->client_update == NULL) {
				p_entry = p_entry->next;
				continue;
			}

			if(p_entry->update_timer > 0) {
				p_entry->update_timer--;
				if(p_entry->update_timer == 0) {
				
					p_entry->update_timer = 1;

					u.type = htonl(TYPE_CLIENT_UPDATE);
					u.client_id = htonl(p_entry->client_update->client_id);
					u.server_id = htonl(p_entry->client_update->server_id);
					u.timestamp = htonl(p_entry->client_update->timestamp);
					u.update = htonl(p_entry->client_update->update);

					if(state == STATE_REG_NONLEADER) {
						leader = (Last_Installed % num_servers_g) + 1;

						retval = sendto(peers_sockets[leader], &u, sizeof(Client_Update), 0, &peers_sockaddr[leader], sizeof(struct sockaddr));
						if(retval == -1) {
							printf("send error %d while sending to %d\n", errno, leader);
							fflush(NULL);
						}
					}
				}
			}
			p_entry = p_entry->next;

		}
		/*
		for(i = 0; i < CLIENT_MAX; i++) {

			if(update_timer_ctr[i] > 0) {
				update_timer_ctr[i]--;
				if(update_timer_ctr[i] == 0) {

					if(Pending_Updates[i] == NULL) {
						continue;
					}

					update_timer_ctr[i] = 1;

					//TODO Send Pending_Updates[i] to leader.

					u.type = htonl(TYPE_CLIENT_UPDATE);
					u.client_id = htonl(Pending_Updates[i]->client_id);
					u.server_id = htonl(Pending_Updates[i]->server_id);
					u.timestamp = htonl(Pending_Updates[i]->timestamp);
					u.update = htonl(Pending_Updates[i]->update);

					if(state == STATE_REG_NONLEADER) {
						leader = (Last_Installed % num_servers_g) + 1;

						retval = sendto(peers_sockets[leader], &u, sizeof(Client_Update), 0, &peers_sockaddr[leader], sizeof(struct sockaddr));
						if(retval == -1) {
							printf("send error %d while sending to %d\n", errno, leader);
							fflush(NULL);
						}
					}

				}
			}
		}
		*/
		vcp_msg.installed = htonl(Last_Installed);
		
		for(i = 1; i<= num_servers_g; i++) {

			if(i == my_server_id) {
				continue;
			}

			retval = sendto(peers_sockets[i], &vcp_msg, sizeof(VC_Proof), 0, &peers_sockaddr[i], sizeof(struct sockaddr));
			if(retval == -1) {
				printf("send error %d while sending to %d\n", errno, i);
				fflush(NULL);
			}
		}
		
		pthread_mutex_unlock(&global_lock);
		fflush(NULL);
	}

	return NULL;
}
