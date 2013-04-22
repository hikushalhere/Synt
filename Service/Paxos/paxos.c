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

int my_server_id;
int num_servers_g;
int state;

uint32_t Last_Attempted;
uint32_t Last_Installed;

View_Change 	**VC_g;
Prepare		*Prepare_g;
Prepare_OK	**Prepare_OK_g;

uint32_t Local_Aru;
uint32_t Last_Proposed;

Global_History_entry Global_History;

Update_Queue_entry Update_Queue;

//uint32_t Last_Executed[CLIENT_MAX];
//uint32_t Last_Enqueued[CLIENT_MAX];
//Client_Update *Pending_Updates[CLIENT_MAX];

Last_Exe_Enq_entry Last_Exe_Enq;
Pending_Update_entry Pending_Updates;

int	synch;
pthread_mutex_t global_lock, synch_lock;

struct 	sockaddr *peers_sockaddr;
int	*peers_sockets;
struct	sockaddr_in peer;

int	progress_timer;
int	progress_timer_ctr;
int	update_timer[CLIENT_MAX];
int	update_timer_ctr[CLIENT_MAX];

int	client_open_sockets[CLIENT_MAX];
int	zookeeper_socket;
void 	*paxos_socket_handler(void *);
void 	*client_socket_handler(void *);
void	*timer(void *);

int paxos(char **hostnames, uint16_t paxos_port, uint16_t server_port,  int num_servers, int my_id) {

	struct 	addrinfo 	hints, *result, *ptr;

	int	udp_listen_socket, tcp_listen_socket;
	int	retval;
	char	temp_string[100];
	int	i, j, k;

	peers_sockaddr = NULL;
	while(peers_sockaddr == NULL) {
		peers_sockaddr = (struct sockaddr *)malloc(sizeof(struct sockaddr) * (num_servers + 1));
	}

	peers_sockets = NULL;
	while(peers_sockets == NULL) {
		peers_sockets = (int *)malloc(sizeof(int) * (num_servers + 1));
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	sprintf(temp_string, "%d", paxos_port);

	retval = getaddrinfo(NULL, temp_string, &hints, &result);
	if(retval != 0) {
		printf("Error in getaddrinfo %d\n", errno);
		fflush(NULL);
		exit(1);
	}

	for(ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		udp_listen_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if(udp_listen_socket == -1) {
			continue;
		}

		retval = bind(udp_listen_socket, ptr->ai_addr, ptr->ai_addrlen);
		if(retval == 0) {
			break;
		}
	}

	if(ptr == NULL) {
		printf("Cannot bind to udp socket\n");
		fflush(NULL);
		exit(1);
	}

	freeaddrinfo(result);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	sprintf(temp_string, "%d", server_port);

	retval = getaddrinfo(NULL, temp_string, &hints, &result);
	if(retval != 0) {
		printf("Error in getaddrinfo %d\n", errno);
		fflush(NULL);
		exit(1);
	}

	for(ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		int yes;
		yes = 1;

		tcp_listen_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if(tcp_listen_socket == -1) {
			continue;
		}

		retval = setsockopt(tcp_listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if(retval == -1) {
			continue;
		}

		retval = bind(tcp_listen_socket, ptr->ai_addr, ptr->ai_addrlen);
		if(retval == -1) {
			continue;
		}

		retval = listen(tcp_listen_socket, 10);
		if(retval == 0) {
			break;
		}
	}

	if(ptr == NULL) {
		printf("Cannot bind to tcp socket\n");
		fflush(NULL);
		exit(1);
	}

	freeaddrinfo(result);

	sprintf(temp_string, "%d", paxos_port);

	for(i = 1; i <= num_servers; i++) {

		if(i == my_id) {
			continue;
		}

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;

		retval = getaddrinfo(hostnames[i], temp_string, &hints, &result);
		if(retval != 0) {
			printf("getaddrinfo error %d for host %s\n", errno, hostnames[i]);
			fflush(NULL);
			exit(1);
		}

		peers_sockaddr[i] = *result->ai_addr;
		peers_sockets[i] = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if(peers_sockets[i] == -1) {
			printf("socket error %d for host %s\n", errno, hostnames[i]);
			fflush(NULL);
			exit(1);
		}

	}

	/*
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	sprintf(temp_string, "%d", z_port);
	retval = getaddrinfo(hostnames[my_server_id], temp_string, &hints, &result);

	zookeeper_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if(zookeeper_socket == -1) {
		printf("Cannot create zookeeper socket\n");
		fflush(NULL);
		exit(1);
	}

	retval = connect(zookeeper_socket, result->ai_addr, result->ai_addrlen);
	if(retval == -1) {
		printf("Cannot connect to zookeeper server\n");
		fflush(NULL);
		exit(1);
	}
	*/

	freeaddrinfo(result);

	/* Set all the global variables */

	retval = -1;
	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);

	while(retval != 0) {
		retval = pthread_mutex_init(&global_lock, &mattr);
	}

	retval = -1;
	while(retval != 0) {
		retval = pthread_mutex_init(&synch_lock, NULL);
	}

	pthread_mutex_lock(&global_lock);

	my_server_id = my_id;
	num_servers_g = num_servers;

	Last_Attempted = Last_Installed = 0;
	
	VC_g = (View_Change **)calloc(sizeof(View_Change *), num_servers + 1);

	Prepare_g = NULL;

	Prepare_OK_g = (Prepare_OK **)calloc(sizeof(Prepare_OK *), num_servers + 1);

	Local_Aru = Last_Proposed = 0;

	Global_History.seq = UINT32_INV;
	Global_History.next = NULL;

	Update_Queue.next = NULL;

	//memset(Pending_Updates, 0, sizeof(Client_Update *) * CLIENT_MAX);
	Pending_Updates.next = NULL;
	Last_Exe_Enq.next = NULL;

	state = STATE_LEADER_ELECTION;

	for(i = 0; i < CLIENT_MAX; i++) {
		client_open_sockets[i] = -1;
	}

	progress_timer = 4;
	progress_timer_ctr = 0;

	pthread_mutex_unlock(&global_lock);

	/* Done setting global variables */

	pthread_t 	paxos_handler_thr, timer_thr, client_handler_thr[CLIENT_MAX];
	pthread_attr_t 	pattr;;

	pthread_attr_init(&pattr);

	pthread_create(&paxos_handler_thr, &pattr, paxos_socket_handler, (void *)&udp_listen_socket);

	pthread_create(&timer_thr, &pattr, timer, NULL);

	sleep(1);

	shift_to_leader_election(1);
	
	//while(1) {

	int new_sock, client_addr_len;
	struct sockaddr client_addr;
	pthread_t thr_temp;

	while(1) {
		client_addr_len = sizeof(struct sockaddr);
		new_sock = accept(tcp_listen_socket, &client_addr, &client_addr_len);
		if(new_sock == -1) {
			continue;
		}
	}

	zookeeper_socket = new_sock;

	synch = 0;
	pthread_create(&thr_temp, &pattr, client_socket_handler, (void *)&new_sock);

	while(1) {
		int done;

		done = 0;
		pthread_mutex_lock(&synch_lock);
		if(synch == 1) {
			done = 1;
		}
		pthread_mutex_unlock(&synch_lock);

		if(done == 1) {
			break;
		}
	}
	//}
	
	pthread_join(paxos_handler_thr, NULL);
	
	return 0;
}
