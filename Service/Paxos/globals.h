#include <stdint.h>
#include <pthread.h>

#define STATE_LEADER_ELECTION 	1
#define STATE_REG_LEADER	2
#define STATE_REG_NONLEADER	3

#define UINT32_INV 0xffffffff

#define CLIENT_MAX 100

//#define DEBUG

extern pthread_mutex_t global_lock, synch_lock;
extern int synch;
extern int	client_open_sockets[];
extern int zookeeper_socket;

extern int my_server_id;
extern int num_servers_g;
extern int state;

extern uint32_t Last_Attempted;
extern uint32_t Last_Installed;

extern View_Change **VC_g;

extern Prepare 		*Prepare_g;
extern Prepare_OK	**Prepare_OK_g;

extern uint32_t Local_Aru;
extern uint32_t Last_Proposed;

struct Global_History_entry_t {
	uint32_t	seq;
	Proposal	*proposal;
	Accept		**accepts;

	Globally_Ordered_Update *ordered;
	struct Global_History_entry_t 	*next;
};
typedef struct Global_History_entry_t Global_History_entry;

#define GH_HEAD_SEQ 0xffffffff
extern Global_History_entry Global_History;

struct Update_Queue_entry_t {
	Client_Update			*client_update;
	struct Update_Queue_entry_t 	*next;
};
typedef struct Update_Queue_entry_t Update_Queue_entry;

extern Update_Queue_entry Update_Queue;

struct Pending_Update_entry_t {
	Client_Update			*client_update;
	int				client_socket;
	int				update_timer;
	struct Pending_Update_entry_t	*next;
};
typedef struct Pending_Update_entry_t Pending_Update_entry;

extern Pending_Update_entry Pending_Updates;

struct Last_Exe_Enq_entry_t {
	uint32_t		client_id;
	uint32_t		last_executed;
	uint32_t		last_enqueued;
	struct Last_Exe_Enq_entry_t *next;
};

typedef struct Last_Exe_Enq_entry_t Last_Exe_Enq_entry;

extern Last_Exe_Enq_entry Last_Exe_Enq;

extern uint32_t Last_Executed[];
extern uint32_t Last_Enqueued[];
//extern Client_Update *Pending_Updates[];

extern Global_History_entry *global_history_exists(Global_History_entry *, uint32_t);

extern struct 	sockaddr *peers_sockaddr;
extern struct 	sockaddr_in peer;
extern int	*peers_sockets;

extern	int	progress_timer;
extern 	int	progress_timer_ctr;
extern	int	update_timer[];
extern	int	update_timer_ctr[];
