#include <stdint.h>

#define TYPE_CLIENT_UPDATE 	1
#define TYPE_VIEW_CHANGE	2
#define TYPE_VC_PROOF		3
#define TYPE_PREPARE		4
#define TYPE_PROPOSAL		5
#define TYPE_ACCEPT		6
#define TYPE_GOU		7
#define TYPE_PREPARE_OK		8

#pragma pack(2)
typedef struct {
	uint32_t type;
	uint32_t client_id;
	uint32_t server_id;
	uint32_t timestamp;
	uint32_t update;
} Client_Update;
#pragma pack()

#pragma pack(2)
typedef struct {
	uint32_t type;
	uint32_t server_id;
	uint32_t attempted;
} View_Change;
#pragma pack()

#pragma pack(2)
typedef struct {
	uint32_t type;
	uint32_t server_id;
	uint32_t installed;
} VC_Proof;
#pragma pack()

#pragma pack(2)
typedef struct {
	uint32_t type;
	uint32_t server_id;
	uint32_t view;
	uint32_t local_aru;
} Prepare;
#pragma pack()

#pragma pack(2)
typedef struct {
	uint32_t type;
	uint32_t server_id;
	uint32_t view;
	uint32_t seq;
	Client_Update update;
} Proposal;
#pragma pack()

#pragma pack(2)
typedef struct {
	uint32_t type;
	uint32_t server_id;
	uint32_t view;
	uint32_t seq;
} Accept;
#pragma pack()

#pragma pack(2)
typedef struct {
	uint32_t type;
	uint32_t server_id;
	uint32_t seq;
	Client_Update update;
} Globally_Ordered_Update;
#pragma pack()

#pragma pack(2)
typedef struct {
	uint32_t type;
	uint32_t server_id;
	uint32_t view;
	//uint32_t total_proposals;
	//struct Proposal proposals[];
	//uint32_t total_globally_ordered_updates;
	//struct Globally_Ordered_Update globally_ordered_updates[];
} Prepare_OK;
#pragma pack()
