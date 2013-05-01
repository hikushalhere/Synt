#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <vector>
#include <set>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
using namespace std;

#define HB_INTERVAL 	2000000
#define MAX_MISSES	2

struct ProxyServer {

	vector<string>  svr_hostnames;
	vector<bool>	blacklist;
	uint32_t	next_cid;
	uint16_t	client_listen_port;
	uint16_t	svr_listen_port;

	vector<struct sockaddr> svr_sockaddr;
	vector<int>		svr_socket;
	int			svr_listen_socket;
	int			client_listen_socket;

	vector<set<int> >	sessions;

	ProxyServer();
};

#define TYPE_HB_CHECK 	4
#define TYPE_HB_DELETE	5

struct HeartBeat {
	uint32_t type;
	uint32_t server_id;
};
