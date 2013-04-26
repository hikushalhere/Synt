#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <string>
#include <string.h>
#include <vector>
#include "LockService.h"
using namespace std;

Lock::Lock(Session *t, string l) {
	s = t;
	label = l + "/lock";
	locked = false;
}

bool Lock::getLock() {

	uint32_t	seq;
	bool		retval;
	char		prev_label[50];

	if(locked == true) {
		return false;
	}

	seq = s->createNode(label, NULL, 0, NODE_EPHEMERAL | NODE_SEQUENTIAL, false);
	if(seq == 0) {
		return false;
	}
	seq_num = seq;

	sprintf(prev_label, "%d", seq - 1);
	do {
		retval = s->exists(label + string(prev_label), true, false);
	} while(retval == true);

	locked = true;
	return true;
}

bool Lock::releaseLock() {

	bool	retval;
	char	temp_string[50];

	sprintf(temp_string, "%d", seq_num);
	retval = s->deleteNode(label + string(temp_string), 1, false);
	
	if(retval = false) {
		retval = s->exists(label + string(temp_string), false, false);
		if(retval == false) {
			locked = false;
			return true;
		}
		else {
			return false;
		}
	}

	return true;
}

int main() {

	Session s;
	Lock my_lock(&s, "/file1");

	bool retval;
	retval = s.createSession("xinu10");
	cout << "Acquiring lock..\n";
	retval  = my_lock.getLock();
	cout << "Acquired lock " << retval << "\n";
	cout << "Press Enter to release lock.";
	int i;
	cin >> i;
	my_lock.releaseLock();
	cout << "Released lock\n";

	while(1)
		;

	return 0;
}
