#include <iostream>
#include <cstdlib>
#include <vector>
#include <string>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include "ClientApi.h"
using namespace std;

int main() {

	Session s;
	string server, command, path, data;
	char recv_data[100];
	uint32_t version;
	int retval;

	cout << "size of request = " << sizeof(Request) << "\n";
	cout << "Enter server: ";
	getline(cin, server);

	retval = s.createSession(server);
	if(retval == false) {
		cout << "Cannot establish session with server " << server << "\n";
		exit(1);
	}

	while(1) {


		cout << "Enter command: ";
		getline(cin, command);
		cout << "Enter path: ";
		getline(cin, path);

		if(command == "create") {

			string eph, seq;
			uint32_t flags = 0;

			cout << "Enter data: ";
			getline(cin, data);
			cout << "Ephemeral? ";
			getline(cin, eph);
			cout << "Sequential? ";
			getline(cin, seq);

			if(eph == "yes") {
				flags |= NODE_EPHEMERAL;
			}
			if(seq == "yes") {
				flags |= NODE_SEQUENTIAL;
			}

			retval = s.createNode(path, (void *)data.c_str(), data.size(), flags, false);
			cout << "retval = " << retval << "\n";
			continue;
		}
		if(command == "exists") {
		
			retval = s.exists(path, false, false);
			cout << "retval = " << retval << "\n";
			continue;
		}
		if(command == "delete") {
		
			string version_str;
			cout << "Enter version: ";
			getline(cin, version_str);
			version = atoi(version_str.c_str());

			retval = s.deleteNode(path, version, false);
			cout << "retval = " << retval << "\n";
			continue;
		}
		if(command == "getchildren") {
			vector<string> children;
			int i;
			children = s.getChildren(path, false, false);
			cout << "Children: ";
			for(i = 0; i < children.size(); i++) {
				cout << children[i] << ", ";
			}
			cout << "\n";
			continue;
		}
		if(command == "getdata") {
		
			retval = s.getData(path, recv_data, false, &version, false);
			cout << "retval = " << retval << "\n";
			cout << "Data: " << recv_data << "\n";
			cout << "Version = " << version << "\n";
			continue;
		}
		if(command == "setdata") {
		
			string version_str;
			cout << "Enter data: ";
			getline(cin, data);
			cout << "Enter version: ";
			getline(cin, version_str);
			version = atoi(version_str.c_str());

			retval = s.setData(path, (void *)data.c_str(), data.size(), version, false);
			cout << "retval = " << retval << "\n";
			continue;
		}

	}
	
	return 0;
}
