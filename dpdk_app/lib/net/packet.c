#include <rte_ether.h>

#include "af_inet.h"
#include "ipv4.h"
#include "packet.h"

struct ptype_list ptype_base;

int 
packet_type_add(struct packet_type *pt)
{
	LIST_INSERT_HEAD(&ptype_base, pt, list);
	return 0;
}

struct packet_type ipv4_packet = {
	.type = ETHER_TYPE_IPv4,
	.func = ipv4_rcv,
	.list = LIST_HEAD_INITIALIZER(NULL)
};

int 
packet_init(void) 
{
	int retval;

	retval = packet_type_add(&ipv4_packet);
	
	return retval;
}
	