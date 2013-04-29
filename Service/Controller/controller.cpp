#include <cstring>
#include <fstream>
#include "SyntController.h"

#define NOP 0
#define PAXOS_PORT 1
#define SYNT_PORT 2
#define CLIENT_PORT 3
#define HEARTBEAT_PORT 4
#define HOSTFILE 5

#define MIN_PORT_NUM 1024
#define MAX_PORT_NUM 65535

#define HOST_NAME_LEN 256

using namespace std;

SyntController *bootstrap(string, string, string, string, char *, uint32_t *);
void printUsage();

int main(int argc, char **argv) {
    int nextArg = NOP, paxosPortNum = NOP, syntPortNum = NOP, clientPortNum = NOP, heartbeatPortNum = NOP;
    char *hostFilePath;
    string paxosPort, syntPort, clientPort, heartbeatPort;
    bool proceed = true;

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
                    nextArg = SYNT_PORT;
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
                
                case SYNT_PORT:
                    syntPort = string(argv[i]);
                    syntPortNum = atoi(argv[i]);
                    if(syntPortNum < MIN_PORT_NUM || syntPortNum > MAX_PORT_NUM) {
                        cerr<<"The port number should lie between 1024 and 65535 including both.";
                        proceed = false;
                        continue;
                    }
                    if(syntPortNum == paxosPortNum || syntPortNum == heartbeatPortNum) {
                        cerr<<"The Synt port number must not be the same as Client port number or Heartbeat port number.";
                        proceed = false;
                        continue;
                    }
		    #ifdef DEBUG
		    cout<<"\nSynt port is: "<<syntPort;
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
                    if(clientPortNum == syntPortNum || clientPortNum == heartbeatPortNum) {
                        cerr<<"The Client port number must not be the same as Synt port number or Heartbeat port number.";
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
                    if(heartbeatPortNum == syntPortNum || heartbeatPortNum == clientPortNum) {
                        cerr<<"The Heartbeat port number must not be the same as Synt port number or Client port number.";
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
        SyntController *controller = bootstrap(paxosPort, syntPort, clientPort, heartbeatPort, hostFilePath, &myId);
        if(controller) {
            controller->start();
        }   
    }
}

void printUsage() {
    cout<<"Incorrect usage.";
    cout<<"\nUsage: controller -h hostfile -p paxos_port -s synt_port -c client_port";
    cout.flush();
}

SyntController *bootstrap(string paxosPort, string syntPort, string clientPort, string heartbeatPort, char *hostFilePath, uint32_t *myId) {
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
            syntInfo->syntListenPort = syntPort;
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
