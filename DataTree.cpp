#include <iostream>
#include <cstdlib>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <pthread.h>
#include "DataTree.h"
using namespace std;

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
	child_seq = 1;
	children.clear();
	parent = NULL;
}

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

vector <string> parse_path(string path) {

	vector<string> retval;
	unsigned pos;

	if(path.length() == 0) {
		return retval;
	}

	pos = path.find_first_of("/");
	if(pos != 0) {
		cout << "Invalid path " << path << "\n";
		return retval;
	}

	size_t start, end;
	start = 0;
	while(1) {
		start = path.find_first_of("/", start);
		if(start == string::npos) {
			break;
		}
		start += 1;

		end = path.find_first_of("/", start);
		if(end == string::npos) {
			end = path.length() - 1;
		}
		else {
			end -= 1;
		}

		if(start > end) {
			continue;
		}
		
		retval.push_back(path.substr(start, end - start + 1));
	}
	return retval;
}

struct TreeNode *DataTree::get_node(string path) {

	vector<string> parsed_path;

	/* Special case: root */
	if(path == "/") {
		return root;
	}

	parsed_path = parse_path(path);
	if(parsed_path.size() == 0) {
		return NULL;
	}

	struct TreeNode *curr;
	vector<string>::iterator piter;

	pthread_mutex_lock(&lock);
	curr = root;
	for(piter = parsed_path.begin(); piter != parsed_path.end(); piter++) {
		vector<struct TreeNode *>::iterator citer;

		for(citer = curr->children.begin(); citer != curr->children.end(); citer++) {
			if((*citer)->label == (*piter)) {
				break;
			}
		}

		if(citer != curr->children.end()) {
			if(piter != (parsed_path.end() - 1)) {
				if((*citer)->exists == true) {
					curr = (*citer);
					continue;
				}
				else {
					pthread_mutex_unlock(&lock);
					return NULL;
				}
			}
			else {
				if((*citer)->exists == true) {
					pthread_mutex_unlock(&lock);
					return (*citer);
				}
				else {
					pthread_mutex_unlock(&lock);
					return NULL;
				}
			}
		}
		else {
			pthread_mutex_unlock(&lock);
			return NULL;
		}

	}
}

uint32_t DataTree::createNode(string path, void *data, uint32_t data_size, uint32_t flags) {

	vector<string>parsed_path;

	parsed_path = parse_path(path);
	if(parsed_path.size() == 0) {
		return 0;
	}

	bool ephemeral, sequential;
	int  max_seq;
	char temp_string[10];
	size_t pos;
	struct TreeNode *curr = root, *new_node;
	vector<string>::iterator piter;

	ephemeral = false;
	if(flags & 0x00000001) {
		ephemeral = true;
	}

	sequential = false;
	if(flags & 0x00000002) {
		sequential = true;
	}

	pthread_mutex_lock(&lock);
	for(piter = parsed_path.begin(); piter != parsed_path.end(); piter++) {
		vector<struct TreeNode *>::iterator citer;

		for(citer = curr->children.begin(); citer != curr->children.end(); citer++) {
			if((*citer)->label == (*piter)) {
				break;
			}
		}

		if(piter != (parsed_path.end() - 1)) { /* Intermediate level */
			if(citer != curr->children.end()) {
				curr = *citer;
				continue;
			}
			else {
				new_node = NULL;
				while(new_node == NULL) {
					new_node = new TreeNode(*piter, true, ephemeral);
				}
				curr->children.push_back(new_node);
				new_node->parent = curr;
				curr = new_node;
				continue;
			}
		}
		else { /* Last level */
			if(citer != curr->children.end()) {
				if(sequential == true) {
					max_seq = 0;
					for(citer = curr->children.begin(); citer != curr->children.end(); citer++) {
						pos = (*citer)->label.find(*piter);
						if(pos != 0) {
							continue;
						}
						if(atoi(&((*citer)->label.data()[pos + (*piter).size()])) > max_seq) {
							max_seq = atoi(&((*citer)->label.data()[pos + (*piter).size()]));
							continue;
						}
					}

					sprintf(temp_string, "%d", max_seq+1);
					new_node = NULL;
					while(new_node == NULL) {
						new_node = new TreeNode(string(*piter + string(temp_string)), true, ephemeral);
					}
					curr->children.push_back(new_node);
					new_node->parent = curr;
					new_node->version = 1;
					if(data != NULL) {
						memcpy(new_node->data, data, data_size);
					}
					else {
						new_node->data = NULL;
						new_node->data_size = 0;
					}
					pthread_mutex_unlock(&lock);
					return (max_seq + 1);
				}
				else {
					if((*citer)->exists == false) {
						(*citer)->exists = true;
						(*citer)->ephemeral = ephemeral;
						(*citer)->version = 1;
						if((*citer)->data != NULL) {
							free(data);
						}
						if(data != NULL) {
							memcpy((*citer)->data,  data, data_size);
							(*citer)->data_size = data_size;
						}
						else {
							(*citer)->data = NULL;
							(*citer)->data_size = 0;
						}
						//TODO Check if watch is set at this point.
					}
					else {
						pthread_mutex_unlock(&lock);
						return 0;
					}
				}
					
			}
			else {
				if(sequential == true) {
					max_seq = 0;
					for(citer = curr->children.begin(); citer != curr->children.end(); citer++) {
						pos = (*citer)->label.find(*piter);
						if(pos != 0) {
							continue;
						}
						if(atoi(&((*citer)->label.data()[pos + (*piter).size()])) > max_seq) {
							max_seq = atoi(&((*citer)->label.data()[pos + (*piter).size()]));
							continue;
						}
					}

					sprintf(temp_string, "%d", max_seq+1);
					new_node = NULL;
					while(new_node == NULL) {
						new_node = new TreeNode(string(*piter + string(temp_string)), true, ephemeral);
					}
					curr->children.push_back(new_node);
					new_node->parent = curr;
					new_node->version = 1;
					if(data != NULL) {
						memcpy(new_node->data, data, data_size);
					}
					else {
						new_node->data = NULL;
						new_node->data_size = 0;
					}
					pthread_mutex_unlock(&lock);
					return (max_seq + 1);
				}
				else {
					new_node = NULL;
					while(new_node == NULL) {
						new_node = new TreeNode(*piter, true, ephemeral);
					}
					curr->children.push_back(new_node);
					new_node->parent = curr;
					new_node->version = 1;
					if(data != NULL) {
						memcpy(new_node->data, data, data_size);
					}
					else {
						new_node->data = NULL;
						new_node->data_size = 0;
					}
					pthread_mutex_unlock(&lock);
					return 1;
				}
			}
		}
	}
}

bool DataTree::exists(string path, bool watch) {

	vector<string> parsed_path;

	/* special case: root */
	if(path == "/") {
		return true;
	}

	parsed_path = parse_path(path);
	if(parsed_path.size() == 0) {
		return false;
	}

	vector<string>::iterator piter;
	struct TreeNode *curr, *new_node;

	pthread_mutex_lock(&lock);
	curr = root;
	for(piter = parsed_path.begin(); piter != parsed_path.end(); piter++) {
		vector<struct TreeNode *>::iterator citer;

		for(citer = curr->children.begin(); citer != curr->children.end(); citer++) {
			if((*citer)->label == (*piter)) {
				break;
			}
		}

		if(piter != (parsed_path.end() - 1)) { /* Intermediate level */
			if(citer != curr->children.end()) {
				if((*citer)->exists == true) {
					curr = (*citer);
					continue;
				}
				else {
					if(watch == true) {
						new_node = NULL;
						while(new_node == NULL) {
							new_node = new TreeNode(*piter, false, true);
						}
						curr->children.push_back(new_node);
						new_node->parent = curr;
						curr = new_node;
						continue;
					}
					else {
						pthread_mutex_unlock(&lock);
						return false;
					}
				}
			}
			else {
				if(watch == true) {
					new_node = NULL;
					while(new_node == NULL) {
						new_node = new TreeNode(*piter, false, true);
					}
					curr->children.push_back(new_node);
					new_node->parent = curr;
					curr = new_node;
					continue;
				}
				else {
					pthread_mutex_unlock(&lock);
					return false;
				}
			}
		}
		else { /* Last Level */
			if(citer != curr->children.end()) {
				if((*citer)->exists == true) {
					pthread_mutex_unlock(&lock);
					return true;
				}
				else {
					if(watch == true) {
						//TODO insert watch if necessary
					}
					pthread_mutex_unlock(&lock);
					return false;
				}
						
			}
			else {
				if(watch == true) {
					//TODO
					new_node = NULL;
					while(new_node == NULL) {
						new_node = new TreeNode(*piter, false, true);
					}
					curr->children.push_back(new_node);
					new_node->parent = curr;
					pthread_mutex_unlock(&lock);
					return false;
				}
				else {
					pthread_mutex_unlock(&lock);
					return false;
				}
			}
		}
	}

	pthread_mutex_unlock(&lock);
	return true;
}

bool DataTree::deleteNode(string path, uint32_t version) {

	struct TreeNode *n;
	vector<struct TreeNode *>::iterator citer;

	pthread_mutex_lock(&lock);

	n = get_node(path);
	if(n == NULL) {
		pthread_mutex_unlock(&lock);
		return false;
	}

	if(n == root) {
		pthread_mutex_unlock(&lock);
		return false;
	}

	if(n->children.size() > 0) {
		pthread_mutex_unlock(&lock);
		return false;
	}

	if(n->version != version) {
		pthread_mutex_unlock(&lock);
		return false;
	}

	//TODO check what to do of all the watches set on this node
	for(citer = n->parent->children.begin(); citer != n->parent->children.end(); citer++) {
		if((*citer) == n) {
			break;
		}
	}

	if(citer != n->parent->children.end()) {
		n->parent->children.erase(citer);

		if(n->data != NULL) {
			free(n->data);
			delete n;
		}
		pthread_mutex_unlock(&lock);
		return true;
	}
	else { //Something's wrong!
		pthread_mutex_unlock(&lock);
		return false;
	}

}

uint32_t DataTree::getData(string path, void *data, bool watch, uint32_t *version) {

	struct TreeNode *n;
	int len;

	pthread_mutex_lock(&lock);
	n = get_node(path);

	if(n == NULL) {
		pthread_mutex_unlock(&lock);
		*version = 0;
		return 0;
	}

	if(n->data == NULL) {
		pthread_mutex_unlock(&lock);
		*version = 0;
		return 0;
	}

	memcpy(data, n->data, n->data_size);
	len = n->data_size;

	//TODO watch
	*version = n->version;

	pthread_mutex_unlock(&lock);
	return len;
}

bool DataTree::setData(string path, void *data, uint32_t data_len, uint32_t version) {

	struct TreeNode *n;

	if(data == NULL) {
		return false;
	}

	pthread_mutex_lock(&lock);
	n = get_node(path);

	if(n == NULL) {
		pthread_mutex_unlock(&lock);
		return false;
	}

	if(n->version != version) {
		return false;
		pthread_mutex_unlock(&lock);
	}

	if(n->data != NULL) {
		free(data);
		n->data_size = 0;
	}

	n->data = data;
	n->data_size = data_len;

	pthread_mutex_unlock(&lock);
	return true;

}

vector<string> DataTree::getChildren(string path, bool watch) {

	struct TreeNode *n;
	vector<string> retvec;
	vector<struct TreeNode *>::iterator citer;

	pthread_mutex_lock(&lock);

	n = get_node(path);
	if(n == NULL) {
		pthread_mutex_unlock(&lock);
		return retvec;
	}

	for(citer = n->children.begin(); citer != n->children.end(); citer++) {
		if((*citer)->exists == true) {
			retvec.push_back((*citer)->label);
		}
	}

	//TODO watch
	
	pthread_mutex_unlock(&lock);
	return retvec;
}

int main() {

	string p1("/abc//def/ghi/jkl");

	vector<string>sv;
	cout << "Size of string vector " << sv.size() << "\n";

	vector<string>parsed = parse_path(p1);
	vector<string>::iterator it;

	cout << "path break up is " ;
	for(it = parsed.begin(); it != parsed.end(); it++) {
		cout << *it << ", ";
	}
	cout << "\n";

	DataTree t;

	int retval;
	retval = t.createNode("/a/b/c", NULL,0, 0);
	cout << "retval " << retval << "\n";
	retval = t.createNode("/a/b/c", NULL,0, 0);
	cout << "retval " << retval << "\n";

	retval = t.createNode("/q/rajas", NULL, 0, 0);
	cout << "retval " << retval << "\n";
	retval = t.createNode("/q/rajas", NULL, 0, NODE_SEQUENTIAL);
	cout << "retval " << retval << "\n";
	retval = t.createNode("/q/rajas", NULL, 0, NODE_SEQUENTIAL);
	cout << "retval " << retval << "\n";
	retval = t.createNode("/q/rajas", NULL, 0, NODE_SEQUENTIAL);
	cout << "retval " << retval << "\n";
	retval = t.createNode("/q/rajas", NULL, 0, NODE_SEQUENTIAL);
	cout << "retval " << retval << "\n";
	retval = t.deleteNode("/q/rajas1", 0);
	cout << "delete retval " << retval << "\n";
	retval = t.createNode("/q/rajas", NULL, 0, NODE_SEQUENTIAL);
	cout << "retval " << retval << "\n";

	vector<struct TreeNode*>::iterator citer;
	for(citer = t.root->children[1]->children.begin(); citer != t.root->children[1]->children.end(); citer++) {
		cout << (*citer)->label << " ";
	}
	cout << "\n";

	retval = t.exists(string("/cs505/rajas"), true);
	cout << "exists retval " << retval << "\n";

	cout << "cs505 e " << t.root->children[2]->exists << " rajas e " <<t.root->children[2]->children[0]->exists << "\n";

	struct TreeNode *n = t.get_node("/q/rajas3");
	cout << "node " << n << " label " << n->label << "\n";

	retval = t.deleteNode("/cs505/rajas", 0);
	cout << "delete retval " << retval << "\n";

	cout << "cs505 e " << t.root->children[2]->exists << " rajas e " <<t.root->children[2]->children[0]->exists << "\n";

	vector<string> children;

	children = t.getChildren("/", false);
	for(int i = 0; i < children.size(); i++) {
		cout << children[i] << ", ";
	}
	cout << "\n";
	return 0;
}
