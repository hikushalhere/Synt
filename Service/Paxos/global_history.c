#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "message.h"
#include "globals.h"

Global_History_entry *global_history_exists(Global_History_entry *head, uint32_t seq) {

	if (head == NULL) {
		return NULL;
	}

	if(head->seq != GH_HEAD_SEQ) {
		return NULL;
	}

	if(seq == GH_HEAD_SEQ) {
		return NULL;
	}

	pthread_mutex_lock(&global_lock);

	Global_History_entry *curr = head->next;

	while(curr != NULL) {
		if(curr->seq == seq)
			break;
		curr = curr->next;
	}

	pthread_mutex_unlock(&global_lock);

	return curr;
}

Global_History_entry *new_global_history_entry(uint32_t seq) {

	Global_History_entry *entry = NULL;
	while(entry == NULL) {
		entry = malloc(sizeof(Global_History_entry));
	}

	entry->seq = seq;
	entry->proposal = NULL;
	entry->accepts = NULL;
	entry->ordered = NULL;
	entry->next = NULL;

	return entry;
}

int insert_into_global_history(Global_History_entry *head, Global_History_entry *new) {

	if( (head == NULL) || (new == NULL) ) {
		return -1;
	}

	if(head->seq != GH_HEAD_SEQ) {
		return -1;
	}

	if(new->seq == GH_HEAD_SEQ) {
		return -1;
	}

	if(global_history_exists(head, new->seq)) {
		return -1;
	}


	pthread_mutex_lock(&global_lock);

	Global_History_entry *curr = head->next, *prev = head;

	while(curr != NULL) {
		if(curr->seq < new->seq) {
			curr = curr->next;
			prev = prev->next;
		}
		else {
			break;
		}
	}

	new->next = curr;
	prev->next = new;

	pthread_mutex_unlock(&global_lock);

	return 1;
}

int global_history_insert_proposal(Global_History_entry *head, Proposal *p) {

	Global_History_entry *entry;
	int	i;

	if( (head == NULL) || (p == NULL) ) {
		return -1;
	}

	if(head->seq != GH_HEAD_SEQ) {
		return -1;
	}

	if(p->seq == GH_HEAD_SEQ) {
		return -1;
	}

	pthread_mutex_lock(&global_lock);

	entry = global_history_exists(head, p->seq);

	if(entry != NULL) {
	
		if(entry->ordered != NULL) {
			pthread_mutex_unlock(&global_lock);
			return -1;
		}

		if(entry->proposal != NULL) {
			if(p->view > entry->proposal->view) {
				free(entry->proposal);
				entry->proposal = NULL;
				while(entry->proposal == NULL) {
					entry->proposal = malloc(sizeof(Proposal));
				}
				*entry->proposal = *p;

				if(entry->accepts != NULL) {
					for(i = 1; i <= num_servers_g; i++) {
						if(entry->accepts[i]) {
							free(entry->accepts[i]);
						}
					}
					free(entry->accepts);
					entry->accepts = NULL;
				}
			}
		}
		else {
			while(entry->proposal == NULL) {
				entry->proposal = malloc(sizeof(Proposal));
			}
			*entry->proposal = *p;
		}
	}
	else {
		while(entry == NULL) {
			entry = new_global_history_entry(p->seq);
		}
		while(entry->proposal == NULL) {
			entry->proposal = malloc(sizeof(Proposal));
		}
		*entry->proposal = *p;

		insert_into_global_history(head, entry);
	}

	Last_Exe_Enq_entry *xentry = Last_Exe_Enq.next;
	while(xentry != NULL) {
		if(xentry->client_id == p->update.client_id) {
			break;
		}
		xentry = xentry->next;
	}
	if(xentry == NULL) {
		while(xentry == NULL) {
			xentry = malloc(sizeof(Last_Exe_Enq_entry));
		}
		xentry->client_id = p->update.client_id;
		xentry->last_executed = 0;
		xentry->last_enqueued = 0;
		xentry->next = Last_Exe_Enq.next;
		Last_Exe_Enq.next = xentry;
	}

	#ifdef DEBUG
	printf("prop upd ts %d, cid %d\n", p->update.timestamp, p->update.client_id);
	#endif
	if(p->update.timestamp > xentry->last_enqueued) {
		xentry->last_enqueued = p->update.timestamp;
	}

	pthread_mutex_unlock(&global_lock);

	return 1;
}

int global_history_insert_accept(Global_History_entry *head, Accept *a) {

	if( (head == NULL) || (a == NULL) ) {
		return -1;
	}

	if(head->seq != GH_HEAD_SEQ) {
		return -1;
	}

	if(a->seq == GH_HEAD_SEQ) {
		return -1;
	}

	pthread_mutex_lock(&global_lock);

	Global_History_entry *entry = global_history_exists(head, a->seq);
	int 	i, num_accepts;

	if(entry != NULL) {

		if(entry->ordered != NULL) {
			pthread_mutex_unlock(&global_lock);
			return -1;
		}

		if(entry->accepts != NULL) {

			num_accepts = 0;
			for(i = 1; i <= num_servers_g; i++) {

				if(entry->accepts[i] != NULL)
					num_accepts++;
			}

			if(num_accepts >= (num_servers_g/2)) {
				pthread_mutex_unlock(&global_lock);
				return -1;
			}

			if(entry->accepts[a->server_id] != NULL) {
				pthread_mutex_unlock(&global_lock);
				return -1;
			}

			while(entry->accepts[a->server_id] == NULL) {
				entry->accepts[a->server_id] = malloc(sizeof(Accept));
			}
			*entry->accepts[a->server_id] = *a;
		}
		else {
			while(entry->accepts == NULL) {
				entry->accepts = (Accept **)calloc(sizeof(Accept *), num_servers_g + 1);
			}

			while(entry->accepts[a->server_id] == NULL) {
				entry->accepts[a->server_id] = malloc(sizeof(Accept));
			}
			*entry->accepts[a->server_id] = *a;
		}
	}
	else {
		entry = new_global_history_entry(a->seq);

		while(entry->accepts == NULL) {
			entry->accepts = (Accept **)calloc(sizeof(Accept *), num_servers_g + 1);
		}

		while(entry->accepts[a->server_id] == NULL) {
			entry->accepts[a->server_id] = malloc(sizeof(Accept));
		}
		*entry->accepts[a->server_id] = *a;

		insert_into_global_history(head, entry);
	}

	pthread_mutex_unlock(&global_lock);
	return 1;
}

int global_history_insert_gou(Global_History_entry *head, Globally_Ordered_Update *g) {

	if( (head == NULL) || (g == NULL) ) {
		return -1;
	}

	if(head->seq != GH_HEAD_SEQ) {
		return -1;
	}

	if(g->seq == GH_HEAD_SEQ) {
		return -1;
	}

	pthread_mutex_lock(&global_lock);

	Global_History_entry *entry = global_history_exists(head, g->seq);

	if(entry != NULL) {

		if(entry->ordered == NULL) {
			while(entry->ordered == NULL) {
				entry->ordered = (Globally_Ordered_Update *)malloc(sizeof(Globally_Ordered_Update));
			}

			entry->ordered->type = g->type;
			entry->ordered->seq = g->seq;
			entry->ordered->server_id = g->server_id;
			entry->ordered->update = g->update;
		}
	}
	else {
		entry = new_global_history_entry(g->seq);
		if(insert_into_global_history(head, entry) != -1) {
			while(entry->ordered == NULL) {
				entry->ordered = (Globally_Ordered_Update *)malloc(sizeof(Globally_Ordered_Update));
			}

			entry->ordered->type = g->type;
			entry->ordered->seq = g->seq;
			entry->ordered->server_id = g->server_id;
			entry->ordered->update = g->update;
		}
		else {
			pthread_mutex_unlock(&global_lock);
			return -1;
		}
	}

	pthread_mutex_unlock(&global_lock);
	return 1;
}

void traverse_global_history(Global_History_entry *head) {

	if(head == NULL) {
		return;
	}

	pthread_mutex_lock(&global_lock);

	Global_History_entry *entry = head->next;

	while(entry != NULL) {

		printf("%d: ", entry->seq);
		if(entry->proposal) {
			printf("p:%d ", entry->proposal->view);
		}
		if(entry->ordered) {
			printf("o ");
		}
		printf(" :: ");
		entry = entry->next;
	}
	printf("\n");

	Last_Exe_Enq_entry *xentry = Last_Exe_Enq.next;
	while(xentry != NULL) {
		printf("cid %d, enq %d, exe %d ::", xentry->client_id, xentry->last_enqueued, xentry->last_executed);
		xentry = xentry->next;
	}
	printf("\n");
	pthread_mutex_unlock(&global_lock);
}
