#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <vector>

// Data structure to pass to SyntController's constructor.
typedef struct {
    uint32_t myId;
    uint32_t numControllers;
    std::vector<std::string> hostNames;
    std::string paxosPort;
    std::string syntListenPort;
    std::string clientListenPort;
} SyntInfo;

// Message format to communicate with client.
typedef struct {
    uint32_t opType;
    uint32_t pathLength;
    uint32_t dataLength;
    uint32_t uint32Value; // Can hold any integer as required. (Version or Flag)
    char data[];
} SyntMessage;

// Message format to communicate with peer Synt controllers.
typedef struct {
    uint32_t clientId;
    uint32_t timestamp;
    // The following fields are from a SyntMessage that this update should carry.
    uint32_t opType;
    uint32_t pathLength;
    uint32_t dataLength;
    uint32_t uint32Value;
    char data[];
} SyntUpdate;

// Message format to communicate with Paxos.
typedef struct {
    uint32_t type;      // Must be equal to 1.
    uint32_t clientId;
    uint32_t serverId;
    uint32_t timestamp;
    uint32_t update;
} PaxosUpdate;

#endif
