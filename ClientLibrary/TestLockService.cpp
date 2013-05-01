#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <string>
#include <string.h>
#include <vector>
#include <sys/time.h>
#include "LockService.h"
using namespace std;

int main() {

	Session s;
	Lock my_lock(&s, "/file1");

	bool retval;
	retval = s.createSession("xinu10");
	if(retval == false) {
		exit(1);
	}

	cout << "Acquiring lock..\n";
	retval  = my_lock.getLock();
	cout << "Acquired lock " << retval << "\n";
	cout << "Press Enter to release lock.";
	getchar();
	my_lock.releaseLock();
	cout << "Released lock\n";

	while(1)
		;

	return 0;
}
