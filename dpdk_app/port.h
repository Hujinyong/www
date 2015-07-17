#ifndef _PORT_H_
#define _PORT_H_

#define MAX_PORT_NAME 64

typedef struct port_conf_s {
	unsigned port_id;
	char port_name[MAX_PORT_NAME];
	unsigned promiscuous;
	unsigned ipv4;
	struct ether_addr mac;
} port_conf_t;

int port_register(struct port_conf_s *);

#endif