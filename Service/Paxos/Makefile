all: clean server client

server:
	gcc 	client_handler.c 	client_updates.c 	conflicts.c 	global_history.c 	\
		global_ordering.c 	leader_election.c	paxos.c 	paxos_handler.c 	\
		prepare_phase.c 	server.c 		timer.c 				\
		-o server -lpthread -ggdb

client:
	gcc	client.c -o client -ggdb

clean:
	rm -f server client
