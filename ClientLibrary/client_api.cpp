#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include "client_api.h"
using namespace std;

struct thread_arg {
	int	svr_sock;
	int	pxy_sock;
};

Session::Session() {

	op_sockfd = -1;
	svr_sockfd = -1;
	pxy_sockfd = -1;
	timestamp = 0;
	last_request = NULL;
	last_req_len = 0;
}

bool Session::createSession(string pxy_server) {

	struct 	addrinfo hints, *result, *ptr;
	char 	temp_string[1000];
	int	retval;
	string	server;

	pthread_attr_t attr;


	sprintf(temp_string, "%d", PROXY_PORT);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	retval = getaddrinfo(pxy_server.c_str(), temp_string, &hints, &result);
	if(retval != 0) {
		cout << "getaddrinfo error " << errno << " for proxy server " << pxy_server.c_str() << ":" << PROXY_PORT << "\n";
		return false;
	}

	pxy_sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if(pxy_sockfd == -1) {
		cout << "socket error " << errno << "  while creating for proxy server " << pxy_server.c_str() << ":" << PROXY_PORT << "\n";
		return false;
	}

	retval = connect(pxy_sockfd, result->ai_addr, result->ai_addrlen);
	if(retval != 0) {
		cout << "Cannot connect to proxy server at " <<pxy_server.c_str() << ":" << PROXY_PORT << "\n";
		return false;
	}

	retval = recv(pxy_sockfd, &client_id, sizeof(uint32_t), 0);
	if(retval <= 0) {
		cout << "recv error " << errno << " while receiving from proxy server " << pxy_server.c_str() << ":" << PROXY_PORT << "\n";
		return false;
	}

	cout << "Client Id = " << client_id << "\n";

	retval = recv(pxy_sockfd, temp_string, 1000, 0);
	if(retval <= 0) {
		cout << "recv error " << errno << " while receiving from proxy server " << pxy_server.c_str() << ":" << PROXY_PORT << "\n";
		return false;
	}

	cout << "Connecting to server " << temp_string << "\n";
	server = string(temp_string);

	svr_hostname = server;

	sprintf(temp_string, "%d", SERVER_PORT);
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	retval = getaddrinfo(server.c_str(), temp_string, &hints, &result);
	if(retval != 0) {
		return false;
	}

	svr_sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if(svr_sockfd == -1) {
		return false;
	}
	
	retval = connect(svr_sockfd, result->ai_addr, result->ai_addrlen);
	if(retval != 0) {
		svr_sockfd = -1;
		return false;
	}
	
	freeaddrinfo(result);

	struct thread_arg targ;
	targ.svr_sock = svr_sockfd;
	targ.pxy_sock = pxy_sockfd;

	pthread_attr_init(&attr);
	pthread_create(&op_thread, &attr, op_thread_fn, this);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	sprintf(temp_string, "%d", OP_THREAD_PORT);
	retval = getaddrinfo(NULL, temp_string, &hints, &result);
	if(retval != 0) {
		svr_sockfd = -1;
		op_sockfd = -1;
		return false;
	}

	op_sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if(op_sockfd == -1) {
		svr_sockfd = -1;
		op_sockfd = -1;
		return false;
	}

	retval = 1;
	while(retval != 0) {
		retval = connect(op_sockfd, result->ai_addr, result->ai_addrlen);
	}

	return true;
}

void *op_thread_fn(void *arg) {

	int	listen_sock, main_sock, svr_sock, pxy_sock;
	struct 	addrinfo hints, *result, *ptr;
	struct	sockaddr main_addr;
	int	main_addr_len, retval;
	char	temp_string[1000];
	uint8_t req_hdr[50];
	bool	reply_bool;

	Session *s = (Session *)arg;

	fd_set	master, read_fds;
	int	fd_max, i;

	Request	*req;

	cout << "Created session thread!\n";

	//struct thread_arg *targ = (thread_arg *)arg;
	//svr_sock = targ->svr_sock;
	//pxy_sock = targ->pxy_sock;
	svr_sock = s->svr_sockfd;
	pxy_sock = s->pxy_sockfd;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	sprintf(temp_string, "%d", OP_THREAD_PORT);
	retval = getaddrinfo(NULL, temp_string, &hints, &result);
	if(retval != 0) {
		return NULL;
	}

	for(ptr = result; ptr != NULL; ptr = ptr->ai_next) {
		int yes;

		listen_sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if(listen_sock == -1) {
			continue;
		}

		retval = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if(retval != 0) {
			continue;
		}

		retval = bind(listen_sock, ptr->ai_addr, ptr->ai_addrlen);
		if(retval != 0) {
			continue;
		}

		retval = listen(listen_sock, 10);
		if(retval == 0) {
			break;
		}
	}

	if(ptr == NULL) {
		return NULL;
	}

	char *req_data;
	main_addr_len = sizeof(struct sockaddr);
	main_sock = accept(listen_sock, &main_addr, (socklen_t *)&main_addr_len);
	if(main_sock == -1) {
		return NULL;
	}

	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	FD_SET(main_sock, &master);
	FD_SET(svr_sock, &master);
	FD_SET(pxy_sock, &master);
	fd_max = (main_sock > svr_sock) ? main_sock : svr_sock;

	while(1) {

		read_fds = master;
		select(fd_max + 1, &read_fds, NULL, NULL, NULL);

		for(i = 0; i <= fd_max; i++) {

			if(FD_ISSET(i, &read_fds)) { /* Handle client operation */

				string new_server;
				struct addrinfo hints, *result;

				if(i == pxy_sock) {
					retval = recv(pxy_sock, temp_string, 1000, 0);
					if(retval > 0) {
						cout << "Connecting to server: " << temp_string << "\n";
						new_server = string(temp_string);
					}
					else {
						cout << "Connection with the proxy server lost.\n";
						exit(1);
					}

					FD_CLR(svr_sock, &master);
					close(svr_sock);

					memset(&hints, 0, sizeof(hints));
					hints.ai_family = AF_INET;
					hints.ai_socktype = SOCK_STREAM;
					
					sprintf(temp_string, "%d", SERVER_PORT);
					retval = getaddrinfo(new_server.c_str(), temp_string, &hints, &result);
					if(retval != 0) {
						continue;
					}

					svr_sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
					if(svr_sock == -1) {
						continue;
					}

					retval = connect(svr_sock, result->ai_addr, result->ai_addrlen);
					if(retval != 0) {
						continue;
					}

					if(s->last_request != NULL) {
						send(svr_sock, s->last_request + 4, s->last_req_len - 4, 0);
					}

					FD_SET(svr_sock, &master);
					break;
				}
				else if(i == main_sock) { /* From main thread */

					retval = recv(main_sock, req_hdr, sizeof(Request), 0);
					if(retval > 0) {

						req = (Request *)req_hdr;
						cout << "Type: " << ntohl(req->type) << "\n";
						cout << "Pathlen: " << ntohl(req->path_len) << "\n";
						cout << "Datalen: " << ntohl(req->data_len) << "\n";

						req_data = NULL;
						while(req_data == NULL) {
							req_data = (char *)malloc(sizeof(Request) + ntohl(req->data_len));
						}
						memcpy(req_data, req_hdr, sizeof(Request));
						recv(main_sock, req_data + sizeof(Request), ntohl(req->data_len) , 0);

						req = (Request *)req_data;
						printf("%s\n", req->data);

						send(svr_sock, req_data + 4, sizeof(Request) - 4 + ntohl(req->data_len), 0);

						s->last_request = req_data;
						s->last_req_len = sizeof(Request) + ntohl(req->data_len);
						//free(req_data);

						continue;
					}
					else if(retval == 0) {
						cout << "Main thread closed connection.\n";
						return NULL;
					}
					else {
						continue;
					}
				}
				else if(i == svr_sock) {

					retval = recv(svr_sock, req_hdr + 4, sizeof(Request) - 4, 0);
					if(retval <= 0) {
						close(svr_sock);
					}

					req = (Request *)req_hdr;

					req->type = ntohl(req->type);
					req->path_len = ntohl(req->path_len);
					req->data_len = ntohl(req->data_len);
					req->value = ntohl(req->value);

					cout << "Received reply from server. Datalen: " << req->data_len << "\n";
					req_data = NULL;
					while(req_data == NULL) {
						req_data = (char *)malloc(req->data_len);
					}

					recv(svr_sock, req_data, req->data_len, 0);

					switch(req->type) {

						case TYPE_CREATE:
							retval = *((uint32_t *)(req_data));
							send(main_sock, &retval, sizeof(uint32_t), 0);
							break;


						case TYPE_GETDATA:
							send(main_sock, &req->data_len, sizeof(uint32_t), 0);
							send(main_sock, req_data, req->data_len, 0);
							break;

						case TYPE_EXISTS:
						case TYPE_DELETE:
						case TYPE_SETDATA:
							reply_bool = (bool)(*(uint32_t *)req_data);
							send(main_sock, &reply_bool, sizeof(bool), 0);
							break;

						case TYPE_GETCHILDREN:
							send(main_sock, &req->data_len, sizeof(uint32_t), 0);
							send(main_sock, req_data, req->data_len, 0);
							break;

						default:
							break;
					}

					free(req_data);
					free(s->last_request);
					s->last_req_len = 0;
					s->last_request = NULL;
				}
			}
		}
	}
}

uint32_t Session::createNode(string path, void *data, uint32_t data_size, uint32_t flags, bool async) {

	if( (svr_sockfd == -1) || (op_sockfd == -1) ) {
		return -1;
	}

	int	retval, reply;

	submitRequest(TYPE_CREATE, path, data, data_size, flags, async);

	if(async == false) {
		retval = recv(op_sockfd, &reply, sizeof(int), 0);
		if(retval > 0) {
			return reply;
		}
		else {
			return 0;
		}
	}
	else { //TODO async
	}
}

bool Session::exists(string path, bool watch, bool async) {

	if( (svr_sockfd == -1) || (op_sockfd == -1) ) {
		return false;
	}

	int	retval;
	bool	reply;
	uint32_t value;

	value = (watch == true) ? 1 : 0;

	submitRequest(TYPE_EXISTS, path, NULL, 0, value, async);

	if(async == false) {
		retval = recv(op_sockfd, &reply, 1, 0);
		if(retval > 0) {
			return reply;
		}
		else {
			return false;
		}
	}
	else { //TODO
	}
}

bool Session::deleteNode(string path, uint32_t version, bool async) {

	if( (op_sockfd == -1) || (svr_sockfd == -1) ) {
		return false;
	}

	int	retval;
	bool	reply;

	submitRequest(TYPE_DELETE, path, NULL, 0, version, async);

	if(async == false) {

		retval = recv(op_sockfd, &reply, 1, 0);
		if(retval > 0) {
			return reply;
		}
		else {
			return false;
		}
	}
	else { //TODO
	}
}

uint32_t Session::getData(string path, void *data, bool watch, uint32_t *version, bool async) {

	int		retval;
	uint32_t	value, recv_len, *iptr;
	uint8_t		*recv_buf;

	if( (op_sockfd == -1) || (svr_sockfd == -1) ) {
		return 0;
		*version = 0;
	}

	value = (watch == true) ? 1: 0;

	submitRequest(TYPE_GETDATA, path, NULL, 0, value, async);

	if(async == false) {
		retval = recv(op_sockfd, &recv_len, 4, 0);

		recv_buf = NULL;
		while(recv_buf == NULL) {
			recv_buf = (uint8_t *)malloc(recv_len);
		}

		retval = recv(op_sockfd, recv_buf, recv_len, 0);
		if(retval <= 0) {
			*version = 0;
			return 0;
		}

		iptr = (uint32_t *)recv_buf;
		*version = *iptr;

		recv_len -= 4;
		memcpy(data, ((char *)recv_buf) + 4, recv_len);

		return recv_len;
	}
	else { //TODO
	}
}

vector<string> Session::getChildren(string path, bool watch, bool async) {

	if( (op_sockfd == -1) || (svr_sockfd == -1) ) {
		return vector<string>();
	}

	uint32_t	value, num_children, data_len;
	int		retval, i;
	vector<string>  children;
	char		*data_buf, *bptr;

	value = (watch == true) ? 1 : 0;

	submitRequest(TYPE_GETCHILDREN, path, NULL, 0, value, async);

	if(async == false) {
		retval = recv(op_sockfd, &data_len, 4, 0);

		data_buf = NULL;
		while(data_buf == NULL) {
			data_buf = (char *)malloc(data_len);
		}

		retval = recv(op_sockfd, data_buf, data_len, 0);

		num_children = *((uint32_t *)data_buf);
		bptr = data_buf + 4;

		children.clear();
		for(i = 0; i < num_children; i++) {
			children.push_back(string(bptr));
			bptr += children[i].size() + 1;
		}

		free(data_buf);
		return children;
	}
	else { //TODO
	}
}
bool Session::setData(string path, void *data, uint32_t data_len, uint32_t version, bool async) {

	if( (op_sockfd == -1) || (svr_sockfd == -1) ) {
		return false;
	}

	int	retval;
	bool	reply;

	submitRequest(TYPE_SETDATA, path, data, data_len, version, async);

	if(async == false) {
		retval = recv(op_sockfd, &reply, 1, 0);

		if(retval > 0) {
			return reply;
		}
		else {
			return false;
		}
	}
	else { // TODO
	}
}

void Session::submitRequest(uint32_t type, string path, void *data, uint32_t data_len, uint32_t value, bool async) {

	int	req_len, retval;
	uint8_t	*req_buf;
	Request	*req;

	req_len = sizeof(struct Request) + path.size() + data_len;

	req_buf = NULL;
	while(req_buf == NULL) {
		req_buf = (uint8_t *)malloc(req_len);
	}

	timestamp++;
	req = (Request *)req_buf;
	req->async = async;
	req->client_id = htonl(client_id);
	req->timestamp = htonl(timestamp);
	req->type = htonl(type);
	req->path_len = htonl((uint32_t)path.size());
	req->data_len = htonl((uint32_t)path.size() + data_len);
	req->value = htonl(value);

	strncpy((char *)req->data, path.c_str(), path.size());
	if(data_len > 0) {
		memcpy(req->data + path.size(), data, data_len);
	}

	cout << "Request size: " << req_len << "\n";
	cout << "Data len: " << req->data_len << "\n";

	retval = send(op_sockfd, req_buf, req_len, 0);

	free(req_buf);
}
/*
int main() {

	Session s;
	string server, command, path, data;
	char recv_data[100];
	uint32_t version;
	int retval;

	cout << "size of request = " << sizeof(Request) << "\n";
	cout << "Enter server: ";
	getline(cin, server);

	retval = s.createSession(server);
	if(retval == false) {
		cout << "Cannot establish session with server " << server << "\n";
		exit(1);
	}

	while(1) {


		cout << "Enter command: ";
		getline(cin, command);
		cout << "Enter path: ";
		getline(cin, path);

		if(command == "create") {

			string eph, seq;
			uint32_t flags = 0;

			cout << "Enter data: ";
			getline(cin, data);
			cout << "Ephemeral? ";
			getline(cin, eph);
			cout << "Sequential? ";
			getline(cin, seq);

			if(eph == "yes") {
				flags |= NODE_EPHEMERAL;
			}
			if(seq == "yes") {
				flags |= NODE_SEQUENTIAL;
			}

			retval = s.createNode(path, (void *)data.c_str(), data.size(), flags, false);
			cout << "retval = " << retval << "\n";
			continue;
		}
		if(command == "exists") {
		
			retval = s.exists(path, false, false);
			cout << "retval = " << retval << "\n";
			continue;
		}
		if(command == "delete") {
		
			string version_str;
			cout << "Enter version: ";
			getline(cin, version_str);
			version = atoi(version_str.c_str());

			retval = s.deleteNode(path, version, false);
			cout << "retval = " << retval << "\n";
			continue;
		}
		if(command == "getchildren") {
			vector<string> children;
			int i;
			children = s.getChildren(path, false, false);
			cout << "Children: ";
			for(i = 0; i < children.size(); i++) {
				cout << children[i] << ", ";
			}
			cout << "\n";
			continue;
		}
		if(command == "getdata") {
		
			retval = s.getData(path, recv_data, false, &version, false);
			cout << "retval = " << retval << "\n";
			cout << "Data: " << recv_data << "\n";
			cout << "Version = " << version << "\n";
			continue;
		}
		if(command == "setdata") {
		
			string version_str;
			cout << "Enter data: ";
			getline(cin, data);
			cout << "Enter version: ";
			getline(cin, version_str);
			version = atoi(version_str.c_str());

			retval = s.setData(path, (void *)data.c_str(), data.size(), version, false);
			cout << "retval = " << retval << "\n";
			continue;
		}

	}
	
	return 0;
}
*/
