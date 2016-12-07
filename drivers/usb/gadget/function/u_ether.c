/*
 * u_ether.c -- Ethernet-over-USB link layer utilities for Gadget stack
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/msm_rmnet.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/notifier.h>
#include <linux/cpufreq.h>
#include "u_ether.h"



#define UETH__VERSION	"29-May-2008"

static struct workqueue_struct	*uether_wq;
static struct workqueue_struct	*uether_tx_wq;

static int tx_start_threshold = 1500;
module_param(tx_start_threshold, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tx_start_threshold,
	"Threashold to start stopped network queue");

static int tx_stop_threshold = 2000;
module_param(tx_stop_threshold, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(tx_stop_threshold,
	"Threashold to stop network queue");

static unsigned int min_cpu_freq;
module_param(min_cpu_freq, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(min_cpu_freq,
	"to set minimum cpu frquency to when ethernet ifc is active");

#define DL_MAX_PKTS_PER_XFER	20

#define EXTRA_ALLOCATION_SIZE_U_ETH	128

enum ifc_state {
	ETH_UNDEFINED,
	ETH_STOP,
	ETH_START,
};

struct eth_dev {
	spinlock_t		lock;
	struct gether		*port_usb;

	struct net_device	*net;
	struct usb_gadget	*gadget;

	spinlock_t		req_lock;	
	struct list_head	tx_reqs, rx_reqs;
	unsigned		tx_qlen;
#define MAX_TX_REQ_WITH_NO_INT	5
	int			no_tx_req_used;
	int			tx_skb_hold_count;
	u32			tx_req_bufsize;
	struct sk_buff_head	tx_skb_q;

	struct sk_buff_head	rx_frames;

	unsigned		qmult;

	unsigned		header_len;
	unsigned int		ul_max_pkts_per_xfer;
	unsigned int		dl_max_pkts_per_xfer;
	uint32_t		dl_max_xfer_size;
	bool			rx_trigger_enabled;
	struct sk_buff		*(*wrap)(struct gether *, struct sk_buff *skb);
	int			(*unwrap)(struct gether *,
						struct sk_buff *skb,
						struct sk_buff_head *list);

	struct work_struct	work;
	struct work_struct	rx_work;
	struct work_struct	tx_work;

	unsigned long		todo;
	unsigned long		flags;
	unsigned short		rx_needed_headroom;
#define	WORK_RX_MEMORY		0

	bool			zlp;
	u8			host_mac[ETH_ALEN];
	u8			dev_mac[ETH_ALEN];

	int			miMaxMtu;

	
	unsigned long		tx_throttle;
	unsigned long		rx_throttle;
	unsigned int		tx_aggr_cnt[DL_MAX_PKTS_PER_XFER];
	unsigned int		tx_pkts_rcvd;
	unsigned int		tx_bytes_rcvd;
	unsigned int		loop_brk_cnt;
	unsigned long		skb_expand_cnt;
	struct dentry		*uether_dent;

	enum ifc_state		state;
	struct notifier_block	cpufreq_notifier;
	struct work_struct	cpu_policy_w;

	bool			sg_enabled;
};

struct sg_ctx {
	struct sk_buff_head	skbs;
};


static void uether_debugfs_init(struct eth_dev *dev, const char *n);
static void uether_debugfs_exit(struct eth_dev *dev);



#define RX_EXTRA	20	

#define DEFAULT_QLEN	2	

static unsigned tx_qmult = 2;
module_param(tx_qmult, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(tx_qmult, "Additional queue length multiplier for tx");

static inline int qlen(struct usb_gadget *gadget, unsigned qmult)
{
	if (gadget_is_dualspeed(gadget) && (gadget->speed == USB_SPEED_HIGH ||
					    gadget->speed == USB_SPEED_SUPER))
		return qmult * DEFAULT_QLEN;
	else
		return DEFAULT_QLEN;
}

#define U_ETHER_RX_PENDING_TSHOLD 500

static unsigned int u_ether_rx_pending_thld = U_ETHER_RX_PENDING_TSHOLD;
module_param(u_ether_rx_pending_thld, uint, S_IRUGO | S_IWUSR);



#undef DBG
#undef VDBG
#undef ERROR
#undef INFO

#define xprintk(d, level, fmt, args...) \
	printk(level "%s: " fmt , (d)->net->name , ## args)

#ifdef DEBUG
#undef DEBUG
#define DBG(dev, fmt, args...) \
	xprintk(dev , KERN_DEBUG , fmt , ## args)
#else
#define DBG(dev, fmt, args...) \
	do { } while (0)
#endif 

#ifdef VERBOSE_DEBUG
#define VDBG	DBG
#else
#define VDBG(dev, fmt, args...) \
	do { } while (0)
#endif 

#define ERROR(dev, fmt, args...) \
	xprintk(dev , KERN_ERR , fmt , ## args)
#define INFO(dev, fmt, args...) \
	xprintk(dev , KERN_INFO , fmt , ## args)

static struct eth_dev *the_dev;



static int ueth_change_mtu(struct net_device *net, int new_mtu)
{
	struct eth_dev	*dev = netdev_priv(net);
	unsigned long	flags;
	int		status = 0;
	int 		iMaxMtuSet = ETH_FRAME_LEN;

	if (the_dev) {
		if (the_dev->miMaxMtu == ETH_FRAME_LEN_MAX - ETH_HLEN)
			iMaxMtuSet = ETH_FRAME_LEN_MAX - ETH_HLEN;
	}
	
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb)
		status = -EBUSY;
	else if (new_mtu <= ETH_HLEN || new_mtu > iMaxMtuSet)
		status = -ERANGE;
	else
		net->mtu = new_mtu;
	spin_unlock_irqrestore(&dev->lock, flags);

	return status;
}

static int ueth_change_mtu_ip(struct net_device *net, int new_mtu)
{
	struct eth_dev	*dev = netdev_priv(net);
	unsigned long	flags;
	int		status = 0;

	spin_lock_irqsave(&dev->lock, flags);
	if (new_mtu <= 0)
		status = -EINVAL;
	else
		net->mtu = new_mtu;

	DBG(dev, "[%s] MTU change: old=%d new=%d\n", net->name,
					net->mtu, new_mtu);
	spin_unlock_irqrestore(&dev->lock, flags);

	return status;
}

static void eth_get_drvinfo(struct net_device *net, struct ethtool_drvinfo *p)
{
	struct eth_dev *dev = netdev_priv(net);

	strlcpy(p->driver, "g_ether", sizeof(p->driver));
	strlcpy(p->version, UETH__VERSION, sizeof(p->version));
	strlcpy(p->fw_version, dev->gadget->name, sizeof(p->fw_version));
	strlcpy(p->bus_info, dev_name(&dev->gadget->dev), sizeof(p->bus_info));
}


static const struct ethtool_ops ops = {
	.get_drvinfo = eth_get_drvinfo,
	.get_link = ethtool_op_get_link,
};

static void defer_kevent(struct eth_dev *dev, int flag)
{
	if (test_and_set_bit(flag, &dev->todo))
		return;
	if (!schedule_work(&dev->work))
		ERROR(dev, "kevent %d may have been dropped\n", flag);
	else
		DBG(dev, "kevent %d scheduled\n", flag);
}

static void rx_complete(struct usb_ep *ep, struct usb_request *req);
static void tx_complete(struct usb_ep *ep, struct usb_request *req);

static int
rx_submit(struct eth_dev *dev, struct usb_request *req, gfp_t gfp_flags)
{
	struct sk_buff	*skb;
	int		retval = -ENOMEM;
	size_t		size = 0;
	struct usb_ep	*out;
	unsigned long	flags;
	unsigned short reserve_headroom = 0;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb)
		out = dev->port_usb->out_ep;
	else
		out = NULL;

	if (!out) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return -ENOTCONN;
	}


	size += sizeof(struct ethhdr) + dev->net->mtu + RX_EXTRA;
	size += dev->port_usb->header_len;
	size += out->maxpacket - 1;
	size -= size % out->maxpacket;

	if (dev->ul_max_pkts_per_xfer)
		size *= dev->ul_max_pkts_per_xfer;

	if (dev->port_usb->is_fixed)
		size = max_t(size_t, size, dev->port_usb->fixed_out_len);
	spin_unlock_irqrestore(&dev->lock, flags);

	if (dev->rx_needed_headroom)
		reserve_headroom = ALIGN(dev->rx_needed_headroom, 4);

	pr_debug("%s: size: %zu + %d(hr)", __func__, size, reserve_headroom);

	skb = alloc_skb(size + reserve_headroom, gfp_flags);
	if (skb == NULL) {
		DBG(dev, "no rx skb\n");
		goto enomem;
	}

	skb_reserve(skb, reserve_headroom);

	req->buf = skb->data;
	req->length = size;
	req->context = skb;

	retval = usb_ep_queue(out, req, gfp_flags);
	if (retval == -ENOMEM)
enomem:
		defer_kevent(dev, WORK_RX_MEMORY);
	if (retval) {
		DBG(dev, "rx submit --> %d\n", retval);
		if (skb)
			dev_kfree_skb_any(skb);
	}
	return retval;
}

static void rx_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct sk_buff	*skb = req->context;
	struct eth_dev	*dev = ep->driver_data;
	int		status = req->status;
	bool		queue = 0;

	switch (status) {

	
	case 0:
		skb_put(skb, req->actual);
		if (dev->unwrap) {
			unsigned long	flags;

			spin_lock_irqsave(&dev->lock, flags);
			if (dev->port_usb) {
				status = dev->unwrap(dev->port_usb,
							skb,
							&dev->rx_frames);
				if (status == -EINVAL)
					dev->net->stats.rx_errors++;
				else if (status == -EOVERFLOW)
					dev->net->stats.rx_over_errors++;
			} else {
				dev_kfree_skb_any(skb);
				status = -ENOTCONN;
			}
			spin_unlock_irqrestore(&dev->lock, flags);
		} else {
			skb_queue_tail(&dev->rx_frames, skb);
		}

		if (!status)
			queue = 1;
		break;

	
	case -ECONNRESET:		
	case -ESHUTDOWN:		
		VDBG(dev, "rx shutdown, code %d\n", status);
		goto quiesce;

	
	case -ECONNABORTED:		
		DBG(dev, "rx %s reset\n", ep->name);
		defer_kevent(dev, WORK_RX_MEMORY);
quiesce:
		if (skb)
			dev_kfree_skb_any(skb);
		goto clean;

	
	case -EOVERFLOW:
		dev->net->stats.rx_over_errors++;
		

	default:
		queue = 1;
		dev_kfree_skb_any(skb);
		dev->net->stats.rx_errors++;
		DBG(dev, "rx status %d\n", status);
		break;
	}

clean:
	if (queue && dev->rx_frames.qlen <= u_ether_rx_pending_thld) {
		if (rx_submit(dev, req, GFP_ATOMIC) < 0) {
			spin_lock(&dev->req_lock);
			list_add(&req->list, &dev->rx_reqs);
			spin_unlock(&dev->req_lock);
		}
	} else {
		
		if (queue)
			dev->rx_throttle++;
		spin_lock(&dev->req_lock);
		list_add(&req->list, &dev->rx_reqs);
		spin_unlock(&dev->req_lock);
	}

	if (queue)
		queue_work(uether_wq, &dev->rx_work);
}

static int prealloc(struct list_head *list,
		struct usb_ep *ep, unsigned n,
		bool sg_supported, int hlen)
{
	unsigned		i;
	struct usb_request	*req;
	bool			usb_in;
	struct sg_ctx		*sg_ctx;

	if (!n)
		return -ENOMEM;

	
	i = n;
	list_for_each_entry(req, list, list) {
		if (i-- == 0)
			goto extra;
	}

	if (ep->desc->bEndpointAddress & USB_DIR_IN)
		usb_in = true;
	else
		usb_in = false;

	while (i--) {
		req = usb_ep_alloc_request(ep, GFP_ATOMIC);
		if (!req)
			return list_empty(list) ? -ENOMEM : 0;
		
		if (usb_in) {
			req->complete = tx_complete;
			if (!sg_supported)
				goto add_list;
			req->sg = kmalloc(
					DL_MAX_PKTS_PER_XFER *
						sizeof(struct scatterlist),
					GFP_ATOMIC);
			if (!req->sg)
				goto extra;
			sg_ctx = kmalloc(sizeof(*sg_ctx), GFP_ATOMIC);
			if (!sg_ctx)
				goto extra;
			req->context = sg_ctx;
			req->buf = kzalloc(DL_MAX_PKTS_PER_XFER * hlen,
						GFP_ATOMIC);
		} else {
			req->complete = rx_complete;
		}
add_list:
		list_add(&req->list, list);
	}
	return 0;

extra:
	
	for (;;) {
		struct list_head	*next;

		next = req->list.next;
		list_del(&req->list);

		if (sg_supported) {
			kfree(req->sg);
			kfree(req->context);
			kfree(req->buf);
		}

		usb_ep_free_request(ep, req);

		if (next == list)
			break;

		req = container_of(next, struct usb_request, list);
	}
	return 0;
}

static int alloc_requests(struct eth_dev *dev, struct gether *link, unsigned n)
{
	int	status;

	spin_lock(&dev->req_lock);
	status = prealloc(&dev->tx_reqs, link->in_ep, n * tx_qmult,
				dev->sg_enabled,
				dev->header_len);
	if (status < 0)
		goto fail;
	status = prealloc(&dev->rx_reqs, link->out_ep, n,
				dev->sg_enabled,
				dev->header_len);
	if (status < 0)
		goto fail;
	goto done;
fail:
	DBG(dev, "can't alloc requests\n");
done:
	spin_unlock(&dev->req_lock);
	return status;
}

static void rx_fill(struct eth_dev *dev, gfp_t gfp_flags)
{
	struct usb_request	*req;
	unsigned long		flags;
	int			req_cnt = 0;

	
	spin_lock_irqsave(&dev->req_lock, flags);
	while (!list_empty(&dev->rx_reqs)) {
		
		if (++req_cnt > qlen(dev->gadget, dev->qmult))
			break;

		req = container_of(dev->rx_reqs.next,
				struct usb_request, list);
		list_del_init(&req->list);
		spin_unlock_irqrestore(&dev->req_lock, flags);

		if (rx_submit(dev, req, gfp_flags) < 0) {
			spin_lock_irqsave(&dev->req_lock, flags);
			list_add(&req->list, &dev->rx_reqs);
			spin_unlock_irqrestore(&dev->req_lock, flags);
			defer_kevent(dev, WORK_RX_MEMORY);
			return;
		}

		spin_lock_irqsave(&dev->req_lock, flags);
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);
}

static __be16 ether_ip_type_trans(struct sk_buff *skb,
	struct net_device *dev)
{
	__be16	protocol = 0;

	skb->dev = dev;

	switch (skb->data[0] & 0xf0) {
	case 0x40:
		protocol = htons(ETH_P_IP);
		break;
	case 0x60:
		protocol = htons(ETH_P_IPV6);
		break;
	default:
		if ((skb->data[0] & 0x40) == 0x00)
			protocol = htons(ETH_P_MAP);
		else
			pr_debug_ratelimited("[%s] L3 protocol decode error: 0x%02x",
					dev->name, skb->data[0] & 0xf0);
	}

	return protocol;
}

static void process_rx_w(struct work_struct *work)
{
	struct eth_dev	*dev = container_of(work, struct eth_dev, rx_work);
	struct sk_buff	*skb;
	int		status = 0;
	unsigned int	uiCurMtu = 0;

	if (!dev->port_usb)
		return;

	set_wake_up_idle(true);
	uiCurMtu = dev->net->mtu + ETH_HLEN;
	if ((uiCurMtu <= ETH_HLEN) || (uiCurMtu > ETH_FRAME_LEN_MAX))
		uiCurMtu = ETH_FRAME_LEN;
	while ((skb = skb_dequeue(&dev->rx_frames))) {
		if (status < 0
				|| ETH_HLEN > skb->len
				|| (skb->len > uiCurMtu &&
				test_bit(RMNET_MODE_LLP_ETH, &dev->flags))) {
			dev->net->stats.rx_errors++;
			dev->net->stats.rx_length_errors++;
			DBG(dev, "rx length %d\n", skb->len);
			dev_kfree_skb_any(skb);
			continue;
		}
		if (test_bit(RMNET_MODE_LLP_IP, &dev->flags))
			skb->protocol = ether_ip_type_trans(skb, dev->net);
		else
			skb->protocol = eth_type_trans(skb, dev->net);

		dev->net->stats.rx_packets++;
		dev->net->stats.rx_bytes += skb->len;

		status = netif_rx_ni(skb);
	}
	set_wake_up_idle(false);

	if (netif_running(dev->net))
		rx_fill(dev, GFP_KERNEL);
}

static void eth_work(struct work_struct *work)
{
	struct eth_dev	*dev = container_of(work, struct eth_dev, work);

	if (test_and_clear_bit(WORK_RX_MEMORY, &dev->todo)) {
		if (netif_running(dev->net))
			rx_fill(dev, GFP_KERNEL);
	}

	if (dev->todo)
		DBG(dev, "work done, flags = 0x%lx\n", dev->todo);
}

static void tx_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct sk_buff	*skb;
	struct eth_dev	*dev;
	struct net_device *net;
	struct usb_request *new_req;
	struct usb_ep *in;
	int n = 1;
	int length;
	int retval;

	if (!ep->driver_data) {
		usb_ep_free_request(ep, req);
		return;
	}

	dev = ep->driver_data;
	net = dev->net;

	if (!dev->port_usb) {
		usb_ep_free_request(ep, req);
		return;
	}

	switch (req->status) {
	default:
		dev->net->stats.tx_errors++;
		VDBG(dev, "tx err %d\n", req->status);
		
	case -ECONNRESET:		
	case -ESHUTDOWN:		
		break;
	case 0:
		if (req->num_sgs)
			req->actual -= (req->num_sgs/2) * dev->header_len;

		if (!req->zero)
			dev->net->stats.tx_bytes += req->actual-1;
		else
			dev->net->stats.tx_bytes += req->actual;
	}

	if (req->num_sgs) {
		struct sg_ctx *sg_ctx = req->context;

		n = skb_queue_len(&sg_ctx->skbs);
		dev->tx_aggr_cnt[n-1]++;

		
		__skb_queue_purge(&sg_ctx->skbs);
	}

	dev->net->stats.tx_packets += n;

	spin_lock(&dev->req_lock);

	if (req->num_sgs) {
		if (!req->status)
			queue_work(uether_tx_wq, &dev->tx_work);

		list_add_tail(&req->list, &dev->tx_reqs);
		spin_unlock(&dev->req_lock);
		return;
	}

	if (dev->port_usb->multi_pkt_xfer && !req->context) {
		dev->no_tx_req_used--;
		req->length = 0;
		in = dev->port_usb->in_ep;

		
		if (!req->no_interrupt && !list_empty(&dev->tx_reqs)) {
			new_req = container_of(dev->tx_reqs.next,
					struct usb_request, list);
			list_del(&new_req->list);
			spin_unlock(&dev->req_lock);
			if (new_req->length > 0) {
				length = new_req->length;

				if (dev->port_usb->is_fixed &&
					length == dev->port_usb->fixed_in_len &&
					(length % in->maxpacket) == 0)
					new_req->zero = 0;
				else
					new_req->zero = 1;

				if (new_req->zero && !dev->zlp &&
						(length % in->maxpacket) == 0) {
					new_req->zero = 0;
					length++;
				}

				
				spin_lock(&dev->req_lock);
				dev->tx_qlen++;
				if (dev->tx_qlen == MAX_TX_REQ_WITH_NO_INT) {
					new_req->no_interrupt = 0;
					dev->tx_qlen = 0;
				} else {
					new_req->no_interrupt = 1;
				}
				spin_unlock(&dev->req_lock);
				new_req->length = length;
				new_req->complete = tx_complete;
				retval = usb_ep_queue(in, new_req, GFP_ATOMIC);
				switch (retval) {
				default:
					DBG(dev, "tx queue err %d\n", retval);
					new_req->length = 0;
					spin_lock(&dev->req_lock);
					list_add_tail(&new_req->list,
							&dev->tx_reqs);
					spin_unlock(&dev->req_lock);
					break;
				case 0:
					spin_lock(&dev->req_lock);
					dev->no_tx_req_used++;
					spin_unlock(&dev->req_lock);
					net->trans_start = jiffies;
				}
			} else {
				spin_lock(&dev->req_lock);
				list_add_tail(&new_req->list, &dev->tx_reqs);
				spin_unlock(&dev->req_lock);
			}
		} else {
			spin_unlock(&dev->req_lock);
		}
	} else {
		skb = req->context;
		
		if (dev->port_usb->multi_pkt_xfer && dev->tx_req_bufsize) {
			req->buf = kzalloc(dev->tx_req_bufsize
				+ dev->gadget->extra_buf_alloc, GFP_ATOMIC);
			req->context = NULL;
		} else {
			req->buf = NULL;
		}

		spin_unlock(&dev->req_lock);
		dev_kfree_skb_any(skb);
	}

	
	spin_lock(&dev->req_lock);
	list_add_tail(&req->list, &dev->tx_reqs);
	spin_unlock(&dev->req_lock);

	if (netif_carrier_ok(dev->net))
		netif_wake_queue(dev->net);
}

static inline int is_promisc(u16 cdc_filter)
{
	return cdc_filter & USB_CDC_PACKET_TYPE_PROMISCUOUS;
}

static int alloc_tx_buffer(struct eth_dev *dev)
{
	struct list_head	*act;
	struct usb_request	*req;

	dev->tx_req_bufsize = (dev->dl_max_pkts_per_xfer *
				(dev->net->mtu
				+ sizeof(struct ethhdr)
				
				+ 44
				+ 22));

	list_for_each(act, &dev->tx_reqs) {
		req = container_of(act, struct usb_request, list);
		if (!req->buf) {
			req->buf = kzalloc(dev->tx_req_bufsize
				+ dev->gadget->extra_buf_alloc, GFP_ATOMIC);
			if (!req->buf)
				goto free_buf;
		}
		
		req->context = NULL;
	}
	return 0;

free_buf:
	
	dev->tx_req_bufsize = 0;
	list_for_each(act, &dev->tx_reqs) {
		req = container_of(act, struct usb_request, list);
		kfree(req->buf);
		req->buf = NULL;
	}
	return -ENOMEM;
}

static void process_tx_w(struct work_struct *w)
{
	struct eth_dev		*dev = container_of(w, struct eth_dev, tx_work);
	struct net_device	*net = NULL;
	struct sk_buff		*skb = NULL;
	struct sg_ctx		*sg_ctx;
	struct usb_request	*req;
	struct usb_ep		*in = NULL;
	int			ret, count, hlen = 0, hdr_offset;
	uint32_t		max_size = 0;
	uint32_t		max_num_pkts = 1;
	unsigned long		flags;
	bool			header_on = false;
	int			req_cnt = 0;
	bool			port_usb_active;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb) {
		in = dev->port_usb->in_ep;
		max_size = dev->dl_max_xfer_size;
		max_num_pkts = dev->dl_max_pkts_per_xfer;
		if (!max_num_pkts)
			max_num_pkts = 1;
		hlen = dev->header_len;
		net = dev->net;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	spin_lock_irqsave(&dev->req_lock, flags);
	while (in && !list_empty(&dev->tx_reqs) &&
			(skb = skb_dequeue(&dev->tx_skb_q))) {
		req = list_first_entry(&dev->tx_reqs, struct usb_request,
				list);
		list_del(&req->list);
		spin_unlock_irqrestore(&dev->req_lock, flags);

		req->num_sgs = 0;
		req->zero = 1;
		req->length = 0;
		sg_ctx = req->context;
		skb_queue_head_init(&sg_ctx->skbs);
		sg_init_table(req->sg, DL_MAX_PKTS_PER_XFER);

		hdr_offset = 0;
		count = 1;
		do {
			spin_lock_irqsave(&dev->lock, flags);
			if (!dev->port_usb) {
				spin_unlock_irqrestore(&dev->lock, flags);
				skb_queue_purge(&sg_ctx->skbs);
				kfree(req->sg);
				kfree(req->context);
				kfree(req->buf);
				usb_ep_free_request(in, req);

				return;
			}

			if (hlen && dev->wrap) {
				dev->port_usb->header = req->buf + hdr_offset;
				skb = dev->wrap(dev->port_usb, skb);
				header_on = true;
			}
			spin_unlock_irqrestore(&dev->lock, flags);

			if (header_on) {
				sg_set_buf(&req->sg[req->num_sgs],
					req->buf + hdr_offset, hlen);
				req->num_sgs++;
				hdr_offset += hlen;
				req->length += hlen;
			}

			
			sg_set_buf(&req->sg[req->num_sgs], skb->data, skb->len);
			req->num_sgs++;

			req->length += skb->len;
			skb_queue_tail(&sg_ctx->skbs, skb);

			skb = skb_dequeue(&dev->tx_skb_q);
			if (!skb)
				break;
			if ((req->length + skb->len + hlen) >= max_size ||
					count >= max_num_pkts) {
				skb_queue_head(&dev->tx_skb_q, skb);
				break;
			}
			count++;
		} while (true);

		sg_mark_end(&req->sg[req->num_sgs - 1]);

		spin_lock_irqsave(&dev->lock, flags);
		if (dev->port_usb) {
			in = dev->port_usb->in_ep;
			port_usb_active = 1;
		} else {
			port_usb_active = 0;
		}
		spin_unlock_irqrestore(&dev->lock, flags);

		if (!port_usb_active) {
			__skb_queue_purge(&sg_ctx->skbs);
			kfree(req->sg);
			kfree(req->context);
			kfree(req->buf);
			usb_ep_free_request(in, req);

			return;
		}

		ret = usb_ep_queue(in, req, GFP_KERNEL);
		spin_lock_irqsave(&dev->req_lock, flags);
		switch (ret) {
		default:
			dev->net->stats.tx_dropped +=
				skb_queue_len(&sg_ctx->skbs);

			__skb_queue_purge(&sg_ctx->skbs);
			list_add_tail(&req->list, &dev->tx_reqs);
			break;
		case 0:
			net->trans_start = jiffies;
		}

		if (ret || ++req_cnt > 10) {
			dev->loop_brk_cnt++;
			break;
		}

		if (dev->tx_skb_q.qlen <  tx_start_threshold)
			netif_start_queue(net);

	}
	spin_unlock_irqrestore(&dev->req_lock, flags);
}

static netdev_tx_t eth_start_xmit(struct sk_buff *skb,
					struct net_device *net)
{
	struct eth_dev		*dev = netdev_priv(net);
	int			length = 0;
	int			tail_room = 0;
	int			extra_alloc = 0;
	int			retval;
	struct usb_request	*req = NULL;
	struct sk_buff		*new_skb;
	unsigned long		flags;
	struct usb_ep		*in = NULL;
	u16			cdc_filter = 0;
	bool			multi_pkt_xfer = false;

	if ((!skb) || (IS_ERR(skb)))
		return NETDEV_TX_OK;

	if ((!net) || (IS_ERR(net)))
		return NETDEV_TX_OK;

	length = skb->len;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb) {
		in = dev->port_usb->in_ep;
		cdc_filter = dev->port_usb->cdc_filter;
		multi_pkt_xfer = dev->port_usb->multi_pkt_xfer;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	if (!in) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	
	if (!test_bit(RMNET_MODE_LLP_IP, &dev->flags) &&
						!is_promisc(cdc_filter)) {
		u8		*dest = skb->data;

		if (is_multicast_ether_addr(dest)) {
			u16	type;

			if (is_broadcast_ether_addr(dest))
				type = USB_CDC_PACKET_TYPE_BROADCAST;
			else
				type = USB_CDC_PACKET_TYPE_ALL_MULTICAST;
			if (!(cdc_filter & type)) {
				dev_kfree_skb_any(skb);
				return NETDEV_TX_OK;
			}
		}
		
	}

	dev->tx_pkts_rcvd++;
	dev->tx_bytes_rcvd += skb->len;
	if (dev->sg_enabled) {
		skb_queue_tail(&dev->tx_skb_q, skb);
		if (dev->tx_skb_q.qlen > tx_stop_threshold) {
			dev->tx_throttle++;
			netif_stop_queue(net);
		}

		queue_work(uether_tx_wq, &dev->tx_work);
		return NETDEV_TX_OK;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->wrap && dev->port_usb)
		skb = dev->wrap(dev->port_usb, skb);
	spin_unlock_irqrestore(&dev->lock, flags);

	if (!skb) {
		dev->net->stats.tx_dropped++;

		
		return NETDEV_TX_OK;
	}

	
	spin_lock_irqsave(&dev->req_lock, flags);
	if (multi_pkt_xfer && !dev->tx_req_bufsize) {
		retval = alloc_tx_buffer(dev);
		if (retval < 0) {
			spin_unlock_irqrestore(&dev->req_lock, flags);
			return -ENOMEM;
		}
	}

	if (list_empty(&dev->tx_reqs)) {
		spin_unlock_irqrestore(&dev->req_lock, flags);
		return NETDEV_TX_BUSY;
	}

	req = container_of(dev->tx_reqs.next, struct usb_request, list);
	list_del(&req->list);

	
	if (list_empty(&dev->tx_reqs)) {
		dev->tx_throttle++;
		netif_stop_queue(net);
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);


	if (multi_pkt_xfer) {

		pr_debug("req->length:%d header_len:%u\n"
				"skb->len:%d skb->data_len:%d\n",
				req->length, dev->header_len,
				skb->len, skb->data_len);
		
		memcpy(req->buf + req->length, dev->port_usb->header,
						dev->header_len);
		
		req->length += dev->header_len;
		
		memcpy(req->buf + req->length, skb->data, skb->len);
		
		req->length += skb->len;
		length = req->length;
		dev_kfree_skb_any(skb);

		spin_lock_irqsave(&dev->req_lock, flags);
		dev->tx_skb_hold_count++;
		if (dev->tx_skb_hold_count < dev->dl_max_pkts_per_xfer) {

			if (dev->no_tx_req_used > MAX_TX_REQ_WITH_NO_INT) {
				list_add(&req->list, &dev->tx_reqs);
				spin_unlock_irqrestore(&dev->req_lock, flags);
				goto success;
			}
		}

		dev->no_tx_req_used++;
		dev->tx_skb_hold_count = 0;
		spin_unlock_irqrestore(&dev->req_lock, flags);
	} else {
		bool do_align = false;

		
		if (!gadget_is_dwc3(dev->gadget) &&
		    !IS_ALIGNED((size_t)skb->data, 4))
			do_align = true;

		if (dev->gadget->extra_buf_alloc)
			extra_alloc = EXTRA_ALLOCATION_SIZE_U_ETH;
		tail_room = skb_tailroom(skb);
		if (do_align || tail_room < extra_alloc) {
			pr_debug("%s:align skb and update tail_room %d to %d\n",
					__func__, tail_room, extra_alloc);
			tail_room = extra_alloc;
			new_skb = skb_copy_expand(skb, 0, tail_room,
						  GFP_ATOMIC);
			if (!new_skb)
				return -ENOMEM;
			dev_kfree_skb_any(skb);
			skb = new_skb;
			dev->skb_expand_cnt++;
		}

		length = skb->len;
		req->buf = skb->data;
		req->context = skb;
	}

	
	if (dev->port_usb->is_fixed &&
	    length == dev->port_usb->fixed_in_len &&
	    (length % in->maxpacket) == 0)
		req->zero = 0;
	else
		req->zero = 1;

	if (req->zero && !dev->zlp && (length % in->maxpacket) == 0) {
		req->zero = 0;
		length++;
	}

	req->length = length;

	
	if (gadget_is_dualspeed(dev->gadget) &&
			 (dev->gadget->speed == USB_SPEED_HIGH ||
			  dev->gadget->speed == USB_SPEED_SUPER)) {
		spin_lock_irqsave(&dev->req_lock, flags);
		dev->tx_qlen++;
		if (dev->tx_qlen == MAX_TX_REQ_WITH_NO_INT) {
			req->no_interrupt = 0;
			dev->tx_qlen = 0;
		} else {
			req->no_interrupt = 1;
		}
		spin_unlock_irqrestore(&dev->req_lock, flags);
	} else {
		req->no_interrupt = 0;
	}

	retval = usb_ep_queue(in, req, GFP_ATOMIC);
	switch (retval) {
	default:
		DBG(dev, "tx queue err %d\n", retval);
		break;
	case 0:
		net->trans_start = jiffies;
	}

	if (retval) {
		if (!multi_pkt_xfer)
			dev_kfree_skb_any(skb);
		else
			req->length = 0;
		dev->net->stats.tx_dropped++;
		spin_lock_irqsave(&dev->req_lock, flags);
		if (list_empty(&dev->tx_reqs))
			netif_start_queue(net);
		list_add_tail(&req->list, &dev->tx_reqs);
		spin_unlock_irqrestore(&dev->req_lock, flags);
	}
success:
	return NETDEV_TX_OK;
}


static void eth_start(struct eth_dev *dev, gfp_t gfp_flags)
{
	DBG(dev, "%s\n", __func__);

	
	rx_fill(dev, gfp_flags);

	
	dev->tx_qlen = 0;
	netif_wake_queue(dev->net);
}

static int eth_open(struct net_device *net)
{
	struct eth_dev	*dev = netdev_priv(net);
	struct gether	*link;
	int i;
	bool wait_for_rx_trigger;

	DBG(dev, "%s\n", __func__);

	dev->state = ETH_START;
	for_each_online_cpu(i)
		cpufreq_update_policy(i);

	spin_lock_irq(&dev->lock);
	link = dev->port_usb;
	spin_unlock_irq(&dev->lock);

	wait_for_rx_trigger = dev->rx_trigger_enabled && link &&
		!link->rx_triggered;

	if (netif_carrier_ok(dev->net) && !wait_for_rx_trigger)
		eth_start(dev, GFP_KERNEL);

	spin_lock_irq(&dev->lock);
	if (link && link->open)
		link->open(link);
	spin_unlock_irq(&dev->lock);

	return 0;
}

static int eth_stop(struct net_device *net)
{
	struct eth_dev	*dev = netdev_priv(net);
	unsigned long	flags;
	int i;
	enum ifc_state prev_state;

	VDBG(dev, "%s\n", __func__);

	netif_stop_queue(net);

	DBG(dev, "stop stats: rx/tx %ld/%ld, errs %ld/%ld\n",
		dev->net->stats.rx_packets, dev->net->stats.tx_packets,
		dev->net->stats.rx_errors, dev->net->stats.tx_errors
		);

	
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb) {
		struct gether	*link = dev->port_usb;
		const struct usb_endpoint_descriptor *in;
		const struct usb_endpoint_descriptor *out;

		if (link->close)
			link->close(link);

		in = link->in_ep->desc;
		out = link->out_ep->desc;
		usb_ep_disable(link->in_ep);
		usb_ep_disable(link->out_ep);
		if (netif_carrier_ok(net)) {
			if (config_ep_by_speed(dev->gadget, &link->func,
					       link->in_ep) ||
			    config_ep_by_speed(dev->gadget, &link->func,
					       link->out_ep)) {
				link->in_ep->desc = NULL;
				link->out_ep->desc = NULL;
				return -EINVAL;
			}
			DBG(dev, "host still using in/out endpoints\n");
			link->in_ep->desc = in;
			link->out_ep->desc = out;
			usb_ep_enable(link->in_ep);
			usb_ep_enable(link->out_ep);
		}
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	prev_state = dev->state;
	dev->state = ETH_STOP;

	
	if (prev_state == ETH_START)
		for_each_online_cpu(i)
			cpufreq_update_policy(i);

	return 0;
}


static u8 host_ethaddr[ETH_ALEN];

static int get_ether_addr(const char *str, u8 *dev_addr)
{
	if (str) {
		unsigned	i;

		for (i = 0; i < 6; i++) {
			unsigned char num;

			if ((*str == '.') || (*str == ':'))
				str++;
			num = hex_to_bin(*str++) << 4;
			num |= hex_to_bin(*str++);
			dev_addr [i] = num;
		}
		if (is_valid_ether_addr(dev_addr))
			return 0;
	}
	eth_random_addr(dev_addr);
	return 1;
}

static int get_ether_addr_str(u8 dev_addr[ETH_ALEN], char *str, int len)
{
	if (len < 18)
		return -EINVAL;

	snprintf(str, len, "%02x:%02x:%02x:%02x:%02x:%02x",
		 dev_addr[0], dev_addr[1], dev_addr[2],
		 dev_addr[3], dev_addr[4], dev_addr[5]);
	return 18;
}

static int get_host_ether_addr(u8 *str, u8 *dev_addr)
{
	memcpy(dev_addr, str, ETH_ALEN);
	if (is_valid_ether_addr(dev_addr))
		return 0;

	random_ether_addr(dev_addr);
	memcpy(str, dev_addr, ETH_ALEN);
	return 1;
}

static int ether_ioctl(struct net_device *, struct ifreq *, int);

static const struct net_device_ops eth_netdev_ops = {
	.ndo_open		= eth_open,
	.ndo_stop		= eth_stop,
	.ndo_start_xmit		= eth_start_xmit,
	.ndo_do_ioctl		= ether_ioctl,
	.ndo_change_mtu		= ueth_change_mtu,
	.ndo_set_mac_address 	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static const struct net_device_ops eth_netdev_ops_ip = {
	.ndo_open		= eth_open,
	.ndo_stop		= eth_stop,
	.ndo_start_xmit		= eth_start_xmit,
	.ndo_do_ioctl		= ether_ioctl,
	.ndo_change_mtu		= ueth_change_mtu_ip,
	.ndo_set_mac_address	= 0,
	.ndo_validate_addr	= 0,
};

static int rmnet_ioctl_extended(struct net_device *dev, struct ifreq *ifr)
{
	struct rmnet_ioctl_extended_s ext_cmd;
	struct eth_dev *eth_dev = netdev_priv(dev);
	int rc = 0;

	rc = copy_from_user(&ext_cmd, ifr->ifr_ifru.ifru_data,
			    sizeof(struct rmnet_ioctl_extended_s));

	if (rc) {
		DBG(eth_dev, "%s(): copy_from_user() failed\n", __func__);
		return rc;
	}

	switch (ext_cmd.extended_ioctl) {
	case RMNET_IOCTL_GET_SUPPORTED_FEATURES:
		ext_cmd.u.data = 0;
		break;

	case RMNET_IOCTL_SET_MRU:
		if (netif_running(dev))
			return -EBUSY;

		
		if ((size_t)ext_cmd.u.data > 0x4000)
			return -EINVAL;

		if (eth_dev->port_usb) {
			eth_dev->port_usb->is_fixed = true;
			eth_dev->port_usb->fixed_out_len =
				(size_t) ext_cmd.u.data;
			DBG(eth_dev, "[%s] rmnet_ioctl(): SET MRU to %u\n",
				dev->name, eth_dev->port_usb->fixed_out_len);
		} else {
			pr_err("[%s]: %s: SET MRU failed. Cable disconnected\n",
				dev->name, __func__);
			return -ENODEV;
		}
		break;

	case RMNET_IOCTL_GET_MRU:
		if (eth_dev->port_usb) {
			ext_cmd.u.data = eth_dev->port_usb->is_fixed ?
					eth_dev->port_usb->fixed_out_len :
					dev->mtu;
		} else {
			pr_err("[%s]: %s: GET MRU failed. Cable disconnected\n",
				dev->name, __func__);
			return -ENODEV;
		}
		break;

	case RMNET_IOCTL_GET_DRIVER_NAME:
		strlcpy(ext_cmd.u.if_name, dev->name,
			sizeof(ext_cmd.u.if_name));
		break;

	default:
		break;
	}

	rc = copy_to_user(ifr->ifr_ifru.ifru_data, &ext_cmd,
			  sizeof(struct rmnet_ioctl_extended_s));

	if (rc)
		DBG(eth_dev, "%s(): copy_to_user() failed\n", __func__);
	return rc;
}

static int ether_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct eth_dev	*eth_dev = netdev_priv(dev);
	void __user *addr = (void __user *) ifr->ifr_ifru.ifru_data;
	int		prev_mtu = dev->mtu;
	u32		state, old_opmode;
	int		rc = -EFAULT;

	old_opmode = eth_dev->flags;
	
	switch (cmd) {
	case RMNET_IOCTL_SET_LLP_ETHERNET:	
		
		if (test_bit(RMNET_MODE_LLP_IP, &eth_dev->flags)) {
			ether_setup(dev);
			dev->mtu = prev_mtu;
			dev->netdev_ops = &eth_netdev_ops;
			clear_bit(RMNET_MODE_LLP_IP, &eth_dev->flags);
			set_bit(RMNET_MODE_LLP_ETH, &eth_dev->flags);
			DBG(eth_dev, "[%s] ioctl(): set Ethernet proto mode\n",
					dev->name);
		}
		if (test_bit(RMNET_MODE_LLP_ETH, &eth_dev->flags))
			rc = 0;
		break;

	case RMNET_IOCTL_SET_LLP_IP:		
		
		if (test_bit(RMNET_MODE_LLP_ETH, &eth_dev->flags)) {
			
			dev->header_ops = 0;  
			dev->type = ARPHRD_RAWIP;
			dev->hard_header_len = 0;
			dev->mtu = prev_mtu;
			dev->addr_len = 0;
			dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
			dev->netdev_ops = &eth_netdev_ops_ip;
			clear_bit(RMNET_MODE_LLP_ETH, &eth_dev->flags);
			set_bit(RMNET_MODE_LLP_IP, &eth_dev->flags);
			DBG(eth_dev, "[%s] ioctl(): set IP protocol mode\n",
					dev->name);
		}
		if (test_bit(RMNET_MODE_LLP_IP, &eth_dev->flags))
			rc = 0;
		break;

	case RMNET_IOCTL_GET_LLP:	
		state = eth_dev->flags & (RMNET_MODE_LLP_ETH
						| RMNET_MODE_LLP_IP);
		if (copy_to_user(addr, &state, sizeof(state)))
			break;
		rc = 0;
		break;

	case RMNET_IOCTL_SET_RX_HEADROOM:	
		if (copy_from_user(&eth_dev->rx_needed_headroom, addr,
					sizeof(eth_dev->rx_needed_headroom)))
			break;
		DBG(eth_dev, "[%s] ioctl(): set RX HEADROOM: %x\n",
				dev->name, eth_dev->rx_needed_headroom);
		rc = 0;
		break;

	case RMNET_IOCTL_EXTENDED:
		rc = rmnet_ioctl_extended(dev, ifr);
		break;

	default:
		pr_err("[%s] error: ioctl called for unsupported cmd[%d]",
			dev->name, cmd);
		rc = -EINVAL;
	}

	DBG(eth_dev, "[%s] %s: cmd=0x%x opmode old=0x%08x new=0x%08lx\n",
		dev->name, __func__, cmd, old_opmode, eth_dev->flags);

	return rc;
}

static struct device_type gadget_type = {
	.name	= "gadget",
};

static int gether_cpufreq_notifier_cb(struct notifier_block *nfb,
		unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned int cpu = policy->cpu;
	struct eth_dev	*dev = container_of(nfb, struct eth_dev,
					cpufreq_notifier);

	if (!min_cpu_freq)
		return NOTIFY_OK;

	switch (event) {
	case CPUFREQ_ADJUST:
		pr_debug("%s: cpu:%u\n", __func__, cpu);

		if (dev->state == ETH_START)
			cpufreq_verify_within_limits(policy,
					min_cpu_freq, UINT_MAX);

		break;
	}

	return NOTIFY_OK;
}

static void update_cpu_policy_w(struct work_struct *work)
{
	int i;

	for_each_online_cpu(i)
		cpufreq_update_policy(i);
}

struct eth_dev *gether_setup_name(struct usb_gadget *g,
		const char *dev_addr, const char *host_addr,
		u8 ethaddr[ETH_ALEN], unsigned qmult, const char *netname)
{
	struct eth_dev		*dev;
	struct net_device	*net;
	int			status;


	net = alloc_etherdev(sizeof *dev);
	if (!net)
		return ERR_PTR(-ENOMEM);

	dev = netdev_priv(net);
	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->req_lock);
	INIT_WORK(&dev->work, eth_work);
	INIT_WORK(&dev->rx_work, process_rx_w);
	INIT_WORK(&dev->tx_work, process_tx_w);
	INIT_LIST_HEAD(&dev->tx_reqs);
	INIT_LIST_HEAD(&dev->rx_reqs);
	INIT_WORK(&dev->cpu_policy_w, update_cpu_policy_w);

	skb_queue_head_init(&dev->rx_frames);
	skb_queue_head_init(&dev->tx_skb_q);

	
	dev->net = net;
	dev->qmult = qmult;
	snprintf(net->name, sizeof(net->name), "%s%%d", netname);

	if (get_ether_addr(dev_addr, net->dev_addr))
		dev_warn(&g->dev,
			"using random %s ethernet address\n", "self");

	if (get_host_ether_addr(host_ethaddr, dev->host_mac))
		dev_warn(&g->dev, "using random %s ethernet address\n", "host");
	else
		dev_warn(&g->dev, "using previous %s ethernet address\n", "host");

	if (ethaddr)
		memcpy(ethaddr, dev->host_mac, ETH_ALEN);

	net->netdev_ops = &eth_netdev_ops;

	net->ethtool_ops = &ops;

	
	set_bit(RMNET_MODE_LLP_ETH, &dev->flags);

	dev->gadget = g;
	SET_NETDEV_DEV(net, &g->dev);
	SET_NETDEV_DEVTYPE(net, &gadget_type);

	status = register_netdev(net);
	if (status < 0) {
		dev_dbg(&g->dev, "register_netdev failed, %d\n", status);
		free_netdev(net);
		dev = ERR_PTR(status);
	} else {
		INFO(dev, "MAC %pM\n", net->dev_addr);
		INFO(dev, "HOST MAC %pM\n", dev->host_mac);


		netif_carrier_off(net);
		uether_debugfs_init(dev, netname);

		dev->cpufreq_notifier.notifier_call =
					gether_cpufreq_notifier_cb;
		cpufreq_register_notifier(&dev->cpufreq_notifier,
				CPUFREQ_POLICY_NOTIFIER);
	}

	return dev;
}
EXPORT_SYMBOL_GPL(gether_setup_name);

struct net_device *gether_setup_name_default(const char *netname)
{
	struct net_device	*net;
	struct eth_dev		*dev;

	net = alloc_etherdev(sizeof(*dev));
	if (!net)
		return ERR_PTR(-ENOMEM);

	dev = netdev_priv(net);
	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->req_lock);
	INIT_WORK(&dev->work, eth_work);
	INIT_WORK(&dev->rx_work, process_rx_w);
	INIT_WORK(&dev->tx_work, process_tx_w);
	INIT_LIST_HEAD(&dev->tx_reqs);
	INIT_LIST_HEAD(&dev->rx_reqs);
	INIT_WORK(&dev->cpu_policy_w, update_cpu_policy_w);

	skb_queue_head_init(&dev->rx_frames);
	skb_queue_head_init(&dev->tx_skb_q);

	
	dev->net = net;
	dev->qmult = QMULT_DEFAULT;
	snprintf(net->name, sizeof(net->name), "%s%%d", netname);

	eth_random_addr(dev->dev_mac);
	pr_warn("using random %s ethernet address\n", "self");
	eth_random_addr(dev->host_mac);
	pr_warn("using random %s ethernet address\n", "host");

	net->netdev_ops = &eth_netdev_ops;

	net->ethtool_ops = &ops;

	
	set_bit(RMNET_MODE_LLP_ETH, &dev->flags);

	SET_NETDEV_DEVTYPE(net, &gadget_type);

	the_dev = dev;

	return net;
}
EXPORT_SYMBOL_GPL(gether_setup_name_default);

int gether_register_netdev(struct net_device *net)
{
	struct eth_dev *dev;
	struct usb_gadget *g;
	struct sockaddr sa;
	int status;

	if (!net->dev.parent)
		return -EINVAL;
	dev = netdev_priv(net);
	g = dev->gadget;
	status = register_netdev(net);
	if (status < 0) {
		dev_dbg(&g->dev, "register_netdev failed, %d\n", status);
		return status;
	} else {
		INFO(dev, "HOST MAC %pM\n", dev->host_mac);

		netif_carrier_off(net);
	}
	sa.sa_family = net->type;
	memcpy(sa.sa_data, dev->dev_mac, ETH_ALEN);
	rtnl_lock();
	status = dev_set_mac_address(net, &sa);
	rtnl_unlock();
	if (status)
		pr_warn("cannot set self ethernet address: %d\n", status);
	else
		INFO(dev, "MAC %pM\n", dev->dev_mac);

	return status;
}
EXPORT_SYMBOL_GPL(gether_register_netdev);

void gether_set_gadget(struct net_device *net, struct usb_gadget *g)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	dev->gadget = g;
	SET_NETDEV_DEV(net, &g->dev);
}
EXPORT_SYMBOL_GPL(gether_set_gadget);

int gether_set_dev_addr(struct net_device *net, const char *dev_addr)
{
	struct eth_dev *dev;
	u8 new_addr[ETH_ALEN];

	dev = netdev_priv(net);
	if (get_ether_addr(dev_addr, new_addr))
		return -EINVAL;
	memcpy(dev->dev_mac, new_addr, ETH_ALEN);
	return 0;
}
EXPORT_SYMBOL_GPL(gether_set_dev_addr);

int gether_get_dev_addr(struct net_device *net, char *dev_addr, int len)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	return get_ether_addr_str(dev->dev_mac, dev_addr, len);
}
EXPORT_SYMBOL_GPL(gether_get_dev_addr);

int gether_set_host_addr(struct net_device *net, const char *host_addr)
{
	struct eth_dev *dev;
	u8 new_addr[ETH_ALEN];

	dev = netdev_priv(net);
	if (get_ether_addr(host_addr, new_addr))
		return -EINVAL;
	memcpy(dev->host_mac, new_addr, ETH_ALEN);
	return 0;
}
EXPORT_SYMBOL_GPL(gether_set_host_addr);

int gether_get_host_addr(struct net_device *net, char *host_addr, int len)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	return get_ether_addr_str(dev->host_mac, host_addr, len);
}
EXPORT_SYMBOL_GPL(gether_get_host_addr);

int gether_get_host_addr_cdc(struct net_device *net, char *host_addr, int len)
{
	struct eth_dev *dev;

	if (len < 13)
		return -EINVAL;

	dev = netdev_priv(net);
	snprintf(host_addr, len, "%pm", dev->host_mac);

	return strlen(host_addr);
}
EXPORT_SYMBOL_GPL(gether_get_host_addr_cdc);

void gether_get_host_addr_u8(struct net_device *net, u8 host_mac[ETH_ALEN])
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	memcpy(host_mac, dev->host_mac, ETH_ALEN);
}
EXPORT_SYMBOL_GPL(gether_get_host_addr_u8);

void gether_set_qmult(struct net_device *net, unsigned qmult)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	dev->qmult = qmult;
}
EXPORT_SYMBOL_GPL(gether_set_qmult);

unsigned gether_get_qmult(struct net_device *net)
{
	struct eth_dev *dev;

	dev = netdev_priv(net);
	return dev->qmult;
}
EXPORT_SYMBOL_GPL(gether_get_qmult);

int gether_get_ifname(struct net_device *net, char *name, int len)
{
	rtnl_lock();
	strlcpy(name, netdev_name(net), len);
	rtnl_unlock();
	return strlen(name);
}
EXPORT_SYMBOL_GPL(gether_get_ifname);

void gether_cleanup(struct eth_dev *dev)
{
	int i;

	if (!dev)
		return;

	
	dev->state = ETH_UNDEFINED;
	cancel_work_sync(&dev->cpu_policy_w);
	for_each_online_cpu(i)
		cpufreq_update_policy(i);

	cpufreq_unregister_notifier(&dev->cpufreq_notifier,
				CPUFREQ_POLICY_NOTIFIER);

	uether_debugfs_exit(dev);
	unregister_netdev(dev->net);
	flush_work(&dev->work);
	free_netdev(dev->net);
	the_dev = NULL;
}
EXPORT_SYMBOL_GPL(gether_cleanup);

int gether_change_mtu(int new_mtu)
{
	struct eth_dev *dev = the_dev;
	return ueth_change_mtu(dev->net, new_mtu);
}

void gether_update_dl_max_xfer_size(struct gether *link, uint32_t s)
{
	struct eth_dev		*dev = link->ioport;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	dev->dl_max_xfer_size = s;
	spin_unlock_irqrestore(&dev->lock, flags);
}

void gether_enable_sg(struct gether *link, bool enable)
{
	struct eth_dev		*dev = link->ioport;

	dev->sg_enabled = enable ? dev->gadget->sg_supported : false;
}

void gether_update_dl_max_pkts_per_xfer(struct gether *link, uint32_t n)
{
	struct eth_dev		*dev = link->ioport;
	unsigned long flags;

	if (n > DL_MAX_PKTS_PER_XFER)
		n = DL_MAX_PKTS_PER_XFER;

	spin_lock_irqsave(&dev->lock, flags);
	dev->dl_max_pkts_per_xfer = n;
	spin_unlock_irqrestore(&dev->lock, flags);
}

/**
 * gether_connect - notify network layer that USB link is active
 * @link: the USB link, set up with endpoints, descriptors matching
 *	current device speed, and any framing wrapper(s) set up.
 * Context: irqs blocked
 *
 * This is called to activate endpoints and let the network layer know
 * the connection is active ("carrier detect").  It may cause the I/O
 * queues to open and start letting network packets flow, but will in
 * any case activate the endpoints so that they respond properly to the
 * USB host.
 *
 * Verify net_device pointer returned using IS_ERR().  If it doesn't
 * indicate some error code (negative errno), ep->driver_data values
 * have been overwritten.
 */
struct net_device *gether_connect(struct gether *link)
{
	struct eth_dev		*dev = link->ioport;
	int			result = 0;
	bool wait_for_rx_trigger;

	if (!dev)
		return ERR_PTR(-EINVAL);

	if (!dev->sg_enabled) {
		link->header = kzalloc(sizeof(struct rndis_packet_msg_type),
						GFP_ATOMIC);
		if (!link->header) {
			pr_err("RNDIS header memory allocation failed.\n");
			result = -ENOMEM;
			goto fail;
		}
	}

	link->in_ep->driver_data = dev;
	result = usb_ep_enable(link->in_ep);
	if (result != 0) {
		DBG(dev, "enable %s --> %d\n",
			link->in_ep->name, result);
		goto fail0;
	}

	link->out_ep->driver_data = dev;
	result = usb_ep_enable(link->out_ep);
	if (result != 0) {
		DBG(dev, "enable %s --> %d\n",
			link->out_ep->name, result);
		goto fail1;
	}

	dev->header_len = link->header_len;
	dev->unwrap = link->unwrap;
	dev->wrap = link->wrap;
	dev->ul_max_pkts_per_xfer = link->ul_max_pkts_per_xfer;
	dev->dl_max_pkts_per_xfer = link->dl_max_pkts_per_xfer;
	dev->dl_max_xfer_size = link->dl_max_xfer_size;

	if (result == 0)
		result = alloc_requests(dev, link, qlen(dev->gadget,
					dev->qmult));

	if (result == 0) {

		dev->zlp = link->is_zlp_ok;
		DBG(dev, "qlen %d\n", qlen(dev->gadget, dev->qmult));
		dev->rx_trigger_enabled = link->rx_trigger_enabled;

		spin_lock(&dev->lock);
		dev->tx_skb_hold_count = 0;
		dev->no_tx_req_used = 0;
		dev->tx_req_bufsize = 0;
		dev->port_usb = link;
		if (netif_running(dev->net)) {
			if (link->open)
				link->open(link);
		} else {
			if (link->close)
				link->close(link);
		}
		spin_unlock(&dev->lock);

		netif_carrier_on(dev->net);

		wait_for_rx_trigger = dev->rx_trigger_enabled &&
			!link->rx_triggered;

		if (netif_running(dev->net) && !wait_for_rx_trigger)
			eth_start(dev, GFP_ATOMIC);

	
	} else {
		(void) usb_ep_disable(link->out_ep);
fail1:
		(void) usb_ep_disable(link->in_ep);
	}

	
	if (result < 0) {
fail0:
		kfree(link->header);
fail:
		return ERR_PTR(result);
	}

	return dev->net;
}
EXPORT_SYMBOL_GPL(gether_connect);

void gether_disconnect(struct gether *link)
{
	struct eth_dev		*dev = link->ioport;
	struct usb_request	*req;
	struct sk_buff		*skb;

	if (!dev)
		return;

	DBG(dev, "%s\n", __func__);

	dev->state = ETH_UNDEFINED;
	queue_work(uether_wq, &dev->cpu_policy_w);

	netif_stop_queue(dev->net);
	netif_carrier_off(dev->net);

	usb_ep_disable(link->in_ep);
	spin_lock(&dev->req_lock);
	while (!list_empty(&dev->tx_reqs)) {
		req = container_of(dev->tx_reqs.next,
					struct usb_request, list);
		list_del(&req->list);

		spin_unlock(&dev->req_lock);
		if (link->multi_pkt_xfer ||
				dev->sg_enabled) {
			kfree(req->buf);
			req->buf = NULL;
		}
		if (dev->sg_enabled) {
			kfree(req->context);
			kfree(req->sg);
		}

		usb_ep_free_request(link->in_ep, req);
		spin_lock(&dev->req_lock);
	}

	
	if (!dev->sg_enabled)
		kfree(link->header);
	link->header = NULL;
	spin_unlock(&dev->req_lock);

	skb_queue_purge(&dev->tx_skb_q);

	link->in_ep->driver_data = NULL;
	link->in_ep->desc = NULL;

	usb_ep_disable(link->out_ep);
	spin_lock(&dev->req_lock);
	while (!list_empty(&dev->rx_reqs)) {
		req = container_of(dev->rx_reqs.next,
					struct usb_request, list);
		list_del(&req->list);

		spin_unlock(&dev->req_lock);
		usb_ep_free_request(link->out_ep, req);
		spin_lock(&dev->req_lock);
	}
	spin_unlock(&dev->req_lock);

	spin_lock(&dev->rx_frames.lock);
	while ((skb = __skb_dequeue(&dev->rx_frames)))
		dev_kfree_skb_any(skb);
	spin_unlock(&dev->rx_frames.lock);

	link->out_ep->driver_data = NULL;
	link->out_ep->desc = NULL;

	pr_debug("%s(): tx_throttle count= %lu", __func__,
					dev->tx_throttle);
	
	dev->tx_throttle = 0;
	dev->rx_throttle = 0;

	
	dev->header_len = 0;
	dev->unwrap = NULL;
	dev->wrap = NULL;
	dev->rx_trigger_enabled = 0;

	spin_lock(&dev->lock);
	dev->port_usb = NULL;
	spin_unlock(&dev->lock);
}
EXPORT_SYMBOL_GPL(gether_disconnect);


static int uether_stat_show(struct seq_file *s, void *unused)
{
	struct eth_dev *dev = s->private;
	int ret = 0;
	int i;

	if (dev) {
		seq_printf(s, "rx_throttle = %lu\n",
					dev->rx_throttle);
		seq_printf(s, "tx_qlen=%u tx_throttle = %lu\n aggr count:",
					dev->tx_skb_q.qlen,
					dev->tx_throttle);
		for (i = 0; i < DL_MAX_PKTS_PER_XFER; i++)
			seq_printf(s, "%u\t", dev->tx_aggr_cnt[i]);

		seq_printf(s, "\nloop_brk_cnt = %u\n tx_pkts_rcvd=%u\n",
					dev->loop_brk_cnt,
					dev->tx_pkts_rcvd);
		seq_printf(s, "skb_expand_cnt = %lu\n",
					dev->skb_expand_cnt);
	}

	return ret;
}

static int uether_open(struct inode *inode, struct file *file)
{
	return single_open(file, uether_stat_show, inode->i_private);
}

static ssize_t uether_stat_reset(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct eth_dev *dev = s->private;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	
	dev->tx_throttle = 0;
	dev->rx_throttle = 0;
	dev->skb_expand_cnt = 0;
	spin_unlock_irqrestore(&dev->lock, flags);
	return count;
}

const struct file_operations uether_stats_ops = {
	.open = uether_open,
	.read = seq_read,
	.write = uether_stat_reset,
};

static int uether_bytes_rcvd_show(struct seq_file *s, void *unused)
{
	struct eth_dev *dev = s->private;

	if (dev)
		seq_printf(s, "%u\n", dev->tx_bytes_rcvd);

	return 0;
}

static int uether_bytes_rcvd_open(struct inode *inode, struct file *file)
{
	return single_open(file, uether_bytes_rcvd_show, inode->i_private);
}

static ssize_t uether_bytes_rcvd_reset(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct eth_dev *dev = s->private;

	dev->tx_bytes_rcvd = 0;

	return count;
}

const struct file_operations uether_bytes_rcvd_ops = {
	.open = uether_bytes_rcvd_open,
	.read = seq_read,
	.write = uether_bytes_rcvd_reset,
};

static void uether_debugfs_init(struct eth_dev *dev, const char *name)
{
	struct dentry *uether_dent;
	struct dentry *uether_dfile;

	uether_dent = debugfs_create_dir(name, 0);
	if (IS_ERR(uether_dent))
		return;
	dev->uether_dent = uether_dent;

	uether_dfile = debugfs_create_file("status", S_IRUGO | S_IWUSR,
				uether_dent, dev, &uether_stats_ops);
	if (!uether_dfile || IS_ERR(uether_dfile))
		debugfs_remove(uether_dent);

	uether_dfile = debugfs_create_file("tx_bytes_rcvd", S_IRUGO | S_IWUSR,
				uether_dent, dev, &uether_bytes_rcvd_ops);
	if (!uether_dfile || IS_ERR(uether_dfile))
		debugfs_remove_recursive(uether_dent);
}

static void uether_debugfs_exit(struct eth_dev *dev)
{
	debugfs_remove_recursive(dev->uether_dent);
}

int gether_up(struct gether *link)
{
	struct eth_dev *dev = link->ioport;

	if (dev && netif_carrier_ok(dev->net))
		eth_start(dev, GFP_KERNEL);

	return 0;
}

static int __init gether_init(void)
{
	uether_wq  = create_singlethread_workqueue("uether");
	if (!uether_wq) {
		pr_err("%s: Unable to create workqueue: uether\n", __func__);
		return -ENOMEM;
	}

	uether_tx_wq = alloc_workqueue("uether_tx",
				WQ_CPU_INTENSIVE | WQ_UNBOUND, 1);
	if (!uether_tx_wq) {
		destroy_workqueue(uether_wq);
		pr_err("%s: Unable to create workqueue: uether\n", __func__);
		return -ENOMEM;
	}

	return 0;
}
module_init(gether_init);

static void __exit gether_exit(void)
{
	destroy_workqueue(uether_tx_wq);
	destroy_workqueue(uether_wq);

}
module_exit(gether_exit);
MODULE_AUTHOR("David Brownell");
MODULE_DESCRIPTION("ethernet over USB driver");
MODULE_LICENSE("GPL v2");
