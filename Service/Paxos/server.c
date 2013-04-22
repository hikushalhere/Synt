#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>

#define DEBUG

int main(int argc, char *argv[]) {

	char 		c;
	uint16_t	udp_port, tcp_port, z_port;
	char 		*hostfile;
	int		h_flag, p_flag, s_flag, z_flag;
	int		num_servers, my_server_id, i;
	char 		my_hostname[100], temp_string[100];
	char		**hostnames;
	int		retval;
	FILE		*hf;

	if(argc < 7) {
		printf("Usage: %s -h hostfile -p paxos_port -s server_port\n", argv[0]);
		exit(1);
	}

	p_flag = h_flag = s_flag = 0, z_flag = 0;
	while( (c = getopt(argc, argv, "h:p:s:")) != -1 ) {

		switch(c) {

			case 'h':
				if(optarg == 0) {
					printf("Option -h requires an argument\n");
					exit(1);
				}
				hostfile = (char *)calloc(sizeof(char), strlen(optarg) + 1);
				if(hostfile == NULL) {
					printf("Malloc error\n");
					exit(1);
				}
				strncpy(hostfile, optarg, strlen(optarg));
				h_flag = 1;
				break;

			case 'p':
				if(optarg == 0) {
					printf("Option -p requires an argument\n");
					exit(1);
				}
				udp_port = atoi(optarg);
				if( udp_port < 1024 ) {
					printf("Bad argument for -p %d, must be greater than 1024\n", udp_port);
					exit(1);
				}
				p_flag = 1;
				break;

			case 's':
				if(optarg == 0) {
					printf("Option -s requires an argument\n");
					exit(1);
				}
				tcp_port = atoi(optarg);
				if( tcp_port < 1024 ) {
					printf("Bad argument for -s %d, must be greater than 1024\n", tcp_port);
					exit(1);
				}
				s_flag = 1;
				break;

			case '?':
				exit(1);
		}
	}

	if(h_flag == 0) {
		printf("Missing option -h\n");
		printf("Usage: %s -h hostfile -p paxos_port -s server_port -z zookeeper_port\n", argv[0]);
		exit(1);
	}
	if(p_flag == 0) {
		printf("Missing option -p\n");
		printf("Usage: %s -h hostfile -p paxos_port -s server_port -z zookeeper_port\n", argv[0]);
		exit(1);
	}
	if(s_flag == 0) {
		printf("Missing option -s\n");
		printf("Usage: %s -h hostfile -p paxos_port -s server_port -z zookeeper_port\n", argv[0]);
		exit(1);
	}

	#ifdef DEBUG
	printf("hostfile %s, udp_port %d, tcp_port %d\n",hostfile,udp_port,tcp_port);
	#endif

	retval = gethostname(my_hostname, 99);
	if(retval == -1) {
		printf("Cannot get hostname\n");
		exit(1);
	}

	#ifdef DEBUG
	printf("Hostname: %s\n", my_hostname);
	#endif

	hf = fopen(hostfile, "r");
	if(hf == NULL) {
		printf("Cannot open hostfile %s\n", hostfile);
		exit(1);
	}

	num_servers = 0;
	while(fscanf(hf, "%s", temp_string) == 1) {
		num_servers++;
	}

	fclose(hf);

	hostnames = (char **)malloc(sizeof(char *) * (num_servers+1));
	if(hostnames == NULL) {
		printf("Malloc error\n");
		exit(1);
	}

	hf = fopen(hostfile, "r");
	if(hf == NULL) {
		printf("Cannot open hostfile %s\n", hostfile);
		exit(1);
	}

	my_server_id = -1;
	for(i = 1; i <= num_servers; i++) {
		hostnames[i] = (char *)malloc(sizeof(char) * 100);
		fscanf(hf, "%s", hostnames[i]);
		if(strncmp(hostnames[i], my_hostname, strlen(my_hostname)) == 0) {
			my_server_id = i;
		}
	}

	if(my_server_id == -1) {
		printf("My hostname not present in hostfile\n");
		exit(1);
	}

	#ifdef DEBUG
	printf("My_server_id %d\n", my_server_id);
	#endif

	fflush(NULL);

	paxos(hostnames, udp_port, tcp_port, num_servers, my_server_id);

	return 0;
}
