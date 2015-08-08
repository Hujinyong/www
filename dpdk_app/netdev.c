#include <arpa/inet.h>

#include <sys/queue.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ethdev.h>

#include "af_inet.h"
#include "netdev.h"

struct net_device dev_array[RTE_MAX_ETHPORTS];

#define DEFAULT_NDEV_NAME "eth%u"

static int retrive_addr(char *pstr, __out be32 *v4addr, __out uint8_t *mask)
{
	int ret;
	char *p, *q;

	if (pstr == NULL || v4addr == NULL || mask == NULL)
		return -1;

	if (strnlen(pstr, 18) == 0)
		return -1;

	p = strtok(pstr, "/");
	if (p) {
		q = p;
	} else {
		return -1;
	}
	p = strtok(NULL, "/");
	if (p) {
		*mask = strtol(p, NULL, 10);
		*v4addr = inet_aton(q, NULL);
	} else {
		return -1;
	}
	return 0;
}

static int net_dev_add_v4addr(struct net_device *dev, struct iovec *pstr)
{
	be32 v4addr;
	uint8_t mask;
	struct ip_addr *addr, *p;

	if (dev == NULL || pstr == NULL)
		return -1;

	if (retrive_addr(pstr->iov_base, &v4addr, &mask)) {
		RTE_LOG(WARNING, NET, "%s %d Invalid address format\n"
			, __func__, __LINE__);
		return -1;
	}

	if (!IS_VALID_UC_IPV4(dev, v4addr, mask)) {
		return -1;
	}
	
	for (p = dev->v4_addr; p; p = p->next) {
		if (p->addr.ipv4 == v4addr) {
			return -1;
		}			
	}

	addr = rte_malloc(NULL, sizeof *addr, 0);
	if (addr == NULL)
		return -1;

	addr->addr.ipv4 = v4addr;
	addr->mask = mask;
	addr->next = dev->v4_addr;
	dev->v4_addr = addr;

	return 0;	
}

static int net_dev_add_v4addrs(struct net_device *dev, struct iovec *ppstr, int count) 
{
	int i, ret;

	for (i == 0; i < count; i++) {
		ret = dev->ops->add_v4addr(dev, &ppstr[i]);
		if (ret) 
			goto error;
	}

	return ret;
error:
	if (dev->ops->del_v4addrs) {
		dev->ops->del_v4addrs(dev, ppstr, i);
	} else if (dev->ops->del_v4addr) {
		for (i; i > 0; i--) {
			dev->ops->del_v4addr(dev, &ppstr[i - 1]);
		}
	}
	return -1;
}

static int net_device_set_name(struct net_device *dev, struct iovec *iov)
{
	int ret;

	if (dev == NULL || iov == NULL || iov->iov_len == 0 || iov->iov_base == NULL)
		return -1;

	if (dev->dev == NULL)
		return -1;

	strncpy(dev->dev->data->name, iov->iov_base, iov->iov_len);
	return 0;
}

static int net_device_get_name(struct net_device *dev, struct iovec *iov)
{
	if (dev == NULL || iov == NULL || iov->iov_base == NULL)
		return -1;

	if (dev->dev == NULL)
		return -1;

	strncpy(iov->iov_base, dev->dev->data->name, strlen(dev->dev->data->name));
	iov->iov_len = strlen(iov->iov_base) + 1;
	return 0;
}

struct net_device_ops default_ndev_ops = {
	net_dev_add_v4addr, /*add_v4addr*/
	net_dev_add_v4addrs, /*add_v4addrs*/
	NULL, /*del_v4addr*/
	NULL, /*del_v4addrs*/
	NULL, /*get_primary_v4addr*/
	NULL, /*get_v4addrs*/
	NULL, /*set_mode*/
	NULL, /*enable*/
	NULL, /*start*/
	NULL, /*stop*/
	net_device_set_name,/*setname*/
	net_device_get_name,/*getname*/
};

/**
*
*/
struct net_device* net_device_alloc(unsigned portid,
	char *name, struct net_device_ops *ops)
{
	struct net_device *dev;
	struct rte_eth_dev *eth_dev;
	size_t name_len;
	
	if (!rte_eth_dev_is_valid_port(portid)) {
		RTE_LOG(WARNING, NET, "%s %d invalid portid %u !\n", 
			__func__, __LINE__, portid);
		return NULL;
	}

	dev = &dev_array[portid];
	if (dev->dev != NULL) {
		/*TODO*/
		RTE_LOG(WARNING, NET, "%s %d portid %u is used!\n"
			, __func__, __LINE__, portid);
		return NULL;
	}

	if (name == NULL)
		name = DEFAULT_NDEV_NAME;

	name_len = strnlen(name, MAX_NIC_NAME_SIZE);
	if (name_len == 0 || name_len == MAX_NIC_NAME_SIZE) {
		RTE_LOG(WARNING, NET, "%s %d invalid device name!\n", __func__, __LINE__);
		return NULL;
	}

	if (ops == NULL)
		ops = &default_ndev_ops;

	dev->dev = eth_dev;
	dev->ops = ops;
	dev->portid = portid;
	dev->flag = NET_DEV_F_ENABLE;
	strncpy(dev->name, name, name_len);
	dev->v4_addr = NULL;

	return dev;	
}


/**
*
*/
void net_device_release(unsigned portid)
{

}


/**
*
*/
int net_dev_ctrl(struct net_device *dev, int ctrl_type, struct msg_hdr *param)
{
	int retval = -1;
	
	if (dev == NULL)
		return retval;
	
	switch (ctrl_type) {
	
	
	case NDEV_CTRL_T_INET_ADDR:	/*set ip address*/
		if (param == NULL)
			return -1;
		
		if (param->ctlhdr.type == INET_ADD_V4ADDR) {		/*add addresses*/
			if (dev->ops->add_v4addrs) {
				retval = dev->ops->add_v4addrs(dev, param->iov, param->iov_length);
			}
		} else if (param->ctlhdr.type == INET_DEL_V4ADDR) {	/*del addresses*/
			if (dev->ops->del_v4addrs) {
				retval = dev->ops->del_v4addrs(dev, param->iov, param->iov_length);
			}
		} else {
			
		}		
		break;

	case NDEV_CTRL_T_DEV_NAME:	/*set device name*/
		if (param == NULL)
			return -1;

		if (param->ctlhdr.type == NAME_SET) {			/*set device name*/
			retval = dev->ops->set_name(dev, param->iov);
		} else if (param->ctlhdr.type == NAME_GET) {	/*get device name*/
			retval = dev->ops->get_name(dev, param->iov);
			if (!retval) {
				param->iov_length = 1;
			}
		}		
		break;

	default:
		break;	
	}
	return retval;
}

int net_device_init(const unsigned *portid, int length)
{
	struct net_device *retval;
    int i;
    struct msg_hdr mhdr;

	if (portid == NULL || length == 0)
		return -1;

    memset(&mhdr, 0, sizeof mhdr);
    mhdr.iov = msg_hdr_alloc_iovs(1, 64);
    if (mhdr.iov == NULL)
        return -1;
    mhdr.iov_length = 1;
    mhdr.ctlhdr.type = NAME_GET;

	for (i = 0; i < length; i++) {
		retval = net_device_alloc(portid[i], NULL, NULL);
		if (retval == NULL) {
            rte_free(mhdr.iov);
			goto error_release_ndev;
        }
        if (net_dev_ctrl(retval, NDEV_CTRL_T_DEV_NAME, &mhdr) == 0) {
            RTE_LOG(DEBUG, NET, "init nic %s ok\n", (char*)mhdr.iov[0].iov_base);
        }
	}

    rte_free(mhdr.iov);
	return 0;
error_release_ndev:
	for (i; i > 0; i--) {
		net_device_release(portid[i - 1]);
	}
	return -1;
}
