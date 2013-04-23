#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <stdint.h>
#include <pthread.h>
using namespace std;

#define NODE_EPHEMERAL	0x00000001
#define NODE_SEQUENTIAL 0x00000002

#define DEBUG

struct TreeNode {
	string		label;
	bool		exists;
	bool		ephemeral;
	void		*data;
	uint32_t	data_size;
	uint32_t	version;
	uint32_t	child_seq;
	vector<struct TreeNode *> children;
	struct 		TreeNode *parent;

	TreeNode();
	TreeNode(string, bool, bool);
};

struct DataTree {
	struct TreeNode *root;
	pthread_mutex_t lock;

	DataTree();
	struct 		TreeNode *get_node(string);
	uint32_t 	createNode(string, void *, uint32_t, uint32_t);
	bool 		exists(string, bool);
	bool		deleteNode(string, uint32_t);
	uint32_t 	getData(string, void *, bool, uint32_t *);
	bool 		setData(string, void *, uint32_t, uint32_t);
	vector<string> 	getChildren(string, bool);
};
