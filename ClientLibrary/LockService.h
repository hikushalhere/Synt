#include <iostream>
#include <cstdlib>
#include <string>
#include <stdint.h>
#include <string.h>
#include <vector>
#include "client_api.h"

struct Lock {
	Session		*s;
	string		label;
	uint32_t	seq_num;
	bool		locked;

	Lock(Session *, string);
	bool	getLock();
	bool	releaseLock();
};
