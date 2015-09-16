#include "client.h"
#include "dispatch.h"

struct rte_mempool *dns_client_pool;
/*process dns request from stub dns*/
void process_client_request(struct rte_mbuf *mbuf, uint32_t addr, uint16_t port)
{
	struct dns_client *client;

	client = dns_client_create(dns_client_pool, addr, port);
	if (client == NULL) 
		return;
	

}

/*process */
int process_server_response()
{
    return 0;
}

int dispatch_dns_pkt(struct rte_mbuf *mbuf, uint32_t addr, uint16_t port)
{
    struct dns_hdr *dnshdr;

    dnshdr = rte_pktmbuf_mtod(mbuf, struct dns_hdr*);
    if (dnshdr->qr == 0) {
        /*dns request*/
        process_client_request(mbuf, addr, port);
    } else {
        /*dns response*/
        process_server_response();
    }
    printf("%u  %u\n", addr, port);

    return 0;
}
