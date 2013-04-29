#include <cstring>
#include <fstream>
#include "SyntController.h"

#define NOP 0
#define PAXOS_PORT 1
#define SYNT_MESSAGE_PORT 2
#define SYNT_ACK_PORT 3
#define CLIENT_PORT 4
#define HEARTBEAT_PORT 5
#define HOSTFILE 6
#define NUM_ARGUMENTS 6 // Must be equal to the the #arguments.

#define MIN_PORT_NUM 1024
#define MAX_PORT_NUM 65535

#define HOST_NAME_LEN 256

using namespace std;

SyntController *bootstrap(string, string, string, string, string, char *, uint32_t *);
void printUsage();

int main(int argc, char **argv) {
    int nextArg = NOP, paxosPortNum, syntMessagePortNum, syntAckPortNum, clientPortNum, heartbeatPortNum;
    char *hostFilePath;
    string paxosPort, syntMessagePort, syntAckPort, clientPort, heartbeatPort;
    bool proceed = true;

    if(argc-1 != NUM_ARGUMENTS) {
        proceed = false;
    }

    paxosPortNum = syntMessagePortNum = syntAckPortNum = clientPortNum = heartbeatPortNum = NOP;
    for(int i = 1; i < argc && proceed; i++) {
        if(argv[i][0] == '-') {
            if(strlen(argv[i]) != 2) {
                proceed = false;
                continue;
            }
            
            switch(argv[i][1]) {
                case 'p':
                    nextArg = PAXOS_PORT;
                    break;
                
                case 's':
                    nextArg = SYNT_MESSAGE_PORT;
                    break;

                case 'a':
                    nextArg = SYNT_ACK_PORT;
                    break;

                case 'c':
                    nextArg = CLIENT_PORT;
                    break;

                case 'h':
                    nextArg = HEARTBEAT_PORT;
                    break;

                case 'f':
                    nextArg = HOSTFILE;
                    break;
                
                default:
                    printUsage();
                    proceed = false;
            }
        } else {
            switch(nextArg) {
                case PAXOS_PORT:
                    paxosPort = string(argv[i]);
                    paxosPortNum = atoi(argv[i]);
                    if(paxosPortNum < MIN_PORT_NUM || paxosPortNum > MAX_PORT_NUM) {
                        cerr<<"The port number should lie between 1024 and 65535 including both.";
                        proceed = false;
                        continue;
                    }
		    #ifdef DEBUG
		    cout<<"\nPaxos port is: "<<paxosPort;
		    #endif
                    break;
                
                case SYNT_MESSAGE_PORT:
                    syntMessagePort = string(argv[i]);
                    syntMessagePortNum = atoi(argv[i]);
                    if(syntMessagePortNum < MIN_PORT_NUM || syntMessagePortNum > MAX_PORT_NUM) {
                        cerr<<"The port number should lie between 1024 and 65535 including both.";
                        proceed = false;
                        continue;
                    }
                    if(syntMessagePortNum == paxosPortNum || syntMessagePortNum == syntAckPortNum || syntMessagePortNum == heartbeatPortNum) {
                        cerr<<"The Synt Message port number must not be the same as Synt Ack port number or Client port number or Heartbeat port number.";
                        proceed = false;
                        continue;
                    }
		    #ifdef DEBUG
		    cout<<"\nSynt message port is: "<<syntMessagePort;
		    #endif
                    break;
                
                case SYNT_ACK_PORT:
                    syntAckPort = string(argv[i]);
                    syntAckPortNum = atoi(argv[i]);
                    if(syntAckPortNum < MIN_PORT_NUM || syntAckPortNum > MAX_PORT_NUM) {
                        cerr<<"The port number should lie between 1024 and 65535 including both.";
                        proceed = false;
                        continue;
                    }
                    if(syntAckPortNum == paxosPortNum || syntAckPortNum == syntMessagePortNum || syntAckPortNum == heartbeatPortNum) {
                        cerr<<"The Synt Ack port number must not be the same as Synt Message port number or Client port number or Heartbeat port number.";
                        proceed = false;
                        continue;
                    }
		    #ifdef DEBUG
		    cout<<"\nSynt port is: "<<syntMessagePort;
		    #endif
                    break;

                case CLIENT_PORT:
                    clientPort = string(argv[i]);
                    clientPortNum = atoi(argv[i]);
                    if(clientPortNum < MIN_PORT_NUM || clientPortNum > MAX_PORT_NUM) {
                        cerr<<"The port number should lie between 1024 and 65535 including both.";
                        proceed = false;
                        continue;
                    }
                    if(clientPortNum == syntMessagePortNum || clientPortNum == syntAckPortNum || clientPortNum == heartbeatPortNum) {
                        cerr<<"The Client port number must not be the same as Synt Message port number or Synt Ack port number or Heartbeat port number.";
                        proceed = false;
                        continue;
                    }
		    #ifdef DEBUG
		    cout<<"\nClient port is: "<<clientPort;
		    #endif
                    break;

                case HEARTBEAT_PORT:
                    heartbeatPort = string(argv[i]);
                    heartbeatPortNum = atoi(argv[i]);
                    if(heartbeatPortNum < MIN_PORT_NUM || heartbeatPortNum > MAX_PORT_NUM) {
                        cerr<<"The port number should lie between 1024 and 65535 including both.";
                        proceed = false;
                        continue;
                    }
                    if(heartbeatPortNum == syntMessagePortNum || heartbeatPortNum == syntAckPortNum || heartbeatPortNum == clientPortNum) {
                        cerr<<"The Heartbeat port number must not be the same as Synt Message port number or Synt Ack port number or Client port number.";
                        proceed = false;
                        continue;
                    }
		    #ifdef DEBUG
		    cout<<"\nHeartbeat port is: "<<heartbeatPort;
		    #endif
                    break;

                case HOSTFILE:
                    hostFilePath = argv[i];
		    #ifdef DEBUG
		    cout<<"\nHost file is: "<<hostFilePath;
		    #endif
                    break;
                
                case NOP:
                    printUsage();
                    proceed = false;
                    break;
            }
        }
    }

    if(proceed) {
        uint32_t myId;
        SyntController *controller = bootstrap(paxosPort, syntMessagePort, syntAckPort, clientPort, heartbeatPort, hostFilePath, &myId);
        if(controller) {
            controller->start();
        }   
    } else {
        printUsage();
    }
}

void printUsage() {
    cout<<"Incorrect usage.";
    cout<<"\nUsage: controller -p paxos_port -s synt_message_port -a synt_ack_port -h heartbeat_port -c client_port -f host_file\n";
    cout.flush();
}

SyntController *bootstrap(string paxosPort, string syntMessagePort, string syntAckPort, string clientPort, string heartbeatPort, char *hostFilePath, uint32_t *myId) {
    bool foundMyself = false;
    int status, numControllers = 0;
    char myHostName[HOST_NAME_LEN];
    map<uint32_t, string> hostNameMap;
    ifstream hostfile(hostFilePath);
    SyntController *controller = NULL;

    if(hostfile.is_open()) {
        if((status = gethostname(myHostName, HOST_NAME_LEN)) != 0) {
            perror("Error encountered in fetching my host name.");
        }
        while(hostfile.good()) {
            string hostName;
            getline(hostfile, hostName);
            if(!hostName.empty()) {
                ++numControllers;
                hostNameMap[numControllers] = hostName;
                if(strcmp(myHostName, hostName.c_str()) == 0) {
                    *myId = numControllers;
                    foundMyself = true;
                }
            }
        }
        hostfile.close();
    }

    if(foundMyself) {
        SyntInfo *syntInfo = new SyntInfo;

        if(syntInfo) {
            syntInfo->myId = *myId;
            syntInfo->numControllers = numControllers;
            syntInfo->hostNameMap = hostNameMap;
            syntInfo->paxosPort = paxosPort;
            syntInfo->syntMessagePort = syntMessagePort;
            syntInfo->syntAckPort = syntAckPort;
            syntInfo->clientListenPort = clientPort;
            syntInfo->heartbeatPort = heartbeatPort;
            
            try {
                controller = new SyntController(syntInfo);
            } catch(string msg) {
                cerr<<msg;
            }
            delete syntInfo;
        }
    } else {
        cerr<<"\nMy hostname was not found in the file: "<<hostFilePath;
    }

    return controller;
}
