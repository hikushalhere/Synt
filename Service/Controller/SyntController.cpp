#include "SyntController.h"

using namespace std;

// Parameterized constructor.
SyntController::SyntController(SyntInfo *info) {
    // Fetch all the required information from info.
    this->myId = info->myId;
    this->numControllers = info->numControllers;
    this->hostNames = info->hostNames;
    this->paxosPort = info->paxosPort;
    this->syntListenPort = info->syntListenPort;
    this->clientListenPort = info->clientListenPort;

    // Set up the  network sockets.
    this->paxosSocket = establishConnection(PAXOS_HOST, this->paxosPort);
    this->syntListenSocket = startListening(this->syntListenPort, SOCK_DGRAM);
    this->clientListenSocket = startListening(this->clientListenPort, SOCK_STREAM);
    FD_ZERO(&this->masterReadList);
    FD_SET(this->clientListenSocket, &this->masterReadList);

    // Initialize the timestamp.
    this->updateTimestamp = 1;
}

// Establishes a TCP connection with a host.
int SyntController::establishConnection(string hostname, string port) {
    int socketFD, status;
    struct addrinfo hint, *hostInfo, *curr;

    // Set the appropriate flags.
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;

    // Get the address info of the host to send to.
    if((status = getaddrinfo(hostname.c_str(), port.c_str(), &hint, &hostInfo)) != 0) {
        cerr<<"\ngetaddrinfo: "<<gai_strerror(status);
        throw string("\nCould not retrieve my address info.");
    }

    // Get the socket descriptor to use for sending the message.
    for(curr = hostInfo; curr != NULL; curr = curr->ai_next) {
        if((socketFD = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol)) == -1) {
            perror("\nFailed to create a socket: socket() failed.");
            continue;
        }
        
        int yes = 1;
        setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        
        if(connect(socketFD, curr->ai_addr, curr->ai_addrlen) == -1) {
            close(socketFD);
            perror("\nCan not connect to remote host: connect() failed.");
            continue;
        }
        break;
    }

    freeaddrinfo(hostInfo);

    // Socket creation failed for all options.
    if(curr == NULL) {
        throw string("\nFailed to create or bind any socket to listen on.");
    }
    
    return socketFD;
}

// Opens a socket and starts listening on it.
int SyntController::startListening(string port, int socketType) {
    int socketFD, status;
    struct addrinfo hints, *hostInfo, *curr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = socketType;
    hints.ai_flags = AI_PASSIVE;

    if((status = getaddrinfo(NULL, port.c_str(), &hints, &hostInfo)) != 0) {
        cerr<<"\ngetaddrinfo: "<<gai_strerror(status);
        throw string("\nCould not retrieve my address info.");
    }

    // Get the socket descriptor to use for listening.
    for(curr = hostInfo; curr != NULL; curr = curr->ai_next) {
        if((socketFD = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol)) == -1) {
            perror("\nFailed to create a socket for myself to listen on: socket() failed.");
            continue;
        }
       
        int yes = 1;
        if(setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            close(socketFD);
            perror("\nFailed to set set socket options: setsockopt() failed.");
            continue;
        }
        
        if(bind(socketFD, curr->ai_addr, curr->ai_addrlen) == -1) {
            close(socketFD);
            perror("\nFailed to bind the socket for myself to listen on: bind() failed.");
            continue;
        }

        break;
    }

    freeaddrinfo(hostInfo);

    // Socket creation failed for all options.
    if(curr == NULL) {
        throw string("\nFailed to create or bind any socket to listen on.");
    }
    
    // Call listen() if it is supposed to be a TCP socket.
    if(socketType == SOCK_STREAM) {
        if(listen(socketFD, BACKLOG) == -1) {
            close(socketFD);
            throw string("\nFailed while trying to start listening: listen() failed.");
        }
    }
    
    return socketFD;
}

// Starts the controller.
void SyntController::start() {
    pthread_t clientThread;
    pthread_t syntThread;
    pthread_t paxosThread;
    int status;

    status = pthread_create(&clientThread, NULL, SyntController::startClientHandlerThread, this);
    if(status == 0) {
        pthread_detach(clientThread);
    }
    
    status = pthread_create(&syntThread, NULL, SyntController::startSyntHandlerThread, this);
    if(status == 0) {
        pthread_detach(syntThread);
    }
    
    status = pthread_create(&paxosThread, NULL, SyntController::startPaxosHandlerThread, this);
    if(status == 0) {
        pthread_detach(paxosThread);
    }
}

// Static method to start the thread for handling client connections.
void* SyntController::startClientHandlerThread(void *arg) {
    return ((SyntController *) arg)->handleClients();
}

// Static method to start the thread for handling peer controller connections.
void* SyntController::startSyntHandlerThread(void *arg) {
    return ((SyntController *) arg)->handleSynt();
}

// Static method to start the thread for handling Paxos messages.
void* SyntController::startPaxosHandlerThread(void *arg) {
    return ((SyntController *) arg)->handlePaxos();
}

// Handles all client connections.
void* SyntController::handleClients(void) {
    fd_set readList;
    int selectTries = 0, maxFD = this->clientListenSocket;
    FD_ZERO(&readList);

    while(1) {
        readList = this->masterReadList;
        int selectStatus = select(maxFD + 1, &readList, NULL, NULL, NULL);
        
        if(selectStatus == -1) {
            cerr<<"\nThe highest file descriptor was = "<<maxFD;
            perror(": select() failed.");
            ++selectTries;
            if(selectTries >= MAX_SELECT_TRIES) {
                cerr<<"\nTried select() "<<selectTries<<" number of times but could not succeed.";
                break;
            }
        } else {
            for(int fd = 0; fd <= maxFD; ++fd) { // Run through the existing connections for the data to be read
                if(FD_ISSET(fd, &readList)) { // Got something to read from a file descriptor we are monitoring.
                    if(fd == 0 || fd == this->syntListenSocket || fd == this->paxosSocket) { // Ignore these file descriptors.
                        continue;
                    } else if(fd == this->clientListenSocket) { // Got a connection request from a client.
                        struct sockaddr_in clientAddress;
                        socklen_t addrLen = sizeof(clientAddress);
                        int newFD = accept(fd, (struct sockaddr *) &clientAddress, &addrLen); // Accept the connection.

                        if(newFD == -1) {
                            perror("\nError while accepting a connection from the client: accept() failed.");
                        } else {
                            // Add the new client socket into the masterReadList.
                            pthread_mutex_lock(&(this->masterReadListLock));
                            FD_SET(newFD, &this->masterReadList);
                            pthread_mutex_unlock(&(this->masterReadListLock));

                            // Update the highest file descriptor.
                            if(newFD > maxFD) {
                                maxFD = newFD;
                            }
                        }
                    } else { // Got a message from a client.
                        uint32_t syntRequestSize = sizeof(SyntMessage) + MAX_PATH_SIZE + MAX_DATA_SIZE * BYTE_TO_MB;
                        SyntMessage *syntRequest = (SyntMessage *) malloc(syntRequestSize);
                        int numBytesRecvd = recv(fd, (void *) syntRequest, syntRequestSize, 0); // TODO peek

                        if(numBytesRecvd > 0) {
                            ntoh(syntRequest, TYPE_SYNT_MESSAGE);  // Convert the client's request from network to host byte order.
                            handleClientRequest(syntRequest, fd);  // Handle the client's request.
                        } else if(numBytesRecvd == 0) { // Client closed the connection.
                            deleteClientState(fd);
                        } else {
                            perror("\nError while receving a message from the client: recv() failed.");
                        }
                     }
                } // if(FD_ISSET(i, &readList))
           } // for loop for going over all the FDs.
        }  // if(selectStatus == -1) {...} else {...}
    } // while(1)

    pthread_exit(0);
}

// Deletes the state about the client - session socket and all pending requests.
void SyntController::deleteClientState(int clientSocket) {
    // Clear all pending requests from the client.
    pthread_mutex_lock(&(this->myClientRequestsMapLock));
    while(!(this->myClientRequestsMap[clientSocket].empty())) {
        SyntMessage *syntRequest = this->myClientRequestsMap[clientSocket].front();
        if(syntRequest) {
            free(syntRequest); // Free up memory held by the pending request.
        }
        this->myClientRequestsMap[clientSocket].pop();
    }
    this->myClientRequestsMap.erase(clientSocket);
    pthread_mutex_unlock(&(this->myClientRequestsMapLock));

    UpdatePair paxosUpdate; // The unique key of the update sent to Paxos.

    // Remove the socket from clientSocketMap.
    pthread_mutex_lock(&(this->clientSocketMapLock));
    for(map<UpdatePair, int>::iterator iter = this->clientSocketMap.begin(); iter != this->clientSocketMap.end(); ++iter) {
        if(iter->second == clientSocket) {
            paxosUpdate = iter->first;
            this->clientSocketMap.erase(iter);
            break;
        }
    }
    pthread_mutex_unlock(&(this->clientSocketMapLock));

    // Remove the unordered request from unorderedRequestMap.
    pthread_mutex_lock(&(this->unorderedRequestMapLock));
    SyntMessage *syntRequest = this->unorderedRequestMap[paxosUpdate];
    this->unorderedRequestMap.erase(paxosUpdate);
    pthread_mutex_unlock(&(this->unorderedRequestMapLock));
    if(syntRequest) {
        free(syntRequest);
    }
}

// Handles a read or write request by sending it to a handler in a new thread.
void SyntController::handleClientRequest(SyntMessage *syntRequest, int clientSocket) {
    switch(syntRequest->opType) {
        case EXISTS:
        case GET_DATA:
        case GET_CHILDREN:
            handleReadRequests(syntRequest, clientSocket);
            break;

        case CREATE:
        case SET_DATA:
        case DELETE:
        case SYNC:
            handleWriteRequests(syntRequest, clientSocket);
            break;
    }
}

// Handles clients' read requests.
void* SyntController::handleReadRequests(SyntMessage *syntRequest, int clientSocket) {
    char *data;
    uint32_t dataLength;
    bool respond = true;
    string path = getPath(syntRequest);

    switch(syntRequest->opType) {
        case EXISTS:
            {
                bool watch = false;
                uint32_t nodeExists = (uint32_t) this->dataTree.exists(path, watch);
                dataLength = sizeof(nodeExists) / sizeof(char);
                data = new char[dataLength];
                memcpy(data, &nodeExists, dataLength);
            }
            break;

        case GET_DATA:
            {
                bool watch = false;
                uint32_t version;
                data = new char[MAX_DATA_SIZE * BYTE_TO_MB];
                uint32_t dataLength = this->dataTree.getData(path, data, watch, &version);
                
                // Create a new data buffer and packe the data preceded by the version number.
                char *newData = new char[sizeof(version) + dataLength];
                memcpy(newData, &version, sizeof(version));
                memcpy(newData + sizeof(version), data, dataLength);

                delete[] data;
                data = newData;
            }
            break;

        case GET_CHILDREN:
            {
                bool watch = false;
                vector<string> pathVector = this->dataTree.getChildren(path, watch);
                uint32_t numPaths = pathVector.size();
                dataLength = sizeof(uint32_t) + getPathLengthSum(pathVector) + numPaths;
                data = serializePathVector(pathVector, dataLength);
            }
            break;

        default:
            respond = false;
    }

    if(respond) {
        respondToClient(syntRequest, data, dataLength, clientSocket);
    } else {
        cerr<<"\nThe opType of operation is unknown. Operation type's code = "<<syntRequest->opType;
    }
    
    delete[] data;
    free(syntRequest);
}

// Gets the path out from a SyntMessage.
string SyntController::getPath(SyntMessage *syntRequest) {
    int pathLength = syntRequest->pathLength;
    char *path = new char[pathLength];
    memcpy(path, syntRequest + sizeof(SyntMessage), pathLength);
    string pathString(path);
    delete[] path;
    return pathString;
}

// Computes and returns the total length of all the path strings in the vector.
uint32_t SyntController::getPathLengthSum(vector<string> pathVector) {
    uint32_t pathLengthSum = 0;
    for(vector<string>::iterator iter = pathVector.begin(); iter != pathVector.end(); ++iter) {
        pathLengthSum += (*iter).length();    
    }
    return pathLengthSum;
}

// Extracts the paths from a vector of paths and serializes them into a byte stream.
char* SyntController::serializePathVector(vector<string> pathVector, uint32_t dataLength) {
    uint32_t numPaths = pathVector.size();
    char *data = new char[dataLength];
    char *destination = data;
   
    // Copy the number of paths at the beginning of the byte stream.
    memcpy(destination, &numPaths, sizeof(uint32_t));
    destination += sizeof(uint32_t);

    // Stitch together the paths one by one delimited by '\0'.
    for(vector<string>::iterator iter = pathVector.begin(); iter != pathVector.end(); ++iter) {
        string path = *iter;
        memcpy(destination, path.c_str(), path.length());
        destination += path.length();
        *destination = '\0';
        ++destination;
    }

    return data;
}

// Send the response to client.
void SyntController::respondToClient(SyntMessage *syntRequest, char *data, uint32_t dataLength, int clientSocket) {
    uint32_t responseLength = sizeof(SyntMessage) + dataLength;
    SyntMessage *syntResponse = (SyntMessage *) malloc(responseLength);
    constructClientResponse(syntRequest, syntResponse, data, dataLength); // Construct the response.
    sendToClient(syntResponse, responseLength, clientSocket); // Send the response to client.
    free(syntResponse); // Free up memory for storing the response.
}

// Constructs the response to be sent to the client.
void SyntController::constructClientResponse(SyntMessage *syntRequest, SyntMessage *syntResponse, char *data, uint32_t dataLength) {
    // Copy the components of the repsonse one by one.
    syntResponse->opType = syntRequest->opType;
    syntResponse->pathLength = syntRequest->pathLength;
    syntResponse->dataLength = dataLength;
    syntResponse->uint32Value = syntRequest->uint32Value;
    
    // Copy the data.
    char *destination = (char *) syntResponse;
    destination += sizeof(SyntMessage);
    memcpy(destination, data, dataLength);
}

// Sends the response to client.
void SyntController::sendToClient(SyntMessage *syntResponse, uint32_t responseSize, int clientSocket) {
    hton(syntResponse, TYPE_SYNT_MESSAGE); // Convert the response from host to network byte order.
    int numBytes = send(clientSocket, (void *) syntResponse, responseSize, 0); // Send the response to client.
    
    if(numBytes == -1) {
        cerr<<"\nFailed to send message to client.";
        perror("\nFailed to send message: send() failed.");
    }
}

// Handles clients' write requests.
void* SyntController::handleWriteRequests(SyntMessage *syntRequest, int clientSocket) {
    UpdatePair paxosUpdate(this->myId, this->updateTimestamp);
    bool firstTime = false;

    // Store the client socket in map indexed by the update key.
    pthread_mutex_lock(&(this->clientSocketMapLock));
    this->clientSocketMap[paxosUpdate] = clientSocket;
    pthread_mutex_unlock(&(this->clientSocketMapLock));
    
    // Check if this is the client's first request.
    if(this->myClientRequestsMap.count(clientSocket) == 0) {
        firstTime = true;
    }

    // Enqueue the client request in my queue of requests from this client.
    pthread_mutex_lock(&(this->myClientRequestsMapLock));
    this->myClientRequestsMap[clientSocket].push(syntRequest);
    pthread_mutex_unlock(&(this->myClientRequestsMapLock));

    if(firstTime) { // First update in this client session.
        orderRequest(paxosUpdate); // Send the update to be ordered via Paxos.
    }
}

// Updates the data structures before a request is sent to Paxos to be ordered.
void SyntController::orderRequest(UpdatePair paxosUpdate) {
    // Buffer the client request to be sent.
    bufferUnorderedRequest(paxosUpdate);
    
    // Send the client request to peer Synt controllers.
    SyntMessage *syntRequest = this->unorderedRequestMap[paxosUpdate];
    SyntUpdate *syntUpdate = constructSyntUpdate(syntRequest, paxosUpdate);
    hton(syntUpdate, TYPE_SYNT_UPDATE);
    sendSyntUpdateToControllers(syntUpdate, sizeof(SyntUpdate) + syntRequest->dataLength);
    free(syntUpdate);
    
    // Send the update to Paxos to be ordered.
    PaxosUpdate *paxosUpdateMessage = constructPaxosUpdate();
    sendUpdateToPaxos(paxosUpdateMessage);
    delete paxosUpdateMessage;

    // Increment the update timestamp.
    this->updateTimestamp++;
}

// Constructs a SyntUpdate message from a SyntMessage request.
SyntUpdate* SyntController::constructSyntUpdate(SyntMessage *syntRequest, UpdatePair paxosUpdate) {
    uint32_t updateSize = sizeof(SyntUpdate) + syntRequest->dataLength;
    SyntUpdate *syntUpdate = (SyntUpdate *) malloc(updateSize);
    
    syntUpdate->clientId = paxosUpdate.first;
    syntUpdate->timestamp = paxosUpdate.second;
    syntUpdate->opType = syntRequest->opType;
    syntUpdate->pathLength = syntRequest->pathLength;
    syntUpdate->dataLength = syntRequest->dataLength;
    syntUpdate->uint32Value = syntRequest->uint32Value;
    memcpy(syntUpdate + sizeof(SyntUpdate), syntRequest + sizeof(SyntMessage), syntUpdate->dataLength);
    
    return syntUpdate;
}

// Buffers the unordered request and removes it from the queue of pending updates.
void SyntController::bufferUnorderedRequest(UpdatePair paxosUpdate) {
    int clientSocket = this->clientSocketMap[paxosUpdate];
    SyntMessage *syntRequest = this->myClientRequestsMap[clientSocket].front();
    
    // Remove the client request from the queue of pending updates.
    pthread_mutex_lock(&(this->myClientRequestsMapLock));
    this->myClientRequestsMap[clientSocket].pop();
    pthread_mutex_unlock(&(this->myClientRequestsMapLock));
    
    // Store in client request in the request queue in the map.
    pthread_mutex_lock(&(this->unorderedRequestMapLock));
    this->unorderedRequestMap[paxosUpdate] = syntRequest;
    pthread_mutex_unlock(&(this->unorderedRequestMapLock));
}

// Sends SyntUpdate to all controllers notifying them of a client request received.
void SyntController::sendSyntUpdateToControllers(void *message, uint32_t messageSize) {
    if(messageSize > 0) {
        bool *sendQueue = new bool[this->numControllers];
        uint32_t sendCount = 0;
        long diff;
        struct timeval start; 
        
        gettimeofday(&start, NULL);                 // Record the start time before sending.
        memset(sendQueue, 0, this->numControllers); // Initialize the send status array.

        do {
            for(uint32_t serverId = 0; serverId < this->numControllers; ++serverId) {
                // I don't want to send the message to myself. :)
                if(serverId != this->myId && !sendQueue[serverId]) {
                    if((sendQueue[serverId] = sendMessage(message, messageSize, hostNames[serverId]) == true)) {
                        ++sendCount;
                    }
                }
            }

            // Record the current time and calculate the difference from the time message was sent.
            struct timeval end;
            gettimeofday(&end, NULL);
            diff = ((end.tv_sec * SEC_TO_MICROSEC + end.tv_usec) - ((start).tv_sec * SEC_TO_MICROSEC + (start).tv_usec));

        } while(sendCount < this->numControllers-1 && diff < SEND_TIMEOUT * SEC_TO_MICROSEC);

        delete[] sendQueue;
    }
}

// Sends a message to a host via UDP.
bool SyntController::sendMessage(void *msg, uint32_t msgSize, string hostname) {
    bool sendStatus;
    int socketFD, status, numbytes;
    struct addrinfo hint, *hostInfo, *curr;
    
    // Set the appropriate flags.
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_DGRAM;
    hint.ai_flags = AI_PASSIVE;

    // Get the address info of the host to send to.
    if((status = getaddrinfo(hostname.c_str(), this->paxosPort.c_str(), &hint, &hostInfo)) != 0) {
        cerr<<"\ngetaddrinfo: "<<gai_strerror(status);
        sendStatus = NOT_SENT;
    } else {
        // Get the socket descriptor to use for sending the message.
        for(curr = hostInfo; curr != NULL; curr = curr->ai_next) {
            if((socketFD = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol)) == -1) {
                perror("\nFailed to create a socket: socket() failed.");
                continue;
            }
            break;
        }

        if(curr == NULL) { // Socket creation failed for all options.
            cerr<<"\nFailed to create any socket for "<<hostname;
            sendStatus = NOT_SENT;
        } else {
            // Try sending the message to host.
            if((numbytes = sendto(socketFD, msg, msgSize, 0, curr->ai_addr, curr->ai_addrlen)) == -1) {
                cerr<<"\nFailed to send message to "<<hostname;
                perror("\nFailed to send message: sendto() failed");
                sendStatus = NOT_SENT;
            } else {
                sendStatus = SENT; // Update the status of the sending.
            }
            close(socketFD);
        }
        freeaddrinfo(hostInfo);
    }

    return sendStatus;
}

// Constructs and returns and PaxosUpdate.
PaxosUpdate* SyntController::constructPaxosUpdate() {
    PaxosUpdate *paxosUpdate = new PaxosUpdate;
    paxosUpdate->type = TYPE_PAXOS_UPDATE;
    paxosUpdate->clientId = this->myId;
    paxosUpdate->serverId = this->myId;
    paxosUpdate->timestamp = this->updateTimestamp;
    paxosUpdate->update = DUMMY_UPDATE_VALUE;
    return paxosUpdate;
}

// Sends an update to Paxos.
void SyntController::sendUpdateToPaxos(PaxosUpdate *paxosUpdate) {
    hton(paxosUpdate, TYPE_PAXOS_UPDATE);
    int numBytes = send(paxosSocket, (void *) paxosUpdate, sizeof(PaxosUpdate), 0);
    
    if(numBytes == -1) {
        cerr<<"\nFailed to send message to Paxos";
        perror("\nFailed to send message: send() failed.");
    }
}

// Handles messages from Paxos.
void* SyntController::handlePaxos(void) {
    while(1) {
        PaxosUpdate paxosUpdateMessage;
        int numBytesRecvd = recv(this->paxosSocket, (void *) &paxosUpdateMessage, sizeof(PaxosUpdate), 0); // Receive message from Paxos service.

        if(numBytesRecvd > 0) {
            bool requestFound = true;
            ntoh(&paxosUpdateMessage, TYPE_PAXOS_UPDATE); // Network to host byte order.
            UpdatePair paxosUpdate(paxosUpdateMessage.clientId, paxosUpdateMessage.timestamp);

            pthread_mutex_lock(&(this->unorderedRequestMapLock));
            bool requestExists = (bool) this->unorderedRequestMap.count(paxosUpdate);
            pthread_mutex_unlock(&(this->unorderedRequestMapLock));
            
            if(requestExists) { // A request exists for the ordered update.
                applyWriteUpdate(paxosUpdate);
            } else {
                requestFound = false;
            }

            if(!requestFound) {
                this->paxosUpdateSet.insert(paxosUpdate);
            }
        } else if(numBytesRecvd == 0) {
            cerr<<"\nPaxos closed the connection.";
            break;
        } else {
            perror("\nFailed to received message from Paxos: recv() failed.");
        }
        
        UpdatePair newPaxosUpdate(this->myId, this->updateTimestamp);
        orderRequest(newPaxosUpdate);
    }
    pthread_exit(0);
}

// Serves the write request corresponding to the Paxos update that has been ordered.
void SyntController::applyWriteUpdate(UpdatePair paxosUpdate) {
    // Extract the request from unorderedRequestMap and erase it from the latter.
    pthread_mutex_lock(&(this->unorderedRequestMapLock));
    SyntMessage *syntRequest = this->unorderedRequestMap[paxosUpdate];
    this->unorderedRequestMap.erase(paxosUpdate);
    pthread_mutex_unlock(&(this->unorderedRequestMapLock));
    
    // Fetch the client socket to write to.
    int clientSocket = -1;
    pthread_mutex_lock(&(this->clientSocketMapLock));
    if(this->clientSocketMap.count(paxosUpdate) == 1) {
        clientSocket = this->clientSocketMap[paxosUpdate];
    }
    pthread_mutex_unlock(&(this->clientSocketMapLock));

    if(clientSocket != -1) { // If the client session is sill active.
        // Apply write to in-memory data store.
        uint32_t result = writeToDataTree(syntRequest);
        
        // Respond to client with the result of the write operation. 
        respondToClient(syntRequest, (char *) &result, sizeof(result), clientSocket);
    }
    
    // Free up memory held by the unordered request.
    if(syntRequest) {
        free(syntRequest);
    }

    // Remove the update from pending update's map.
    if(this->paxosUpdateSet.count(paxosUpdate) == 1) {
        this->paxosUpdateSet.erase(paxosUpdate);
    }
}

// Performs write on the in-memory data store depending on the type of request.
uint32_t SyntController::writeToDataTree(SyntMessage *syntRequest) {
    char *data = NULL;
    uint32_t status, version, flags;
    uint32_t dataSize = syntRequest->dataLength - syntRequest->pathLength;
    string path = getPath(syntRequest);

    if(dataSize > 0) {
        data = new char[dataSize];
        memcpy(data, syntRequest + sizeof(SyntMessage) + syntRequest->pathLength, dataSize);
    }

    switch(syntRequest->opType) {
        case CREATE:
            flags = syntRequest->uint32Value;
            status = dataTree.createNode(path, data, dataSize, version);
            break;

        case SET_DATA:
            version = syntRequest->uint32Value;
            status = dataTree.setData(path, data, dataSize, version);
            break;

        case DELETE:
            version = syntRequest->uint32Value;
            status = dataTree.deleteNode(path, version);
            break;
        
        default:
            status = 0;
    }

    return status;
}

// Handles messages from peer Synt controllers.
void* SyntController::handleSynt(void) {
    while(1) {
        // Receive message from a peer controller in a buffer.
        uint32_t maxMsgSize = sizeof(SyntUpdate) + MAX_PATH_SIZE + MAX_DATA_SIZE * BYTE_TO_MB;
        char *buffer = new char[maxMsgSize];
        struct sockaddr_in peerAddress;
        socklen_t addrLen = sizeof(peerAddress); // TODO peek
        int numBytesRecvd = recvfrom(this->syntListenSocket, (void *) buffer, maxMsgSize, 0, (struct sockaddr *) &peerAddress, &addrLen); // Receive the message from a peer controller.

        if(numBytesRecvd > 0) {
            // Extract the SyntUpdate out of the buffer and convert the byte order.
            SyntUpdate *syntUpdate = (SyntUpdate *) buffer; 
            ntoh(syntUpdate, TYPE_SYNT_UPDATE); 

            // Construct the SyntMessage out of the received SyntUpdate.
            uint32_t syntRequestSize = sizeof(SyntMessage) + syntUpdate->dataLength;
            SyntMessage *syntRequest = (SyntMessage *) malloc(syntRequestSize);
            syntRequest->opType = syntUpdate->opType;
            syntRequest->pathLength = syntUpdate->pathLength;
            syntRequest->dataLength = syntUpdate->dataLength;
            syntRequest->uint32Value = syntUpdate->uint32Value;
            memcpy(syntRequest, syntUpdate + sizeof(SyntUpdate), syntRequest->dataLength);

            // Create the request/update's key.
            UpdatePair paxosUpdate(syntUpdate->clientId, syntUpdate->timestamp);
            
            // Buffer the SyntMessage. It will be used when Paxos orders the update.
            pthread_mutex_lock(&(this->unorderedRequestMapLock));
            this->unorderedRequestMap[paxosUpdate] = syntRequest;
            pthread_mutex_unlock(&(this->unorderedRequestMapLock));
            
            if(this->paxosUpdateSet.count(paxosUpdate) == 1) { // Paxos has already ordered the update.
                applyWriteUpdate(paxosUpdate);
            }
        } else if(numBytesRecvd < 0) {
            perror("\nError while receiving message: recvfrom() failed.");
        } 
        
        delete[] buffer; // Free up the buffer.
    }
    pthread_exit(0);
}

// Utility function to convert messages from host to network byte order.
void SyntController::hton(void *message, uint32_t messageType) {
    switch(messageType) {
        case TYPE_SYNT_MESSAGE:
            {
                SyntMessage *syntRequest = (SyntMessage *) message;
                syntRequest->opType = htonl(syntRequest->opType);
                syntRequest->pathLength = htonl(syntRequest->pathLength);
                syntRequest->dataLength = htonl(syntRequest->dataLength);
                syntRequest->uint32Value = htonl(syntRequest->uint32Value);
            }
            break;

        case TYPE_SYNT_UPDATE:
             {
                SyntUpdate *syntUpdate = (SyntUpdate *) message;
                syntUpdate->clientId = htonl(syntUpdate->clientId);
                syntUpdate->timestamp = htonl(syntUpdate->timestamp);
                syntUpdate->opType = htonl(syntUpdate->opType);
                syntUpdate->pathLength = htonl(syntUpdate->pathLength);
                syntUpdate->dataLength = htonl(syntUpdate->dataLength);
                syntUpdate->uint32Value = htonl(syntUpdate->uint32Value);
            }
            break;

        case TYPE_PAXOS_UPDATE:
            {
                PaxosUpdate *paxosUpdate = (PaxosUpdate *) message;
                paxosUpdate->type = htonl(paxosUpdate->type);
                paxosUpdate->clientId = htonl(paxosUpdate->clientId);
                paxosUpdate->serverId = htonl(paxosUpdate->serverId);
                paxosUpdate->timestamp = htonl(paxosUpdate->timestamp);
                paxosUpdate->update = htonl(paxosUpdate->update);
            }
            break;
    }
}

// Utility function to convert messages from network to host byte order.
void SyntController::ntoh(void *message, uint32_t messageType) {
    switch(messageType) {
        case TYPE_SYNT_MESSAGE:
            {
                SyntMessage *syntRequest = (SyntMessage *) message;
                syntRequest->opType = ntohl(syntRequest->opType);
                syntRequest->pathLength = ntohl(syntRequest->pathLength);
                syntRequest->dataLength = ntohl(syntRequest->dataLength);
                syntRequest->uint32Value = ntohl(syntRequest->uint32Value);
            }
            break;

        case TYPE_SYNT_UPDATE:
            {
                SyntUpdate *syntUpdate = (SyntUpdate *) message;
                syntUpdate->clientId = ntohl(syntUpdate->clientId);
                syntUpdate->timestamp = ntohl(syntUpdate->timestamp);
                syntUpdate->opType = ntohl(syntUpdate->opType);
                syntUpdate->pathLength = ntohl(syntUpdate->pathLength);
                syntUpdate->dataLength = ntohl(syntUpdate->dataLength);
                syntUpdate->uint32Value = ntohl(syntUpdate->uint32Value);
            }
            break;

        case TYPE_PAXOS_UPDATE:
            {
                PaxosUpdate *paxosUpdate = (PaxosUpdate *) message;
                paxosUpdate->type = ntohl(paxosUpdate->type);
                paxosUpdate->clientId = ntohl(paxosUpdate->clientId);
                paxosUpdate->serverId = ntohl(paxosUpdate->serverId);
                paxosUpdate->timestamp = ntohl(paxosUpdate->timestamp);
                paxosUpdate->update = ntohl(paxosUpdate->update);
           }
           break;
    }
}
