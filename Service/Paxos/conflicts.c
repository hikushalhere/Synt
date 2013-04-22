#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "message.h"
#include "globals.h"

/* All conflict messages assume that fields are in host byte order */

int conflict_view_change(View_Change *vc) {

	int retval = -1;
	if(vc == NULL) {
		retval = 1;
	}

	pthread_mutex_lock(&global_lock);

	if(vc->server_id == my_server_id) {
		#ifdef DEBUG
		printf("vc conflict srv id\n");
		#endif
		retval = 1;
	}

	if(state != STATE_LEADER_ELECTION) {
		#ifdef DEBUG
		printf("vc conflict state %d\n", state);
		#endif
		retval = 1;
	}

	//TODO PROGRESS TIMER
	if(progress_timer_ctr > 0) {
		#ifdef DEBUG
		printf("vc conflict tmr\n");
		#endif
		retval = 1;
	}
	
	if(vc->attempted <= Last_Installed) {
		#ifdef DEBUG
		printf("vc conflict att %d li %d\n", vc->attempted, Last_Installed);
		#endif
		retval = 1;
	}

	pthread_mutex_unlock(&global_lock);

	return retval;
}

int conflict_vc_proof(VC_Proof *vc_proof) {

	int retval = -1;

	pthread_mutex_lock(&global_lock);

	if(vc_proof == NULL) {
		retval = 1;
	}

	if(vc_proof->server_id == my_server_id) {
		retval = 1;
	}

	if(state != STATE_LEADER_ELECTION) {
		retval = 1;
	}

	pthread_mutex_unlock(&global_lock);

	return retval;
}

int conflict_prepare(Prepare *p) {

	int retval = -1;

	if(p == NULL) {
		retval = 1;
	}

	pthread_mutex_lock(&global_lock);

	if(p->server_id == my_server_id) {
		retval = 1;
	}

	if(p->view != Last_Attempted) {
		#ifdef DEBUG
		printf("prepare conflict view %d\n", p->view);
		#endif
		retval = 1;
	}

	pthread_mutex_unlock(&global_lock);

	return retval;
}

int conflict_prepare_ok(Prepare_OK *p_ok) {

	int retval = -1;

	if(p_ok == NULL) {
		retval = 1;
	}

	pthread_mutex_lock(&global_lock);

	if(state != STATE_LEADER_ELECTION) {
		#ifdef DEBUG
		printf("pok conflict state %d\n", state);
		#endif
		retval = 1;
	}

	if(p_ok->view != Last_Attempted) {
		#ifdef DEBUG
		printf("pok conflict view %d\n", p_ok->view);
		#endif
		retval = 1;
	}

	pthread_mutex_unlock(&global_lock);

	return retval;
}

int conflict_proposal(Proposal *p) {

	int retval = -1;

	if(p == NULL) {
		#ifdef DEBUG
		printf("prop conflict, null\n");
		#endif
		return 1;
	}

	pthread_mutex_lock(&global_lock);

	if(p->server_id == my_server_id) {
		#ifdef DEBUG
		printf("prop conflict server_id\n");
		#endif
		retval = 1;
	}

	if(state != STATE_REG_NONLEADER) {
		#ifdef DEBUG
		printf("prop conflict state %d\n", state);
		#endif
		retval = 1;
	}

	if(p->view != Last_Installed) {
		#ifdef DEBUG
		printf("prop conflict view %d li %d\n", p->view, Last_Installed);
		#endif
		retval = 1;
	}

	pthread_mutex_unlock(&global_lock);

	return retval;
}

int conflict_accept(Accept *a) {

	int retval = -1;

	if(a == NULL) {
		return 1;
	}

	pthread_mutex_lock(&global_lock);

	if(a->server_id == my_server_id) {
		retval = 1;
	}

	if(a->view != Last_Installed) {
		#ifdef DEBUG
		printf("accept conflict view %d\n", a->view);
		#endif
		retval = 1;
	}

	pthread_mutex_unlock(&global_lock);

	Global_History_entry *entry = global_history_exists(&Global_History, a->seq);

	pthread_mutex_lock(&global_lock);

	if(entry == NULL) {
		retval = 1;
	}
	else if(entry->proposal == NULL) {
		#ifdef DEBUG
		printf("accept conflict no prop\n");
		#endif
		retval = 1;
	}
	else if (entry->proposal->view != a->view) {
		#ifdef DEBUG
		printf("accept conflict prop view\n");
		#endif
		retval = 1;
	}

	pthread_mutex_unlock(&global_lock);

	return retval;
}
