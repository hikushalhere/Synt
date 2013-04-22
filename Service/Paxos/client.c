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
#include <getopt.h>
#include "message.h"

int main(int argc, char *argv[]) {

	char		c;
	uint16_t	server_port;
	char		*command_file, *hostfile, temp_string[100];
	uint32_t	my_client_id;
	int		s_flag, f_flag, i_flag, h_flag;
	FILE		*hf, *cf;

	struct 	addrinfo hints, *result;
	int	server_socket, retval;
	int	i, j, k;

	int	num_servers, num_commands;
	char	**servers;
	char	**cmd_servers;
	uint32_t *cmd_updates;
	uint32_t timestamp = 1;

	Client_Update cu_to_send;

	s_flag = f_flag = i_flag = h_flag = 0;

	if(argc < 9) {
		printf("USAGE:%s -s <server_port> -f <command_file> -i <client_id> -h <hostfile>\n", argv[0]);
		exit(1);
	}

	while( (c = getopt(argc, argv, "s:f:i:h:")) != -1) {

		switch(c) {

			case 's':
				if(optarg == 0) {
					printf("Option -s requires an argument\n");
					exit(1);
				}
				server_port = atoi(optarg);
				if(server_port <= 1024) {
					printf("Bad argument %d for option -s, must be greater than 1024\n", server_port);
					exit(1);
				}
				s_flag = 1;
				break;

			case 'f':
				if(optarg == 0) {
					printf("Option -f requires an argument\n");
					exit(1);
				}
				command_file = NULL;
				while(command_file == NULL) {
					command_file = calloc(sizeof(char), strlen(optarg) + 1);
				}
				strncpy(command_file, optarg, strlen(optarg));
				f_flag  = 1;
				break;

			case 'i':
				if(optarg == 0) {
					printf("Option -i requires an argument\n");
					exit(1);
				}
				my_client_id = atoi(optarg);
				i_flag = 1;
				break;

			case 'h':
				if(optarg == 0) {
					printf("Option -h requires an argument\n");
					exit(1);
				}
				hostfile = NULL;
				while(hostfile == NULL) {
					hostfile = calloc(sizeof(char), strlen(optarg) + 1);
				}
				strncpy(hostfile, optarg, strlen(optarg));
				h_flag = 1;
				break;

			case '?':
				exit(1);
		}
	}

	if(s_flag == 0) {
		printf("Missing option -s\n");
		exit(1);
	}

	if(f_flag == 0) {
		printf("missing option -f\n");
		exit(1);
	}

	if(i_flag == 0) {
		printf("Missing option -i\n");
		exit(1);
	}

	if(h_flag == 0) {
		printf("Missing option -h\n");
		exit(1);
	}

	#ifdef DEBUG
	printf("server_port %d, command_file %s, hostfile %s, client id %d\n", server_port, command_file, hostfile, my_client_id);
	#endif

	hf = fopen(hostfile, "r");
	if(hf == NULL) {
		printf("Cannot read hostfile %s\n", hostfile);
		exit(1);
	}

	num_servers = 0;
	while(fscanf(hf, "%s", temp_string) == 1) {
		num_servers++;
	}
	fclose(hf);

	hf = fopen(hostfile, "r");
	if(hf == NULL) {
		printf("Cannot read hostfile %s\n", hostfile);
		exit(1);
	}

	servers = NULL;
	while(servers == NULL) {
		servers = (char **)malloc(sizeof(char *) * (num_servers+1));
	}

	for(i = 1; i <= num_servers; i++) {
	
		servers[i] = NULL;
		while(servers[i] == NULL) {
			servers[i] = malloc(100);
		}
		fscanf(hf, "%s", servers[i]);
	}
	fclose(hf);

	cf = fopen(command_file, "r");
	if(cf == NULL) {
		printf("Cannot open command file %s\n", command_file);
		exit(1);
	}

	num_commands = 0;
	while(fscanf(cf, "%s %d", temp_string, &j) == 2) {
		num_commands++;
	}
	fclose(cf);

	cf = fopen(command_file, "r");
	if(cf == NULL) {
		printf("Cannot open command file %s\n", command_file);
		exit(1);
	}

	cmd_servers = NULL;
	while(cmd_servers == NULL) {
		cmd_servers = (char **)malloc(sizeof(char *) * num_commands);
	}
	cmd_updates = NULL;
	while(cmd_updates == NULL) {
		cmd_updates = (uint32_t *)malloc(sizeof(uint32_t) * num_commands);
	}

	for(i = 0; i < num_commands; i++) {

		cmd_servers[i] = malloc(100);

		fscanf(cf, "%s %u", cmd_servers[i], &cmd_updates[i]);
	}

	#ifdef DEBUG
	printf("num_commands = %d\n", num_commands);
	#endif

	for(i = 0; i < num_commands; i++) {

		for(j=1; j <= num_servers; j++) {
			if(strncmp(servers[j], cmd_servers[i], strlen(cmd_servers[i])) == 0) {
				break;
			}
		}
		#ifdef DEBUG
		printf("cmd: server id %d, update %d\n", j, cmd_updates[i]);
		#endif
	}


	for(i = 0; i < num_commands; i++) {

		for(j = 1; j <= num_servers; j++) {
			if(strncmp(servers[j], cmd_servers[i], strlen(cmd_servers[i])) == 0) {
				break;
			}
		}
		if(j > num_servers) {
			continue;
		}

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		sprintf(temp_string, "%d", server_port);
		retval = getaddrinfo(cmd_servers[i], temp_string, &hints, &result);
		if(retval == -1) {
			printf("getaddrinfo error %d for server %s\n", errno, cmd_servers[i]);
			continue;
		}

		server_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if(server_socket == -1) {
			printf("cannot create socket for server %s, errno %d\n", cmd_servers[i], errno);
			continue;
		}

		retval = connect(server_socket, result->ai_addr, result->ai_addrlen);
		if(retval == -1) {
			printf("Unable to send update %d to server %d\n", cmd_updates[i], j);
			continue;
		}

		cu_to_send.type = htonl(TYPE_CLIENT_UPDATE);
		cu_to_send.server_id = htonl(j);
		cu_to_send.client_id = htonl(my_client_id);
		cu_to_send.timestamp = htonl(timestamp);
		cu_to_send.update = htonl(cmd_updates[i]);
		timestamp++;

		printf("%d: Sending update %d to server %d\n", my_client_id, cmd_updates[i], j);

		retval = send(server_socket, &cu_to_send, sizeof(Client_Update), 0);
		if(send < 0) {
			printf("Unable to send update %d to server %d\n", cmd_updates[i], j);
		}

		retval = recv(server_socket, temp_string, 2, 0);

		if(retval <= 0) {
			printf("%d: Connection for update %d is closed by server %d\n", my_client_id, cmd_updates[i], j);
			close(server_socket);
		}
		else if(retval == 2){
			printf("%d: Update %d sent to server %d is executed\n", my_client_id, cmd_updates[i], j);
			close(server_socket);
		}
	}

	return 0;
}
