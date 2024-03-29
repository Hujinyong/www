#include <rte_ether.h>

#include "af_inet.h"
#include "ipv4.h"
#include "arp.h"
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

struct packet_type arp_packet = {
	.type = ETHER_TYPE_ARP,
	.func = arp_rcv,
	.list = LIST_HEAD_INITIALIZER(NULL)
};

int packet_xmit(unsigned port, struct rte_mbuf *mbuf, be32 daddr)
{
	struct net_device *ndev = net_device_get(port);
	struct arp_node *node;
		
	NET_ASSERT(ndev != NULL && mbuf != NULL);

	node = arp_node_lookup(ndev, daddr, 1);
	if (node == NULL) {
		rte_free(mbuf);		
		return -1;
	}
	
	return node->sendpkt(node, mbuf);
}

int 
packet_init(void) 
{
	int retval;

	retval = packet_type_add(&ipv4_packet);
	retval = packet_type_add(&arp_packet);
	
	return retval;
}
	
