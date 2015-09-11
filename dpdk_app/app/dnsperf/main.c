#include "packet_construct.h"

static uint16_t nb_tx_desc = 128;
static uint16_t nb_tx_queue = 8;
struct rte_mempool *rxpool;
static rte_ring *reqest_ring_per_socket[RTE_MAX_NUMA_NODES];
#define NB_ELEM_PER_RING 4096
#define NB_BURST 32
#define NB_TX_POOL 4096

static char* request_file[RTE_MAX_LCORE];
static char* request_file_share;

static struct proto_parameter {	
	uint32_t dst_addr;
	uint32_t src_addr;
	uint16_t dst_port;
	uint16_t src_port;
	struct ether_addr dst_mac;
	struct ether_addr src_mac;
} default_proto_param = {
	.dst_addr = 0,
	.src_addr = 0,
	.src_port = 12306,
	.dst_port = 53,
};

struct request_data {
	uint8_t req_type;
	int size;
	char req_domain[0];	
} __attribute((packed));

struct mbuf_table {
	struct rte_mbuf *mbuf[NB_BURST];
	int length;
}

static struct rte_eth_conf default_port_conf = {
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
}; 

static int format_domain(char* orig, char* out, int size); 

static void nothing_to_do(void)
{
	printf("nothing to do on lcore %u\n", rte_lcore_id());
}

static void packet_construct_task(void)
{
#define LINE_SIZE 1024
	FILE *f;
	struct rte_ring *ring;
	char *file_path;
	char line[LINE_SIZE];
	char domain[LINE_SIZE];
	int domain_len;
	char *pch;
	request_data_t *req;
	int ret;

	ring = reqest_ring_per_socket[rte_socket_id()];
	if (ring == NULL) {
		printf("no ring on core %u\n", rte_lcore_id());
		return;
	}

	file_path = request_file[rte_lcore_id()];
	if (file_path == NULL) {
		file_path = request_file_share;
		if (file_path == NULL) {
			printf("no reqeust file on core %u\n", rte_lcore_id());
			return;
		}
	}

	f = fopen(file_path, "r");
	if (f == NULL) {
		printf("cannot open file %s\n", file_path);
		return;
	}

	while (1) {		
		while (fgets(line, LINE_SIZE, f)) {
			domain_len = 0;
			pch = strtok(line, " \n");		
			if (pch == NULL) {
				continue;
			} else {
				strcpy(domain, pch);
				domain_len = strnlen(domain, LINE_SIZE) + 1 + 1;
			}
			pch = strtok(NULL, " \n");
			if (pch == NULL) {
				continue;
			} else if (strcmp(pch, "A") == 0) { 
				req = rte_malloc_socket(NULL, sizeof(sizeof(struct request_data) + domain_len), 0, rte_socket_id());
				if (req == NULL)
					break;
				if (format_domain(domain, req->req_domain, domain_len) == 0) {
					rte_free(req);
					continue;
				}
				req->size = domain_len;
				req->req_type = DNS_QTYPE_A; 		
				ret = rte_ring_enqueue(ring, (void*)req);
				if (ret < 0)
					rte_free(req);
			} else if (strcmp(pch, "PTR") == 0) {
				req = rte_malloc_socket(NULL, sizeof(sizeof(request_data_t) + domain_len), 0, rte_socket_id());
				if (req == NULL)
					break;
				if (format_domain(domain, req->req_domain, domain_len) == 0) {
					rte_free(req);
					continue;
				}
				req->size = domain_len;
				req->req_type = DNS_QTYPE_PTR;			
				ret = rte_ring_enqueue(ring, (void*)req);
				if (ret < 0)
					rte_free(req);
			} else {
				printf("%s %d %s %lu\n", __func__, __LINE__, line, strlen(pch));
			}	
		}
		
		if (feof(f)) {
			rewind(f);
		}
	}
}

static void packet_send_task(void)
{
	struct rte_ring *ring;
	struct request_data *req;
	struct mbuf_table mtable;
	struct rte_mempool *tx_pool;
	struct rte_mbuf *mb;
	char pool_name[64];
	char *p;
	int ret, dns_len, total_len;
	struct rte_mbuf **pkts;
	struct ipv4_hdr *iphdr;

	ring = reqest_ring_per_socket[rte_socket_id()];
	if (ring == NULL) {
		printf("ring\n");
		return;
	}

	snprintf(pool_name, "tx_pool%u", rte_lcore_id());
	tx_pool = rte_pktmbuf_pool_create(pool_name, NB_TX_POOL, 32, 0, 
		RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (tx_pool == NULL) {
		printf("%s %d\n");
	}

	while (1) {
		mtable.length = 0;		
		pkts = mtable.mb;
		while (count != NB_BURST && (rte_ring_dequeue(ring, (void**)&req) == 0)) {
			if (rte_mempool_get(tx_pool, &mb) < 0)
				break;
			total_len = 0;
			p = rte_pktmbuf_adj(mb, sizeof(struct ether_addr) + sizeof(struct ipv4_hdr) + sizeof(struct udp_hdr));
			dns_len = dns_request_construct((struct dns_hdr *)p, 
				req->req_domain, req->req_type, DNS_CLASS_IN, 0, NULL);
			if (dns_len == 0) {
				rte_mempool_put(tx_pool, mb);
				break;
			}
			total_len += dns_len;
			p = rte_pktmbuf_prepend(mb, sizeof(struct udp_hdr) + sizeof(struct ipv4_hdr) + sizeof(struct ether_addr));
			ret = ether_construct((struct ether_hdr *)p, 
				&default_proto_param.dst_mac, &default_proto_param.src_mac, ETHER_TYPE_IPv4);
			total_len += ret;
			iphdr = (struct ipv4_hdr*)rte_pktmbuf_adj(mb, sizeof(struct ether_hdr));
			ret = ipv4_construct(iphdr, 
				default_proto_param.dst_addr, default_proto_param.src_addr, rte_rand() & 0xffff, 
				IPPROTO_UDP, dns_len + sizeof(struct udp_hdr) + sizeof(struct ipv4_hdr), 1);
			total_len += ret;
			p = (struct ipv4_hdr*)rte_pktmbuf_adj(mb, sizeof(struct ipv4_hdr));
			ret = udp_construct((struct udp_hdr *)p, 
				default_proto_param.dport, default_proto_param.sport, dns_len + sizeof(struct udp_hdr), iphdr);
			total_len += ret;

			rte_pktmbuf_prepend(mb, sizeof(struct ipv4_hdr) + sizeof(struct ether_hdr));
			mb->pkt_len = total_len;
			mb->data_len = total_len;
			mb->ol_flags |= PKT_TX_IP_CKSUM;

			mtable.mb[mtable.length++] = mb;
		}
		
		while (mtable.length) {
			ret = rte_eth_tx_burst(uint8_t port_id,uint16_t queue_id, pkts, mtable.length));
			pkts += ret;
			mtable.length -= ret;
		}
	}
}

static int packet_launch_one_lcore(__rte_unused void* unused)
{
    int i;
    struct txrx_queue *rxq;
    struct lcore_queue_conf *lcore_q;

	switch(rte_lcore_id()) {
	case 2: packet_construct_task(); break;
	case 4: packet_send_task(); break;
	default: nothing_to_do; break;
	}

    return 0;
}

static int format_domain(char* orig, char* out, int size) 
{
	char *pch;
	uint8_t length;
	int total;

	total = 0;
	if (orig && out) {
		pch = strtok(orig, ".");
		while (pch) {
			length = strlen(pch);
			if (total + length + 1 >= size) {
				printf("%s %d\n",  __func__, __LINE__);
				return 0;
			}
				
			out[total] = length; total += 1;
			memcpy(out + total, pch, length); total += length;
			pch = strtok(NULL, ".");
		}		
	}
	out[total] = '\0';
	return total;
}

static int config_port(uint32_t port)
{
	unsigned lcore;
	int ret;
	uint16_t tx_queue_id;

	ret = rte_eth_dev_configure(port, 1, nb_tx_queue, &default_port_conf);
	if (ret < 0)
		return ret;

	tx_queue_id = 0;
	for (lcore = 0; lcore < RTE_MAX_LCORE && tx_queue_id < nb_tx_queue; lcore++, tx_queue_id) {
		ret = rte_eth_tx_queue_setup(port, tx_queue_id, nb_tx_desc, rte_eth_dev_socket_id(port), NULL);
		if (ret < 0)
			return ret;
	}
	ret = rte_eth_rx_queue_setup(port, 0, 32, rte_eth_dev_socket_id(port), NULL, rxpool);
	if (ret < 0) 
		return ret;

	ret = rte_eth_dev_start(port);
	if (ret < 0)
		return ret;

	return 0;	
}

static int parse_cmdline(int argc, char** argv)
{
	char c, *pch, *pch2, buf[512], filename[256];
	long n;
	

	while ((c = getopt(argc, argv, "f:")) != EOF) {
		switch(c) {
		default:
			printf("unknown option %c\n", c);
			break;
		case 'f':
			{
				pch = strtok(optarg, ",");
				while (pch) {
					strncpy(buf, pch, 512);
					pch2 = strtok(buf, ":");
					if (pch2) {
						strncpy(filename, pch2, 256);
						pch2 = strtok(NULL, ":");
						if (pch2 == NULL) {
							if (request_file_share) {
								printf("conflict filename\n");
								return -1;
							} else 
								request_file_share = strdup(filename);							
						} else {
							n = strtoul(pch2, NULL, 10);
							if (n >= RTE_MAX_LCORE) {
								printf("invalid core %l\n", n);
								return -1;
							}
							if (request_file[n]) {
								printf("conflict filename\n");
								return -1;
							} else 				
								request_file[n] = strdup(filename); 
						}					
					}
					
					pch = strtok(NULL, ",");
				}
			}
			break;
		}
	}
	return 0;
}

static int request_ring_init(void)
{
	int i;
	char ring_name[64];

	for (i = 0; i < RTE_MAX_NUMA_NODES; i++) {
		if (reqest_ring_per_socket[i] == NULL) {
			snprintf(ring_name, "req_ring%d", i);
			reqest_ring_per_socket[i] = 
				rte_ring_create(ring_name, NB_ELEM_PER_RING, i, 0);
		}
	}
	return 0;	
}

int main(int argc, char** argv)
{
	uint8_t port, nb_ports;
	int ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return ret;
	argc -= ret;
	argv += ret;

	ret = parse_cmdline(argc, argv);
	if (ret < 0) {
		return -1;
	}	

	ret = request_ring_init();
	if (ret < 0)
		return -1;
	
	rxpool = rte_pktmbuf_pool_create("rx_pool", 128, 32, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	if (rxpool == NULL) {
		printf("create rxpool error\n");
		return -1;
	}

	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0) {
		printf("no available port\n");
		return -1;
	}
	for (port = 0; port < nb_ports; port++) {
		ret = config_port(port);
		if (ret != 0) {
			printf("port %u config error\n", port);
			return -1;
		}			
	}
	
    rte_eal_mp_remote_launch(packet_launch_one_lcore, NULL, SKIP_MASTER);
    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
        if (rte_eal_wait_lcore(lcore_id) < 0)
            return -1;
    }

    return 0;
}
