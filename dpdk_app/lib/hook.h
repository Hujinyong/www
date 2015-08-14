#ifndef _HOOK_H_
#define _HOOK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/queue.h>

#include "buffer.h"

enum {
	HOOK_RET_ACCEPT = 1,
	HOOK_RET_DROP
};

enum {
	HOOK_PROTO_ARP = 0,
	HOOK_PROTO_IPV4,
	HOOK_PROTO_UDP,
	HOOK_PROTO_MAX = 8
};

enum {
	HOOK_POS_IN = 0,
	HOOK_POS_OUT,
	HOOK_POS_MAX = 8
};

struct hook_ops {
	int proto;
	int pos;
	int (*func)(struct rte_mbuf *m);
	LIST_ENTRY(hook_ops) list;
};

typedef void (*hook_ok)(struct rte_mbuf *m);

LIST_HEAD(hook_head, hook_ops);

extern struct hook_head hhead[HOOK_PROTO_MAX][HOOK_POS_MAX];

static inline void hook_proccess(struct rte_mbuf *m, uint8_t proto, uint8_t pos, 
	hook_ok ok)
{
	struct hook_ops *ops;
	int ret;
	
	LIST_FOREACH(ops, &hhead[proto][pos], list) {
		ret = ops->func(m);
		if (ret == HOOK_RET_DROP)
			break;
	}
	if (ret == HOOK_RET_ACCEPT) {
		ok(m);
		return;
	}
	TRACE_DROP_MBUF(m, 1);
	rte_pktmbuf_free(m);	
}

int hook_register(struct hook_ops *ops);
void hook_unregister(struct hook_ops *ops);

#ifdef __cplusplus
}
#endif

#endif
