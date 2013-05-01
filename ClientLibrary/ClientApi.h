#include <iostream>
#include <cstdlib>
#include <string>
#include <queue>
using namespace std;

#define SERVER_PORT	5001
#define PROXY_PORT	4000
#define OP_THREAD_PORT	6000

#define NODE_EPHEMERAL	0x00000001
#define NODE_SEQUENTIAL	0X00000002

#define TYPE_EXISTS		1
#define TYPE_GETDATA		2
#define TYPE_GETCHILDREN	3
#define TYPE_CREATE		4
#define TYPE_SETDATA		5
#define TYPE_DELETE		6
#define TYPE_SYNC		7


#pragma pack(1)
struct Request {
	uint32_t	async;
	uint32_t	client_id;
	uint32_t	timestamp;
	uint32_t	type;
	uint32_t	path_len;
	uint32_t	data_len;
	uint32_t	value;
	uint8_t		data[];
};
#pragma pack()

struct Session {

	string		svr_hostname;
	int		pxy_sockfd;
	int		svr_sockfd;
	int		op_sockfd;
	pthread_t	op_thread;
	uint32_t	client_id;
	uint32_t	timestamp;
	queue<Request>	update_queue;
	char		*last_request;
	int		last_req_len;

	Session();
	void		submitRequest(uint32_t, string, void *, uint32_t, uint32_t, bool);
	bool		createSession(string);
	uint32_t	createNode(string, void *, uint32_t, uint32_t, bool);
	bool		exists(string, bool, bool);
	bool		deleteNode(string, uint32_t, bool);
	uint32_t	getData(string, void *, bool, uint32_t *, bool);
	bool		setData(string, void *, uint32_t, uint32_t, bool);
	vector<string>	getChildren(string , bool, bool);
};

void *op_thread_fn(void *);
