#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <common/dns.h>
#include "client.h"

struct dns_message {
	struct dns_question *question;
	struct dns_section section[SECTION_MAX];
	TAILQ_ENTRY(dns_message) list;
	struct dns_client *client;
	struct rte_mempool *name_pool;
	struct rte_mempool *question_pool;
	struct rte_mempool *rr_pool;
	int rcode;
	int request;
};
TAILQ_HEAD(dns_message_queue, dns_message);

#endif