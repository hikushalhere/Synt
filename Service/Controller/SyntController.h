#ifndef SYNT_CONTROLLER_H
#define SYNT_CONTROLLER_H

#define DEBUG

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <queue>
#include <set>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <time.h>
#include "DataTree.h"
#include "message_format.h"

#define BACKLOG 10
#define DUMMY_UPDATE_VALUE 1
#define PAXOS_HOST "localhost"
#define MAX_SELECT_TRIES 10

#define MAX_PATH_SIZE 1024 // in Bytes
#define MAX_DATA_SIZE 1    // in MB
#define BYTE_TO_MB 1048576

#define SENT true
#define NOT_SENT false

#define EXISTS 1
#define GET_DATA 2
#define GET_CHILDREN 3
#define CREATE 4
#define SET_DATA 5
#define DELETE 6
#define SYNC 7

#define ORDERED_UPDATE 1
#define UNORDERED_UPDATE 2
#define NEW_UPDATE 3
#define OLD_UPDATE 4

#define SEC_TO_MICROSEC 1000000
#define SEND_TIMEOUT 2 // In seconds.
#define ACK_TIMEOUT 5  // In seconds.

typedef std::pair<uint32_t, uint32_t> UpdatePair;  // (id, timestamp)
typedef std::pair<int, SyntMessage *> RequestPair; // (socket, request)

class SyntController {
    private:
        /**********************/
        /* Private Variables. */
        /**********************/

        uint32_t myId;                               // My unique id.
        uint32_t numControllers;                     // Number of controllers in the system.
        pthread_mutex_t numControllersLock;          // Mutex variable for numControllers.
        std::map<uint32_t, std::string> hostNameMap; // Map of (Id : Hostname of controller).
        pthread_mutex_t hostNameMapLock;             // Mutex variable for hostNameMap.
        fd_set masterReadList;                       // Master list of client session sockets.
        pthread_mutex_t masterReadListLock;          // Mutex variable for masterReadList.
        DataTree dataTree;                           // In-memory data store object.

        std::string paxosPort;        // Paxos's port to use for making a TCP connection.
        std::string syntListenPort;   // Port to listen for peer Synt Controller UDP connections.
        std::string clientListenPort; // Port to listen for Clients' TCP connections.
        std::string heartbeatPort;    // Port to listen for Heartbeat messages.
        int paxosSocket;              // TCP socket for communicating with Paxos.
        int syntListenSocket;         // UDP socket for receiving connections from peers.
        int clientListenSocket;       // TCP socket for accepting connections from Client.
        int heartbeatSocket;          // UDP socket for receving heartbeat messages.

        // Set of client sockets to maintain all the client connections.
        std::set<int> clientSocketSet;
        pthread_mutex_t clientSocketSetLock;

        // Map of (Client Update : Client Session Socket) and its mutex. 
        std::map<UpdatePair, int> clientUpdateSocketMap;
        pthread_mutex_t clientUpdateSocketMapLock;
        
        // Map of (Client Update : Result of client request ordered by Paxos) and its mutex.
        std::map<UpdatePair, uint32_t> orderedRequestMap;
        pthread_mutex_t orderedRequestMapLock;
        
        // Map of (Client Update : Client Request sent to Paxos) and its mutex.
        std::map<UpdatePair, SyntMessage *> unorderedRequestMap;
        pthread_mutex_t unorderedRequestMapLock;
        
        // Queue of pending client requests, the mutex and condition variable for it.
        std::queue<RequestPair> pendingClientRequestsQueue;
        pthread_mutex_t pendingClientRequestsQueueLock;
        pthread_cond_t queueNotEmptyCondition;

        // Set of unapplied ordered Paxos updates and its mutex.
        std::set<UpdatePair> paxosUpdateSet;
        pthread_mutex_t paxosUpdateSetLock;
        
        /********************/
        /* Private Methods. */
        /********************/

        // Establishes a TCP connection with a host.
        int establishConnection(std::string, std::string);
        
        // Opens a socket and starts listening on it.
        int startListening(std::string, int);
        
        // Handles all client connections.
        void *handleClients(void);
        
        // Deletes the state about the client - session socket and all pending requests.
        void deleteClientState(int);

        // Handles a read or write request by sending it to a handler in a new thread.
        void handleClientRequest(SyntMessage *, int);
        
        // Handles clients' read requests.
        void *handleReadRequests(SyntMessage *, int);

        // Gets the path out from a SyntMessage.
        std::string getPath(SyntMessage *);
        
        // Computes and returns the total length of all the path strings in the vector.
        uint32_t getPathLengthSum(std::vector<std::string>);
        
        // Extracts the paths from a vector of paths and serializes them into a byte stream.
        char *serializePathVector(std::vector<std::string>, uint32_t);
        
        // Send the response to client.
        void respondToClient(SyntMessage *, char *, uint32_t, int);
        
        // Constructs the response to be sent to the client.
        void constructClientResponse(SyntMessage *, SyntMessage *, char *, uint32_t);
        
        // Sends the response to client.
        void sendToClient(SyntMessage *, uint32_t, int);

        // Handles clients' write requests.
        void *handleWriteRequests(SyntMessage *, int);
        
        // Decides on the state of the client request.
        uint16_t checkWriteRequest(UpdatePair); 
        
        // Check if an update's timestamp is old and rejects it.
        bool isUpdateOld(UpdatePair);
   
        // Handles messages from Paxos.
        void *handlePaxos(void);
    
        // Updates the data structures before a request is sent to Paxos to be ordered.
        void orderRequest();

        // Buffers the unordered request and removes it from the queue of pending updates.
        bool updateDataStructures(UpdatePair &);

        // Sends SyntUpdate to all controllers notifying them of a client request received.
        void sendSyntMessageToControllers(void *, uint32_t);

        // Sends a message to a host via UDP.
        bool sendMessage(void *, uint32_t, std::string);   
    
        // Wait for Acks from all controller for the sent update.
        void waitForAcks(SyntMessage *);
        
        // Constructs and returns and PaxosUpdate.
        PaxosUpdate *constructPaxosUpdate(UpdatePair);

        // Sends an update to Paxos.
        void sendUpdateToPaxos(PaxosUpdate *);

        // Serves the write request corresponding to the Paxos update that has been ordered.
        void applyWriteUpdate(UpdatePair);
        
        // Removes all old buffered requests from the same client.
        void cleanUpOldOrderedRequests(UpdatePair);

        // Performs write on the in-memory data store depending on the type of request.
        uint32_t writeToDataTree(SyntMessage *);

        // Handles messages from peer Synt controllers.
        void *handleSynt(void);
        
        // Sends an Ack in response to a SyntMessage received.
        void sendAckToController(struct sockaddr_in *, SyntMessage *);

        // Handles heartbeat messages and replies with "I am alive".
        void *handleHeartbeat(void);

        // Utility function to convert messages from host to network byte order.
        void hton(void *, uint32_t);

        // Utility function to convert messages from network to host byte order.
        void ntoh(void *, uint32_t);



    public:
        /*******************/
        /* Public Methods. */
        /*******************/

        // Parameterized constructor.
        SyntController(SyntInfo *);
        
        // Starts the controller.
        void start();
        
        // Static method to start the thread for handling client connections.
        static void *startClientHandlerThread(void *);
        
        // Static method to start the thread for handling peer controller connections.
        static void *startSyntHandlerThread(void *);
        
        // Static method to start the thread for handling Paxos messages.
        static void *startPaxosHandlerThread(void *);
        
	// Static method to start the thread for responding to heartbeats.
        static void *startHeartbeatHandlerThread(void *);
};

#endif
