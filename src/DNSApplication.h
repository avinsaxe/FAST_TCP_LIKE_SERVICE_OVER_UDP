#include "common.h"

//href:// hw2 shared by prof Dmitri.
using namespace std;

class QueryHeader {
#pragma pack(push,1)
public:
	u_short type;
	u_short clazz;
};
#pragma pack(pop)

#pragma pack(push,1)
class FixedDNSHeader {
public:
	u_short ID;
	u_short flags;
	u_short questions;
	u_short answers;
	u_short authority;	
	u_short additionalz;
};
#pragma pack(pop)

#pragma pack(push,1)
class FixedDNSAnswerHeader {
public:
	u_short type;
	u_short clazz;
	u_int ttl;
	u_short len;
};
#pragma pack(pop)
