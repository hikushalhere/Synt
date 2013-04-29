#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <vector>

#define TYPE_PAXOS_UPDATE 1
#define TYPE_SYNT_MESSAGE 2
#define TYPE_SYNT_MESSAGE_ACK 3
#define TYPE_HEARTBEAT_CHECK 4
#define TYPE_HEARTBEAT_DELETE_SERVER 5

// Data structure to pass to SyntController's constructor.
typedef struct {
    uint32_t myId;
    uint32_t numControllers;
    std::map<uint32_t, std::string> hostNameMap;
    std::string paxosPort;
    std::string syntMessagePort;
    std::string syntAckPort;
    std::string clientListenPort;
    std::string heartbeatPort;
} SyntInfo;

// Message format to for exchanging Synt updates/requests.
#pragma pack(1)
typedef struct {
    uint32_t clientId;
    uint32_t timestamp;
    uint32_t opType;
    uint32_t pathLength;
    uint32_t dataLength;
    uint32_t uint32Value; // Can hold any integer as required. (Version or Flag)
    char data[];
} SyntMessage;
#pragma pack()

// Message format to for acknowledging Synt updates/requests to peers.
#pragma pack(1)
typedef struct {
    uint32_t serverId;
    uint32_t clientId;
    uint32_t timestamp;
} SyntMessageAck;
#pragma pack()

// Message format to communicate with Paxos.
#pragma pack(1)
typedef struct {
    uint32_t type;      // Must be equal to 1.
    uint32_t clientId;
    uint32_t serverId;
    uint32_t timestamp;
    uint32_t update;
} PaxosUpdate;
#pragma pack()

// Message format for heartbeat messages.
#pragma pack(1)
typedef struct {
    uint32_t type;
    uint32_t serverId;
} Heartbeat;
#pragma pack()

#endif
