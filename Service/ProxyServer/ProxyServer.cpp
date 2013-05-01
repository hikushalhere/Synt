#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdint.h>
#include <getopt.h>
#include <sys/time.h>
#include "ProxyServer.h"
using namespace std;

void *heart_beat_fn(void *);

ProxyServer::ProxyServer() {

	client_listen_port = 0;
	svr_listen_port = 0;
	client_listen_socket = 0;
	svr_listen_socket = 0;
	next_cid = 0;
}

int main(int argc, char *argv[]) {

	char		c, *hostfile, temp_string[100];
	uint16_t	listen_port, server_port;
	bool		h_flag = false; 
	bool		c_flag = false; 
	bool		s_flag = false;
	FILE		*hf;
	int		num_servers, i, j, k, retval;

	struct addrinfo	hints, *result, *ptr;

	ProxyServer pxy;

	if(argc < 7) {
		cout << "Usage: " << argv[0] << " -h <hostfile> -c <client_listen_port> -s <zk_server_port>\n";
		exit(1);
	}

	while( (c = getopt(argc, argv, "h:c:s:")) != -1 ) {

		switch(c) {

			case 'h':
				if(optarg == 0) {
					cout << "Option -h requires an argument\n";
					exit(1);
				}
				hostfile = (char *)calloc(sizeof(char), strlen(optarg) + 1);
				strncpy(hostfile, optarg, strlen(optarg));
				h_flag = true;
				break;

			case 'c':
				if(optarg == 0) {
					cout << "Option -c requires an argument.\n";
					exit(1);
				}
				listen_port = atoi(optarg);
				if(listen_port < 1024) {
					cout << "Bad argument for -c. Must be greater than 1024\n";
					exit(1);
				}
				c_flag = true;
				break;

			case 's':
				if(optarg == 0) {
					cout << "Option -s requires an argument.\n";
					exit(1);
				}
				server_port = atoi(optarg);
				if(server_port < 1024) {
					cout << "Bad argument for -s. Must be greater than 1024\n";
					exit(1);
				}
				s_flag = true;
				break;

			case '?':
				exit(1);
		}
	}

	if(h_flag == false) {
		cout << "Required argument for -h\n";
		exit(1);
	}
	if(c_flag == false) {
		cout << "Required argument for -c\n";
		exit(1);
	}
	if(s_flag == false) {
		cout << "Required argument for -s\n";
		exit(1);
	}

	hf = fopen(hostfile, "r");
	if(hf == NULL) {
		cout << "Cannot open hostfile: " << hostfile << "\n";
		exit(1);
	}

	while( fscanf(hf, "%99s", temp_string) == 1) {
		pxy.svr_hostnames.push_back(string(temp_string));
	}

	cout << "Number of servers = " << pxy.svr_hostnames.size() << "\n";

	pxy.client_listen_port = listen_port;
	pxy.svr_listen_port = server_port;

	num_servers = pxy.svr_hostnames.size();
	pxy.svr_socket.resize(num_servers);
	pxy.svr_sockaddr.resize(num_servers);
	pxy.sessions.resize(num_servers);
	pxy.blacklist.resize(num_servers, false);

	sprintf(temp_string, "%d", server_port);
	for(i = 0; i < num_servers; i++) {

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;

		retval = getaddrinfo(pxy.svr_hostnames[i].c_str(), temp_string, &hints, &result);
		if(retval != 0) {
			cout << "getaddrinfo error " << errno << " for server " << pxy.svr_hostnames[i] << "\n";
			exit(1);
		}

		pxy.svr_sockaddr[i] = *result->ai_addr;

		pxy.svr_socket[i] = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if(pxy.svr_socket[i] == -1) {
			cout << "socket error " << errno << " for server " << pxy.svr_hostnames[i] << "\n";
			exit(1);
		}

		freeaddrinfo(result);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	retval = getaddrinfo(NULL, temp_string, &hints, &result);
	if(retval != 0) {
		cout << "getaddrinfo error " << errno << " for this host for udp port " << server_port << "\n";
		exit(1);
	}

	struct timeval timeout;
	memset(&timeout, 0, sizeof(timeout));
	timeout.tv_sec = 0;
	timeout.tv_usec = 100000;

	for(ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		pxy.svr_listen_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if(pxy.svr_listen_socket == -1) {
			continue;
		}

		retval = setsockopt(pxy.svr_listen_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));
		if(retval != 0) {
			continue;
		}

		retval = bind(pxy.svr_listen_socket, ptr->ai_addr, ptr->ai_addrlen);
		if(retval == 0) {
			break;
		}
	}

	if(ptr == NULL) {
		cout << "Cannot create and bind socket for this host for udp port " << server_port << "\n";
		exit(1);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	sprintf(temp_string, "%d", listen_port);
	retval = getaddrinfo(NULL, temp_string, &hints, &result);
	if(retval != 0) {
		cout << "getaddrinfo error " << errno << " while creating client listen socket\n";
		exit(1);
	}

	for(ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		int yes = 1;

		pxy.client_listen_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if(pxy.client_listen_socket == -1) {
			continue;
		}

		retval = bind(pxy.client_listen_socket, ptr->ai_addr, ptr->ai_addrlen);
		if(retval != 0) {
			continue;
		}

		retval = setsockopt(pxy.client_listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
		if(retval != 0) {
			continue;
		}

		retval = listen(pxy.client_listen_socket, 10);
		if(retval == 0) {
			break;
		}
	}

	if(ptr == NULL) {
		cout << "Cannot create and bind socket for this host for tcp port " << listen_port << "\n";
		exit(1);
	}

	pthread_t heart_beat_thr;
	pthread_attr_t attr;

	pthread_attr_init(&attr);

	pthread_create(&heart_beat_thr, &attr, heart_beat_fn, &pxy);

	int new_sock;
	struct sockaddr client_addr;
	socklen_t client_addr_len;

	int next_server = -1;
	while(1) {
		int random_server;

		client_addr_len = sizeof(struct sockaddr);
		new_sock = accept(pxy.client_listen_socket, &client_addr, &client_addr_len);
		if(new_sock == -1) {
			continue;
		}

		set<int>::iterator it;
		for(i = 0; i < num_servers; i++) {
			if( (it = pxy.sessions[i].find(new_sock)) != pxy.sessions[i].end() ) {
				pxy.sessions[i].erase(it);
				break;
			}
		}

		pxy.next_cid++;
		send(new_sock, &pxy.next_cid, sizeof(uint32_t), 0);

		int count;
		count = 0;
		next_server = (next_server + 1) % num_servers;
		while(pxy.blacklist[next_server] == true) {
			count++;
			next_server = (next_server + 1) % num_servers;
			if(count == num_servers) {
				cout << "Seems like all servers are down\n";
				exit(1);
			}
		}

		send(new_sock, pxy.svr_hostnames[next_server].c_str(), pxy.svr_hostnames[next_server].size(), 0);

		pxy.sessions[next_server].insert(new_sock);
	}

	return 0;
}

void *heart_beat_fn(void *arg) {

	ProxyServer *pxy = ((ProxyServer *)arg);
	HeartBeat hb_msg;
	int i, j, retval, num_servers;
	long time_diff;

	struct timeval t_start, t_end, timeout;

	num_servers = pxy->svr_hostnames.size();

	vector<uint32_t>miss_count(num_servers + 1, 0);
	vector<bool>got_hb(num_servers + 1, false);

	while(1) {

		struct sockaddr peer_addr;
		socklen_t peer_addr_len;

		hb_msg.type = htonl(TYPE_HB_CHECK);

		for(i = 0; i < num_servers; i++) {
			hb_msg.server_id = htonl(i);
			cout << "sending heartbeat to " << pxy->svr_hostnames[i] << "\n";
			sendto(pxy->svr_socket[i], &hb_msg, sizeof(hb_msg), 0, &pxy->svr_sockaddr[i], sizeof(struct sockaddr));
			got_hb[i] = false;
		}
		
		gettimeofday(&t_start, NULL);
		while(1) {
			peer_addr_len = sizeof(struct sockaddr);
			cout.flush();
			retval = recvfrom(pxy->svr_listen_socket, &hb_msg, sizeof(hb_msg), 0, &peer_addr, &peer_addr_len);
			if(retval > 0) {
				hb_msg.server_id = ntohl(hb_msg.server_id);
				hb_msg.server_id -= 1;
				cout << "Got a heart beat from: " << pxy->svr_hostnames[hb_msg.server_id] << "\n";
				miss_count[hb_msg.server_id] = 0;
				got_hb[hb_msg.server_id] = true;
			}				
			
			gettimeofday(&t_end, NULL);

			time_diff = (t_end.tv_sec - t_start.tv_sec)*1000000 + (t_end.tv_usec - t_start.tv_usec);
			if(time_diff > HB_INTERVAL) {
				break;
			}
		}
		
		for(i = 0; i < num_servers; i++) {
			if(got_hb[i] == true) {
				continue;
			}

			if(pxy->blacklist[i] == true) {
				continue;
			}

			miss_count[i]++;
			if(miss_count[i] >= MAX_MISSES) {
				pxy->blacklist[i] = true;

				hb_msg.type = htonl(TYPE_HB_DELETE);
				hb_msg.server_id = htonl(i + 1);
				for(j = 0; j < num_servers; j++) {
					if(j == i) {
						continue;
					}
					if(pxy->blacklist[j] == true) {
						continue;
					}

					cout << "Sending update msg to " << pxy->svr_hostnames[j] << "\n";
					retval = sendto(pxy->svr_socket[j], &hb_msg, sizeof(hb_msg), 0, &pxy->svr_sockaddr[j], sizeof(struct sockaddr));
					cout << "retval " << retval << "\n";
				}

				cout << "Server " << pxy->svr_hostnames[i] << " down!\n";

				set<int>::iterator it;
				
				j = (i + 1) % num_servers;
				while(pxy->blacklist[j] == true) {
					j = (j + 1) % num_servers;
					if(j == i) {
						cout << "Seems like all the servers are down!\n";
						return NULL;
					}
				}
				for(it = pxy->sessions[i].begin(); it != pxy->sessions[i].end(); it++) {
					send(*it, pxy->svr_hostnames[j].c_str(), pxy->svr_hostnames[j].size(), 0);
					pxy->sessions[j].insert(*it);
				}
				pxy->sessions[i].clear();

			}
		}
		
	}
	return NULL;
}
