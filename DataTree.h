#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <stdint.h>
#include <pthread.h>
using namespace std;

#define NODE_EPHEMERAL	0x00000001
#define NODE_SEQUENTIAL 0x00000002

struct TreeNode {
	string	label;
	bool	exists;
	bool	ephemeral;
	void	*data;
	int	data_size;
	int	version;
	int	child_seq;
	vector<struct TreeNode *> children;
	struct TreeNode *parent;

	TreeNode();
	TreeNode(string, bool, bool);
};

TreeNode::TreeNode() {
	exists = false;
	ephemeral = false;
	data = NULL;
	data_size = 0;
	version = 0;
	child_seq = 1;
	children.clear();
	parent = NULL;
}

TreeNode::TreeNode(string l, bool ex, bool ep) {
	label = l;
	exists = ex;
	ephemeral = ep;
	data = NULL;
	data_size = 0;
	version = 0;
	children.clear();
	parent = NULL;
}

struct DataTree {
	struct TreeNode *root;
	pthread_mutex_t lock;

	DataTree();
	struct 	TreeNode *get_node(string);
	int 	createNode(string, void *, int, uint32_t);
	bool 	exists(string, bool);
	bool	deleteNode(string, int);
	int 	getData(string, void *, bool, int *);
	bool 	setData(string, void *, int, int);
	vector<string> getChildren(string, bool);
};

DataTree::DataTree() {
	root = NULL;
	while(root == NULL) {
		root = new TreeNode(string("/"), true, false);
	}

	pthread_mutexattr_t mattr;
	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&lock, &mattr);
}
