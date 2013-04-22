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

void *client_socket_handler(void *arg) {

	int	client_sock, retval;
	uint8_t *buf;
	Client_Update *cu;

	pthread_mutex_lock(&synch_lock);
	client_sock = *((uint32_t *)arg);
	synch = 1;
	pthread_mutex_unlock(&synch_lock);

	buf = (uint8_t *)malloc(1000);

	#ifdef DEBUG
	printf("Connection from client\n");
	#endif

	while(1) {
	
		retval = recv(client_sock, buf, sizeof(Client_Update), 0);

		#ifdef DEBUG
		printf("recv retval %d\n", retval);
		#endif

		if(retval == 0) {
			continue;
		}

		cu = (Client_Update *)buf;

		cu->type = ntohl(cu->type);
		cu->client_id = ntohl(cu->client_id);
		cu->server_id = ntohl(cu->server_id);
		cu->timestamp = ntohl(cu->timestamp);
		cu->update = ntohl(cu->update);

		if(cu->type != TYPE_CLIENT_UPDATE) {
			//close(client_sock);
			//client_open_sockets[cu->client_id] = -1;
			//return NULL;
			continue;
		}

		if(cu->server_id != my_server_id) {
			//client_open_sockets[cu->client_id] = -1;
			//close(client_sock);
			//return NULL;
			continue;
		}

		/* Added later */
		//close(client_sock);
		//client_open_sockets[cu->client_id] = client_sock;

		client_update_handler(cu, client_sock);

		//pthread_mutex_unlock(&global_lock);
	}
}
