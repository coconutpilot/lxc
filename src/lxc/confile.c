/*
 * lxc: linux Container library
 *
 * (C) Copyright IBM Corp. 2007, 2008
 *
 * Authors:
 * Daniel Lezcano <dlezcano at fr.ibm.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <sys/personality.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>

#include "parse.h"
#include "confile.h"
#include "utils.h"

#include <lxc/log.h>
#include <lxc/conf.h>
#include "network.h"

lxc_log_define(lxc_confile, lxc);

static int config_personality(const char *, char *, struct lxc_conf *);
static int config_pts(const char *, char *, struct lxc_conf *);
static int config_tty(const char *, char *, struct lxc_conf *);
static int config_ttydir(const char *, char *, struct lxc_conf *);
#if HAVE_APPARMOR
static int config_aa_profile(const char *, char *, struct lxc_conf *);
#endif
static int config_cgroup(const char *, char *, struct lxc_conf *);
static int config_mount(const char *, char *, struct lxc_conf *);
static int config_rootfs(const char *, char *, struct lxc_conf *);
static int config_rootfs_mount(const char *, char *, struct lxc_conf *);
static int config_pivotdir(const char *, char *, struct lxc_conf *);
static int config_utsname(const char *, char *, struct lxc_conf *);
static int config_hook(const char *key, char *value, struct lxc_conf *lxc_conf);
static int config_network_type(const char *, char *, struct lxc_conf *);
static int config_network_flags(const char *, char *, struct lxc_conf *);
static int config_network_link(const char *, char *, struct lxc_conf *);
static int config_network_name(const char *, char *, struct lxc_conf *);
static int config_network_veth_pair(const char *, char *, struct lxc_conf *);
static int config_network_macvlan_mode(const char *, char *, struct lxc_conf *);
static int config_network_hwaddr(const char *, char *, struct lxc_conf *);
static int config_network_vlan_id(const char *, char *, struct lxc_conf *);
static int config_network_mtu(const char *, char *, struct lxc_conf *);
static int config_network_ipv4(const char *, char *, struct lxc_conf *);
static int config_network_ipv4_gateway(const char *, char *, struct lxc_conf *);
static int config_network_script(const char *, char *, struct lxc_conf *);
static int config_network_ipv6(const char *, char *, struct lxc_conf *);
static int config_network_ipv6_gateway(const char *, char *, struct lxc_conf *);
static int config_cap_drop(const char *, char *, struct lxc_conf *);
static int config_console(const char *, char *, struct lxc_conf *);
static int config_seccomp(const char *, char *, struct lxc_conf *);
static int config_includefile(const char *, char *, struct lxc_conf *);
static int config_network_nic(const char *, char *, struct lxc_conf *);

static struct lxc_config_t config[] = {

	{ "lxc.arch",                 config_personality          },
	{ "lxc.pts",                  config_pts                  },
	{ "lxc.tty",                  config_tty                  },
	{ "lxc.devttydir",            config_ttydir               },
#if HAVE_APPARMOR
	{ "lxc.aa_profile",            config_aa_profile          },
#endif
	{ "lxc.cgroup",               config_cgroup               },
	{ "lxc.mount",                config_mount                },
	{ "lxc.rootfs.mount",         config_rootfs_mount         },
	{ "lxc.rootfs",               config_rootfs               },
	{ "lxc.pivotdir",             config_pivotdir             },
	{ "lxc.utsname",              config_utsname              },
	{ "lxc.hook.pre-start",       config_hook                 },
	{ "lxc.hook.pre-mount",           config_hook             },
	{ "lxc.hook.mount",           config_hook                 },
	{ "lxc.hook.start",           config_hook                 },
	{ "lxc.hook.post-stop",       config_hook                 },
	{ "lxc.network.type",         config_network_type         },
	{ "lxc.network.flags",        config_network_flags        },
	{ "lxc.network.link",         config_network_link         },
	{ "lxc.network.name",         config_network_name         },
	{ "lxc.network.macvlan.mode", config_network_macvlan_mode },
	{ "lxc.network.veth.pair",    config_network_veth_pair    },
	{ "lxc.network.script.up",    config_network_script       },
	{ "lxc.network.script.down",  config_network_script       },
	{ "lxc.network.hwaddr",       config_network_hwaddr       },
	{ "lxc.network.mtu",          config_network_mtu          },
	{ "lxc.network.vlan.id",      config_network_vlan_id      },
	{ "lxc.network.ipv4.gateway", config_network_ipv4_gateway },
	{ "lxc.network.ipv4",         config_network_ipv4         },
	{ "lxc.network.ipv6.gateway", config_network_ipv6_gateway },
	{ "lxc.network.ipv6",         config_network_ipv6         },
	/* config_network_nic must come after all other 'lxc.network.*' entries */
	{ "lxc.network.",             config_network_nic          },
	{ "lxc.cap.drop",             config_cap_drop             },
	{ "lxc.console",              config_console              },
	{ "lxc.seccomp",              config_seccomp              },
	{ "lxc.include",              config_includefile          },
};

static const size_t config_size = sizeof(config)/sizeof(struct lxc_config_t);

extern struct lxc_config_t *lxc_getconfig(const char *key)
{
	int i;

	for (i = 0; i < config_size; i++)
		if (!strncmp(config[i].name, key,
			     strlen(config[i].name)))
			return &config[i];
	return NULL;
}

#define strprint(str, inlen, ...) \
	do { \
		len = snprintf(str, inlen, ##__VA_ARGS__); \
		if (len < 0) { SYSERROR("snprintf"); return -1; }; \
		fulllen += len; \
		if (inlen > 0) { \
			if (str) str += len; \
			inlen -= len; \
			if (inlen < 0) inlen = 0; \
		} \
	} while (0);

int lxc_listconfigs(char *retv, int inlen)
{
	int i, fulllen = 0, len;

	if (!retv)
		inlen = 0;
	else
		memset(retv, 0, inlen);
	for (i = 0; i < config_size; i++) {
		char *s = config[i].name;
		if (s[strlen(s)-1] == '.')
			continue;
		strprint(retv, inlen, "%s\n", s);
	}
	return fulllen;
}

/*
 * config entry is something like "lxc.network.0.ipv4"
 * the key 'lxc.network.' was found.  So we make sure next
 * comes an integer, find the right callback (by rewriting
 * the key), and call it.
 */
static int config_network_nic(const char *key, char *value,
			       struct lxc_conf *lxc_conf)
{
	char *copy = strdup(key), *p;
	int ret = -1;
	struct lxc_config_t *config;

	if (!copy) {
		SYSERROR("failed to allocate memory");
		return -1;
	}
	/*
	 * ok we know that to get here we've got "lxc.network."
	 * and it isn't any of the other network entries.  So
	 * after the second . should come an integer (# of defined
	 * nic) followed by a valid entry.
	 */
	if (*(key+12) < '0' || *(key+12) > '9')
		goto out;
	p = index(key+12, '.');
	if (!p)
		goto out;
	strcpy(copy+12, p+1);
	config = lxc_getconfig(copy);
	if (!config) {
		ERROR("unknown key %s", key);
		goto out;
	}
	ret = config->cb(key, value, lxc_conf);

out:
	free(copy);
	return ret;
}

static int config_network_type(const char *key, char *value,
			       struct lxc_conf *lxc_conf)
{
	struct lxc_list *network = &lxc_conf->network;
	struct lxc_netdev *netdev;
	struct lxc_list *list;

	netdev = malloc(sizeof(*netdev));
	if (!netdev) {
		SYSERROR("failed to allocate memory");
		return -1;
	}

	memset(netdev, 0, sizeof(*netdev));
	lxc_list_init(&netdev->ipv4);
	lxc_list_init(&netdev->ipv6);

	list = malloc(sizeof(*list));
	if (!list) {
		SYSERROR("failed to allocate memory");
		return -1;
	}

	lxc_list_init(list);
	list->elem = netdev;

	lxc_list_add_tail(network, list);

	if (!strcmp(value, "veth"))
		netdev->type = LXC_NET_VETH;
	else if (!strcmp(value, "macvlan"))
		netdev->type = LXC_NET_MACVLAN;
	else if (!strcmp(value, "vlan"))
		netdev->type = LXC_NET_VLAN;
	else if (!strcmp(value, "phys"))
		netdev->type = LXC_NET_PHYS;
	else if (!strcmp(value, "empty"))
		netdev->type = LXC_NET_EMPTY;
	else {
		ERROR("invalid network type %s", value);
		return -1;
	}
	return 0;
}

static int config_ip_prefix(struct in_addr *addr)
{
	if (IN_CLASSA(addr->s_addr))
		return 32 - IN_CLASSA_NSHIFT;
	if (IN_CLASSB(addr->s_addr))
		return 32 - IN_CLASSB_NSHIFT;
	if (IN_CLASSC(addr->s_addr))
		return 32 - IN_CLASSC_NSHIFT;

	return 0;
}

/*
 * if you have p="lxc.network.0.link", pass it p+12
 * to get back '0' (the index of the nic)
 */
static int get_network_netdev_idx(const char *key)
{
	int ret, idx;

	if (*key < '0' || *key > '9')
		return -1;
	ret = sscanf(key, "%d", &idx);
	if (ret != 1)
		return -1;
	return idx;
}

/*
 * if you have p="lxc.network.0", pass this p+12 and it will return
 * the netdev of the first configured nic
 */
static struct lxc_netdev *get_netdev_from_key(const char *key,
					      struct lxc_list *network)
{
	int i = 0, idx = get_network_netdev_idx(key);
	struct lxc_netdev *netdev = NULL;
	struct lxc_list *it;
	if (idx == -1)
		return NULL;
	lxc_list_for_each(it, network) {
		if (idx == i++) {
			netdev = it->elem;
			break;
		}
	}
	return netdev;
}

extern int lxc_list_nicconfigs(struct lxc_conf *c, char *key, char *retv, int inlen)
{
	struct lxc_netdev *netdev;
	int fulllen = 0, len;

	netdev = get_netdev_from_key(key+12, &c->network);
	if (!netdev)
		return -1;

	if (!retv)
		inlen = 0;
	else
		memset(retv, 0, inlen);

	strprint(retv, inlen, "script.up\n");
	if (netdev->type != LXC_NET_EMPTY) {
		strprint(retv, inlen, "flags\n");
		strprint(retv, inlen, "link\n");
		strprint(retv, inlen, "name\n");
		strprint(retv, inlen, "hwaddr\n");
		strprint(retv, inlen, "mtu\n");
		strprint(retv, inlen, "ipv6\n");
		strprint(retv, inlen, "ipv6_gateway\n");
		strprint(retv, inlen, "ipv4\n");
		strprint(retv, inlen, "ipv4_gateway\n");
	}
	switch(netdev->type) {
	case LXC_NET_VETH:
		strprint(retv, inlen, "veth.pair\n");
		break;
	case LXC_NET_MACVLAN:
		strprint(retv, inlen, "macvlan.mode\n");
		break;
	case LXC_NET_VLAN:
		strprint(retv, inlen, "vlan.id\n");
		break;
	case LXC_NET_PHYS:
		break;
	}
	return fulllen;
}

static struct lxc_netdev *network_netdev(const char *key, const char *value,
					 struct lxc_list *network)
{
	struct lxc_netdev *netdev = NULL;

	if (lxc_list_empty(network)) {
		ERROR("network is not created for '%s' = '%s' option",
		      key, value);
		return NULL;
	}

	if (get_network_netdev_idx(key+12) == -1)
		netdev = lxc_list_last_elem(network);
	else
		netdev = get_netdev_from_key(key+12, network);

	if (!netdev) {
		ERROR("no network device defined for '%s' = '%s' option",
		      key, value);
		return NULL;
	}

	return netdev;
}

static int network_ifname(char **valuep, char *value)
{
	if (strlen(value) >= IFNAMSIZ) {
		ERROR("invalid interface name: %s", value);
		return -1;
	}

	*valuep = strdup(value);
	if (!*valuep) {
		ERROR("failed to dup string '%s'", value);
		return -1;
	}

	return 0;
}

#ifndef MACVLAN_MODE_PRIVATE
#  define MACVLAN_MODE_PRIVATE 1
#endif

#ifndef MACVLAN_MODE_VEPA
#  define MACVLAN_MODE_VEPA 2
#endif

#ifndef MACVLAN_MODE_BRIDGE
#  define MACVLAN_MODE_BRIDGE 4
#endif

static int macvlan_mode(int *valuep, char *value)
{
	struct mc_mode {
		char *name;
		int mode;
	} m[] = {
		{ "private", MACVLAN_MODE_PRIVATE },
		{ "vepa", MACVLAN_MODE_VEPA },
		{ "bridge", MACVLAN_MODE_BRIDGE },
	};

	int i;

	for (i = 0; i < sizeof(m)/sizeof(m[0]); i++) {
		if (strcmp(m[i].name, value))
			continue;

		*valuep = m[i].mode;
		return 0;
	}

	return -1;
}

static int config_network_flags(const char *key, char *value,
				struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	netdev->flags |= IFF_UP;

	return 0;
}

static int config_network_link(const char *key, char *value,
			       struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	return network_ifname(&netdev->link, value);
}

static int config_network_name(const char *key, char *value,
			       struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	return network_ifname(&netdev->name, value);
}

static int config_network_veth_pair(const char *key, char *value,
				    struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	return network_ifname(&netdev->priv.veth_attr.pair, value);
}

static int config_network_macvlan_mode(const char *key, char *value,
				       struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	return macvlan_mode(&netdev->priv.macvlan_attr.mode, value);
}

static int config_network_hwaddr(const char *key, char *value,
				 struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	netdev->hwaddr = strdup(value);
	if (!netdev->hwaddr) {
		SYSERROR("failed to dup string '%s'", value);
		return -1;
	}

	return 0;
}

static int config_network_vlan_id(const char *key, char *value,
			       struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	if (get_u16(&netdev->priv.vlan_attr.vid, value, 0))
		return -1;

	return 0;
}

static int config_network_mtu(const char *key, char *value,
			      struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	netdev->mtu = strdup(value);
	if (!netdev->mtu) {
		SYSERROR("failed to dup string '%s'", value);
		return -1;
	}

	return 0;
}

static int config_network_ipv4(const char *key, char *value,
			       struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;
	struct lxc_inetdev *inetdev;
	struct lxc_list *list;
	char *cursor, *slash, *addr = NULL, *bcast = NULL, *prefix = NULL;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	inetdev = malloc(sizeof(*inetdev));
	if (!inetdev) {
		SYSERROR("failed to allocate ipv4 address");
		return -1;
	}
	memset(inetdev, 0, sizeof(*inetdev));

	list = malloc(sizeof(*list));
	if (!list) {
		SYSERROR("failed to allocate memory");
		return -1;
	}

	lxc_list_init(list);
	list->elem = inetdev;

	addr = value;

	cursor = strstr(addr, " ");
	if (cursor) {
		*cursor = '\0';
		bcast = cursor + 1;
	}

	slash = strstr(addr, "/");
	if (slash) {
		*slash = '\0';
		prefix = slash + 1;
	}

	if (!addr) {
		ERROR("no address specified");
		return -1;
	}

	if (!inet_pton(AF_INET, addr, &inetdev->addr)) {
		SYSERROR("invalid ipv4 address: %s", value);
		return -1;
	}

	if (bcast && !inet_pton(AF_INET, bcast, &inetdev->bcast)) {
		SYSERROR("invalid ipv4 broadcast address: %s", value);
		return -1;
	}

	/* no prefix specified, determine it from the network class */
	inetdev->prefix = prefix ? atoi(prefix) :
		config_ip_prefix(&inetdev->addr);

	/* if no broadcast address, let compute one from the
	 * prefix and address
	 */
	if (!bcast) {
		inetdev->bcast.s_addr = inetdev->addr.s_addr;
		inetdev->bcast.s_addr |=
			htonl(INADDR_BROADCAST >>  inetdev->prefix);
	}

	lxc_list_add(&netdev->ipv4, list);

	return 0;
}

static int config_network_ipv4_gateway(const char *key, char *value,
			               struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;
	struct in_addr *gw;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	gw = malloc(sizeof(*gw));
	if (!gw) {
		SYSERROR("failed to allocate ipv4 gateway address");
		return -1;
	}

	if (!value) {
		ERROR("no ipv4 gateway address specified");
		return -1;
	}

	if (!strcmp(value, "auto")) {
		netdev->ipv4_gateway = NULL;
		netdev->ipv4_gateway_auto = true;
	} else {
		if (!inet_pton(AF_INET, value, gw)) {
			SYSERROR("invalid ipv4 gateway address: %s", value);
			return -1;
		}

		netdev->ipv4_gateway = gw;
		netdev->ipv4_gateway_auto = false;
	}

	return 0;
}

static int config_network_ipv6(const char *key, char *value,
			       struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;
	struct lxc_inet6dev *inet6dev;
	struct lxc_list *list;
	char *slash;
	char *netmask;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	inet6dev = malloc(sizeof(*inet6dev));
	if (!inet6dev) {
		SYSERROR("failed to allocate ipv6 address");
		return -1;
	}
	memset(inet6dev, 0, sizeof(*inet6dev));

	list = malloc(sizeof(*list));
	if (!list) {
		SYSERROR("failed to allocate memory");
		return -1;
	}

	lxc_list_init(list);
	list->elem = inet6dev;

	inet6dev->prefix = 64;
	slash = strstr(value, "/");
	if (slash) {
		*slash = '\0';
		netmask = slash + 1;
		inet6dev->prefix = atoi(netmask);
	}

	if (!inet_pton(AF_INET6, value, &inet6dev->addr)) {
		SYSERROR("invalid ipv6 address: %s", value);
		return -1;
	}

	lxc_list_add(&netdev->ipv6, list);

	return 0;
}

static int config_network_ipv6_gateway(const char *key, char *value,
			               struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;
	struct in6_addr *gw;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
		return -1;

	gw = malloc(sizeof(*gw));
	if (!gw) {
		SYSERROR("failed to allocate ipv6 gateway address");
		return -1;
	}

	if (!value) {
		ERROR("no ipv6 gateway address specified");
		return -1;
	}

	if (!strcmp(value, "auto")) {
		netdev->ipv6_gateway = NULL;
		netdev->ipv6_gateway_auto = true;
	} else {
		if (!inet_pton(AF_INET6, value, gw)) {
			SYSERROR("invalid ipv6 gateway address: %s", value);
			return -1;
		}

		netdev->ipv6_gateway = gw;
		netdev->ipv6_gateway_auto = false;
	}

	return 0;
}

static int config_network_script(const char *key, char *value,
				 struct lxc_conf *lxc_conf)
{
	struct lxc_netdev *netdev;

	netdev = network_netdev(key, value, &lxc_conf->network);
	if (!netdev)
	return -1;

	char *copy = strdup(value);
	if (!copy) {
		SYSERROR("failed to dup string '%s'", value);
		return -1;
	}
	if (strstr(key, "script.up") != NULL) {
		netdev->upscript = copy;
		return 0;
	}
	if (strcmp(key, "lxc.network.script.down") == 0) {
		netdev->downscript = copy;
		return 0;
	}
	SYSERROR("Unknown key: %s", key);
	free(copy);
	return -1;
}

static int add_hook(struct lxc_conf *lxc_conf, int which, char *hook)
{
	struct lxc_list *hooklist;

	hooklist = malloc(sizeof(*hooklist));
	if (!hooklist) {
		free(hook);
		return -1;
	}
	hooklist->elem = hook;
	lxc_list_add_tail(&lxc_conf->hooks[which], hooklist);
	return 0;
}

static int config_seccomp(const char *key, char *value,
				 struct lxc_conf *lxc_conf)
{
	char *path;

	if (lxc_conf->seccomp) {
		ERROR("seccomp already defined");
		return -1;
	}
	path = strdup(value);
	if (!path) {
		SYSERROR("failed to strdup '%s': %m", value);
		return -1;
	}

	lxc_conf->seccomp = path;

	return 0;
}

static int config_hook(const char *key, char *value,
				 struct lxc_conf *lxc_conf)
{
	char *copy = strdup(value);
	if (!copy) {
		SYSERROR("failed to dup string '%s'", value);
		return -1;
	}
	if (strcmp(key, "lxc.hook.pre-start") == 0)
		return add_hook(lxc_conf, LXCHOOK_PRESTART, copy);
	else if (strcmp(key, "lxc.hook.pre-mount") == 0)
		return add_hook(lxc_conf, LXCHOOK_PREMOUNT, copy);
	else if (strcmp(key, "lxc.hook.mount") == 0)
		return add_hook(lxc_conf, LXCHOOK_MOUNT, copy);
	else if (strcmp(key, "lxc.hook.start") == 0)
		return add_hook(lxc_conf, LXCHOOK_START, copy);
	else if (strcmp(key, "lxc.hook.post-stop") == 0)
		return add_hook(lxc_conf, LXCHOOK_POSTSTOP, copy);
	SYSERROR("Unknown key: %s", key);
	free(copy);
	return -1;
}

static int config_personality(const char *key, char *value,
			      struct lxc_conf *lxc_conf)
{
	signed long personality = lxc_config_parse_arch(value);

	if (personality >= 0)
		lxc_conf->personality = personality;
	else
		WARN("unsupported personality '%s'", value);

	return 0;
}

static int config_pts(const char *key, char *value, struct lxc_conf *lxc_conf)
{
	int maxpts = atoi(value);

	lxc_conf->pts = maxpts;

	return 0;
}

static int config_tty(const char *key, char *value, struct lxc_conf *lxc_conf)
{
	int nbtty = atoi(value);

	lxc_conf->tty = nbtty;

	return 0;
}

static int config_ttydir(const char *key, char *value,
			  struct lxc_conf *lxc_conf)
{
	char *path;

	if (!value || strlen(value) == 0)
		return 0;
	path = strdup(value);
	if (!path) {
		SYSERROR("failed to strdup '%s': %m", value);
		return -1;
	}

	lxc_conf->ttydir = path;

	return 0;
}

#if HAVE_APPARMOR
static int config_aa_profile(const char *key, char *value, struct lxc_conf *lxc_conf)
{
	char *path;

	if (!value || strlen(value) == 0)
		return 0;
	path = strdup(value);
	if (!path) {
		SYSERROR("failed to strdup '%s': %m", value);
		return -1;
	}

	lxc_conf->aa_profile = path;

	return 0;
}
#endif

static int config_cgroup(const char *key, char *value, struct lxc_conf *lxc_conf)
{
	char *token = "lxc.cgroup.";
	char *subkey;
	struct lxc_list *cglist = NULL;
	struct lxc_cgroup *cgelem = NULL;

	subkey = strstr(key, token);

	if (!subkey)
		return -1;

	if (!strlen(subkey))
		return -1;

	if (strlen(subkey) == strlen(token))
		return -1;

	subkey += strlen(token);

	cglist = malloc(sizeof(*cglist));
	if (!cglist)
		goto out;

	cgelem = malloc(sizeof(*cgelem));
	if (!cgelem)
		goto out;
	memset(cgelem, 0, sizeof(*cgelem));

	cgelem->subsystem = strdup(subkey);
	cgelem->value = strdup(value);

	if (!cgelem->subsystem || !cgelem->value)
		goto out;

	cglist->elem = cgelem;

	lxc_list_add_tail(&lxc_conf->cgroup, cglist);

	return 0;

out:
	if (cglist)
		free(cglist);

	if (cgelem) {
		if (cgelem->subsystem)
			free(cgelem->subsystem);

		if (cgelem->value)
			free(cgelem->value);

		free(cgelem);
	}

	return -1;
}

static int config_fstab(const char *key, char *value, struct lxc_conf *lxc_conf)
{
	if (strlen(value) >= MAXPATHLEN) {
		ERROR("%s path is too long", value);
		return -1;
	}

	lxc_conf->fstab = strdup(value);
	if (!lxc_conf->fstab) {
		SYSERROR("failed to duplicate string %s", value);
		return -1;
	}

	return 0;
}

static int config_mount(const char *key, char *value, struct lxc_conf *lxc_conf)
{
	char *fstab_token = "lxc.mount";
	char *token = "lxc.mount.entry";
	char *subkey;
	char *mntelem;
	struct lxc_list *mntlist;

	subkey = strstr(key, token);

	if (!subkey) {
		subkey = strstr(key, fstab_token);

		if (!subkey)
			return -1;

		return config_fstab(key, value, lxc_conf);
	}

	if (!strlen(subkey))
		return -1;

	mntlist = malloc(sizeof(*mntlist));
	if (!mntlist)
		return -1;

	mntelem = strdup(value);
	if (!mntelem)
		return -1;
	mntlist->elem = mntelem;

	lxc_list_add_tail(&lxc_conf->mount_list, mntlist);

	return 0;
}

static int config_cap_drop(const char *key, char *value,
			   struct lxc_conf *lxc_conf)
{
	char *dropcaps, *sptr, *token;
	struct lxc_list *droplist;
	int ret = -1;

	if (!strlen(value))
		return -1;

	dropcaps = strdup(value);
	if (!dropcaps) {
		SYSERROR("failed to dup '%s'", value);
		return -1;
	}

	/* in case several capability drop is specified in a single line
	 * split these caps in a single element for the list */
	for (;;) {
                token = strtok_r(dropcaps, " \t", &sptr);
                if (!token) {
			ret = 0;
                        break;
		}
		dropcaps = NULL;

		droplist = malloc(sizeof(*droplist));
		if (!droplist) {
			SYSERROR("failed to allocate drop list");
			break;
		}

		/* note - i do believe we're losing memory here,
		   note that token is already pointing into dropcaps
		   which is strdup'd.  we never free that bit.
		 */
		droplist->elem = strdup(token);
		if (!droplist->elem) {
			SYSERROR("failed to dup '%s'", token);
			free(droplist);
			break;
		}

		lxc_list_add_tail(&lxc_conf->caps, droplist);
        }

	free(dropcaps);

	return ret;
}

static int config_console(const char *key, char *value,
			  struct lxc_conf *lxc_conf)
{
	char *path;

	path = strdup(value);
	if (!path) {
		SYSERROR("failed to strdup '%s': %m", value);
		return -1;
	}

	lxc_conf->console.path = path;

	return 0;
}

static int config_includefile(const char *key, char *value,
			  struct lxc_conf *lxc_conf)
{
	return lxc_config_read(value, lxc_conf);
}

static int config_rootfs(const char *key, char *value, struct lxc_conf *lxc_conf)
{
	if (strlen(value) >= MAXPATHLEN) {
		ERROR("%s path is too long", value);
		return -1;
	}

	lxc_conf->rootfs.path = strdup(value);
	if (!lxc_conf->rootfs.path) {
		SYSERROR("failed to duplicate string %s", value);
		return -1;
	}

	return 0;
}

static int config_rootfs_mount(const char *key, char *value, struct lxc_conf *lxc_conf)
{
	if (strlen(value) >= MAXPATHLEN) {
		ERROR("%s path is too long", value);
		return -1;
	}

	lxc_conf->rootfs.mount = strdup(value);
	if (!lxc_conf->rootfs.mount) {
		SYSERROR("failed to duplicate string '%s'", value);
		return -1;
	}

	return 0;
}

static int config_pivotdir(const char *key, char *value, struct lxc_conf *lxc_conf)
{
	if (strlen(value) >= MAXPATHLEN) {
		ERROR("%s path is too long", value);
		return -1;
	}

	lxc_conf->rootfs.pivot = strdup(value);
	if (!lxc_conf->rootfs.pivot) {
		SYSERROR("failed to duplicate string %s", value);
		return -1;
	}

	return 0;
}

static int config_utsname(const char *key, char *value, struct lxc_conf *lxc_conf)
{
	struct utsname *utsname;

	utsname = malloc(sizeof(*utsname));
	if (!utsname) {
		SYSERROR("failed to allocate memory");
		return -1;
	}

	if (strlen(value) >= sizeof(utsname->nodename)) {
		ERROR("node name '%s' is too long",
			      utsname->nodename);
		return -1;
	}

	strcpy(utsname->nodename, value);
	lxc_conf->utsname = utsname;

	return 0;
}

static int parse_line(char *buffer, void *data)
{
	struct lxc_config_t *config;
	char *line, *linep;
	char *dot;
	char *key;
	char *value;
	int ret = 0;

	if (lxc_is_line_empty(buffer))
		return 0;

	/* we have to dup the buffer otherwise, at the re-exec for
	 * reboot we modified the original string on the stack by
	 * replacing '=' by '\0' below
	 */
	linep = line = strdup(buffer);
	if (!line) {
		SYSERROR("failed to allocate memory for '%s'", buffer);
		return -1;
	}

	line += lxc_char_left_gc(line, strlen(line));

	/* martian option - ignoring it, the commented lines beginning by '#'
	 * fall in this case
	 */
	if (strncmp(line, "lxc.", 4))
		goto out;

	ret = -1;

	dot = strstr(line, "=");
	if (!dot) {
		ERROR("invalid configuration line: %s", line);
		goto out;
	}

	*dot = '\0';
	value = dot + 1;

	key = line;
	key[lxc_char_right_gc(key, strlen(key))] = '\0';

	value += lxc_char_left_gc(value, strlen(value));
	value[lxc_char_right_gc(value, strlen(value))] = '\0';

	config = lxc_getconfig(key);
	if (!config) {
		ERROR("unknown key %s", key);
		goto out;
	}

	ret = config->cb(key, value, data);

out:
	free(linep);
	return ret;
}

int lxc_config_readline(char *buffer, struct lxc_conf *conf)
{
	return parse_line(buffer, conf);
}

int lxc_config_read(const char *file, struct lxc_conf *conf)
{
	return lxc_file_for_each_line(file, parse_line, conf);
}

int lxc_config_define_add(struct lxc_list *defines, char* arg)
{
	struct lxc_list *dent;

	dent = malloc(sizeof(struct lxc_list));
	if (!dent)
		return -1;

	dent->elem = arg;
	lxc_list_add_tail(defines, dent);
	return 0;
}

int lxc_config_define_load(struct lxc_list *defines, struct lxc_conf *conf)
{
	struct lxc_list *it;
	int ret = 0;

	lxc_list_for_each(it, defines) {
		ret = lxc_config_readline(it->elem, conf);
		if (ret)
			break;
	}

	lxc_list_for_each(it, defines) {
		lxc_list_del(it);
		free(it);
	}

	return ret;
}

signed long lxc_config_parse_arch(const char *arch)
{
	struct per_name {
		char *name;
		unsigned long per;
	} pername[4] = {
		{ "x86", PER_LINUX32 },
		{ "i686", PER_LINUX32 },
		{ "x86_64", PER_LINUX },
		{ "amd64", PER_LINUX },
	};
	size_t len = sizeof(pername) / sizeof(pername[0]);

	int i;

	for (i = 0; i < len; i++) {
		if (!strcmp(pername[i].name, arch))
		    return pername[i].per;
	}

	return -1;
}

static int lxc_get_conf_int(struct lxc_conf *c, char *retv, int inlen, int v)
{
	if (!retv)
		inlen = 0;
	else
		memset(retv, 0, inlen);
	return snprintf(retv, inlen, "%d", v);
}

static int lxc_get_arch_entry(struct lxc_conf *c, char *retv, int inlen)
{
	int len, fulllen = 0;

	if (!retv)
		inlen = 0;
	else
		memset(retv, 0, inlen);

	switch(c->personality) {
	case PER_LINUX32: strprint(retv, inlen, "x86"); break;
	case PER_LINUX: strprint(retv, inlen, "x86_64"); break;
	default: break;
	}

	return fulllen;
}

/*
 * If you ask for a specific cgroup value, i.e. lxc.cgroup.devices.list,
 * then just the value(s) will be printed.  Since there still could be
 * more than one, it is newline-separated.
 * (Maybe that's ambigous, since some values, i.e. devices.list, will
 * already have newlines?)
 * If you ask for 'lxc.cgroup", then all cgroup entries will be printed,
 * in 'lxc.cgroup.subsystem.key = value' format.
 */
static int lxc_get_cgroup_entry(struct lxc_conf *c, char *retv, int inlen, char *key)
{
	int fulllen = 0, len;
	int all = 0;
	struct lxc_list *it;

	if (!retv)
		inlen = 0;
	else
		memset(retv, 0, inlen);

	if (strcmp(key, "all") == 0)
		all = 1;

	lxc_list_for_each(it, &c->cgroup) {
		struct lxc_cgroup *cg = it->elem;
		if (all) {
			strprint(retv, inlen, "lxc.cgroup.%s = %s\n", cg->subsystem, cg->value);
		} else if (strcmp(cg->subsystem, key) == 0) {
			strprint(retv, inlen, "%s\n", cg->value);
		}
	}
	return fulllen;
}

static int lxc_get_item_hooks(struct lxc_conf *c, char *retv, int inlen, char *key)
{
	char *subkey;
	int len, fulllen = 0, found = -1;
	struct lxc_list *it;
	int i;

	/* "lxc.hook.mount" */
	subkey = index(key, '.');
	if (subkey) subkey = index(subkey+1, '.');
	if (!subkey)
		return -1;
	subkey++;
	if (!*subkey)
		return -1;
	for (i=0; i<NUM_LXC_HOOKS; i++) {
		if (strcmp(lxchook_names[i], subkey) == 0) {
			found=i;
			break;
		}
	}
	if (found == -1)
		return -1;

	if (!retv)
		inlen = 0;
	else
		memset(retv, 0, inlen);

	lxc_list_for_each(it, &c->hooks[found]) {
		strprint(retv, inlen, "%s\n", (char *)it->elem);
	}
	return fulllen;
}

static int lxc_get_item_cap_drop(struct lxc_conf *c, char *retv, int inlen)
{
	int len, fulllen = 0;
	struct lxc_list *it;

	if (!retv)
		inlen = 0;
	else
		memset(retv, 0, inlen);

	lxc_list_for_each(it, &c->caps) {
		strprint(retv, inlen, "%s\n", (char *)it->elem);
	}
	return fulllen;
}

static int lxc_get_mount_entries(struct lxc_conf *c, char *retv, int inlen)
{
	int len, fulllen = 0;
	struct lxc_list *it;

	if (!retv)
		inlen = 0;
	else
		memset(retv, 0, inlen);

	lxc_list_for_each(it, &c->mount_list) {
		strprint(retv, inlen, "%s\n", (char *)it->elem);
	}
	return fulllen;
}

/*
 * lxc.network.0.XXX, where XXX can be: name, type, link, flags, type,
 * macvlan.mode, veth.pair, vlan, ipv4, ipv6, upscript, hwaddr, mtu,
 * ipv4_gateway, ipv6_gateway.  ipvX_gateway can return 'auto' instead
 * of an address.  ipv4 and ipv6 return lists (newline-separated).
 * things like veth.pair return '' if invalid (i.e. if called for vlan
 * type).
 */
static int lxc_get_item_nic(struct lxc_conf *c, char *retv, int inlen, char *key)
{
	char *p1;
	int len, fulllen = 0;
	struct lxc_netdev *netdev;

	if (!retv)
		inlen = 0;
	else
		memset(retv, 0, inlen);

	p1 = index(key, '.');
	if (!p1 || *(p1+1) == '\0') return -1;
	p1++;

	netdev = get_netdev_from_key(key, &c->network);
	if (!netdev)
		return -1;
	if (strcmp(p1, "name") == 0) {
		if (netdev->name)
			strprint(retv, inlen, "%s", netdev->name);
	} else if (strcmp(p1, "type") == 0) {
		strprint(retv, inlen, "%s", lxc_net_type_to_str(netdev->type));
	} else if (strcmp(p1, "link") == 0) {
		if (netdev->link)
			strprint(retv, inlen, "%s", netdev->link);
	} else if (strcmp(p1, "flags") == 0) {
		if (netdev->flags & IFF_UP)
			strprint(retv, inlen, "up");
	} else if (strcmp(p1, "upscript") == 0) {
		if (netdev->upscript)
			strprint(retv, inlen, "%s", netdev->upscript);
	} else if (strcmp(p1, "hwaddr") == 0) {
		if (netdev->hwaddr)
			strprint(retv, inlen, "%s", netdev->hwaddr);
	} else if (strcmp(p1, "mtu") == 0) {
		if (netdev->mtu)
			strprint(retv, inlen, "%s", netdev->mtu);
	} else if (strcmp(p1, "macvlan.mode") == 0) {
		if (netdev->type == LXC_NET_MACVLAN) {
			const char *mode;
			switch (netdev->priv.macvlan_attr.mode) {
			case MACVLAN_MODE_PRIVATE: mode = "private"; break;
			case MACVLAN_MODE_VEPA: mode = "vepa"; break;
			case MACVLAN_MODE_BRIDGE: mode = "bridge"; break;
			default: mode = "(invalid)"; break;
			}
			strprint(retv, inlen, "%s", mode);
		}
	} else if (strcmp(p1, "veth.pair") == 0) {
		if (netdev->type == LXC_NET_VETH && netdev->priv.veth_attr.pair)
			strprint(retv, inlen, "%s", netdev->priv.veth_attr.pair);
	} else if (strcmp(p1, "vlan") == 0) {
		if (netdev->type == LXC_NET_VLAN) {
			strprint(retv, inlen, "%d", netdev->priv.vlan_attr.vid);
		}
	} else if (strcmp(p1, "ipv4_gateway") == 0) {
		if (netdev->ipv4_gateway_auto) {
			strprint(retv, inlen, "auto");
		} else if (netdev->ipv4_gateway) {
			char buf[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, netdev->ipv4_gateway, buf, sizeof(buf));
			strprint(retv, inlen, "%s", buf);
		}
	} else if (strcmp(p1, "ipv4") == 0) {
		struct lxc_list *it2;
		lxc_list_for_each(it2, &netdev->ipv4) {
			struct lxc_inetdev *i = it2->elem;
			char buf[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &i->addr, buf, sizeof(buf));
			strprint(retv, inlen, "%s\n", buf);
		}
	} else if (strcmp(p1, "ipv6_gateway") == 0) {
		if (netdev->ipv6_gateway_auto) {
			strprint(retv, inlen, "auto");
		} else if (netdev->ipv6_gateway) {
			char buf[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, netdev->ipv6_gateway, buf, sizeof(buf));
			strprint(retv, inlen, "%s", buf);
		}
	} else if (strcmp(p1, "ipv6") == 0) {
		struct lxc_list *it2;
		lxc_list_for_each(it2, &netdev->ipv6) {
			struct lxc_inetdev *i = it2->elem;
			char buf[INET_ADDRSTRLEN];
			inet_ntop(AF_INET6, &i->addr, buf, sizeof(buf));
			strprint(retv, inlen, "%s\n", buf);
		}
	}
	return fulllen;
}

static int lxc_get_item_network(struct lxc_conf *c, char *retv, int inlen)
{
	int len, fulllen = 0;
	struct lxc_list *it;

	if (!retv)
		inlen = 0;
	else
		memset(retv, 0, inlen);

	lxc_list_for_each(it, &c->network) {
		struct lxc_netdev *n = it->elem;
		const char *t = lxc_net_type_to_str(n->type);
		strprint(retv, inlen, "%s\n", t ? t : "(invalid)");
	}
	return fulllen;
}

int lxc_get_config_item(struct lxc_conf *c, char *key, char *retv, int inlen)
{
	char *v = NULL;

	if (strcmp(key, "lxc.mount.entry") == 0)
		return lxc_get_mount_entries(c, retv, inlen);
	else if (strcmp(key, "lxc.mount") == 0)
		v = c->fstab;
	else if (strcmp(key, "lxc.tty") == 0)
		return lxc_get_conf_int(c, retv, inlen, c->tty);
	else if (strcmp(key, "lxc.pts") == 0)
		return lxc_get_conf_int(c, retv, inlen, c->pts);
	else if (strcmp(key, "lxc.devttydir") == 0)
		v = c->ttydir;
	else if (strcmp(key, "lxc.arch") == 0)
		return lxc_get_arch_entry(c, retv, inlen);
#if HAVE_APPARMOR
	else if (strcmp(key, "lxc.aa_profile") == 0)
		v = c->aa_profile;
#endif
	else if (strcmp(key, "lxc.cgroup") == 0) // all cgroup info
		return lxc_get_cgroup_entry(c, retv, inlen, "all");
	else if (strncmp(key, "lxc.cgroup.", 11) == 0) // specific cgroup info
		return lxc_get_cgroup_entry(c, retv, inlen, key + 11);
	else if (strcmp(key, "lxc.utsname") == 0)
		v = c->utsname ? c->utsname->nodename : NULL;
	else if (strcmp(key, "lxc.console") == 0)
		v = c->console.path;
	else if (strcmp(key, "lxc.rootfs.mount") == 0)
		v = c->rootfs.mount;
	else if (strcmp(key, "lxc.rootfs") == 0)
		v = c->rootfs.path;
	else if (strcmp(key, "lxc.pivotdir") == 0)
		v = c->rootfs.pivot;
	else if (strcmp(key, "lxc.cap.drop") == 0)
		return lxc_get_item_cap_drop(c, retv, inlen);
	else if (strncmp(key, "lxc.hook", 8) == 0)
		return lxc_get_item_hooks(c, retv, inlen, key);
	else if (strcmp(key, "lxc.network") == 0)
		return lxc_get_item_network(c, retv, inlen);
	else if (strncmp(key, "lxc.network.", 12) == 0)
		return lxc_get_item_nic(c, retv, inlen, key + 12);
	else return -1;

	if (!v)
		return 0;
	if (retv && inlen >= strlen(v) + 1)
		strncpy(retv, v, strlen(v)+1);
	return strlen(v);
}

int lxc_clear_config_item(struct lxc_conf *c, char *key)
{
	if (strcmp(key, "lxc.network") == 0)
		return lxc_clear_config_network(c);
	else if (strncmp(key, "lxc.network.", 12) == 0)
		return lxc_clear_nic(c, key + 12);
	else if (strcmp(key, "lxc.cap.drop") == 0)
		return lxc_clear_config_caps(c);
	else if (strncmp(key, "lxc.cgroup", 10) == 0)
		return lxc_clear_cgroups(c, key);
	else if (strcmp(key, "lxc.mount.entries") == 0)
		return lxc_clear_mount_entries(c);
	else if (strncmp(key, "lxc.hook", 8) == 0)
		return lxc_clear_hooks(c, key);

	return -1;
}

/*
 * writing out a confile.
 */
void write_config(FILE *fout, struct lxc_conf *c)
{
	struct lxc_list *it;
	int i;

	if (c->fstab)
		fprintf(fout, "lxc.mount = %s\n", c->fstab);
	lxc_list_for_each(it, &c->mount_list) {
		fprintf(fout, "lxc.mount.entry = %s\n", (char *)it->elem);
	}
	if (c->tty)
		fprintf(fout, "lxc.tty = %d\n", c->tty);
	if (c->pts)
		fprintf(fout, "lxc.pts = %d\n", c->pts);
	if (c->ttydir)
		fprintf(fout, "lxc.devttydir = %s\n", c->ttydir);
	switch(c->personality) {
	case PER_LINUX32: fprintf(fout, "lxc.arch = x86\n"); break;
	case PER_LINUX: fprintf(fout, "lxc.arch = x86_64\n"); break;
	default: break;
	}
#if HAVE_APPARMOR
	if (c->aa_profile)
		fprintf(fout, "lxc.aa_profile = %s\n", c->aa_profile);
#endif
	lxc_list_for_each(it, &c->cgroup) {
		struct lxc_cgroup *cg = it->elem;
		fprintf(fout, "lxc.cgroup.%s = %s\n", cg->subsystem, cg->value);
	}
	if (c->utsname)
		fprintf(fout, "lxc.utsname = %s\n", c->utsname->nodename);
	lxc_list_for_each(it, &c->network) {
		struct lxc_netdev *n = it->elem;
		const char *t = lxc_net_type_to_str(n->type);
		struct lxc_list *it2;
		fprintf(fout, "lxc.network.type = %s\n", t ? t : "(invalid)");
		if (n->flags & IFF_UP)
			fprintf(fout, "lxc.network.flags = up\n");
		if (n->link)
			fprintf(fout, "lxc.network.link = %s\n", n->link);
		if (n->name)
			fprintf(fout, "lxc.network.name = %s\n", n->name);
		if (n->type == LXC_NET_MACVLAN) {
			const char *mode;
			switch (n->priv.macvlan_attr.mode) {
			case MACVLAN_MODE_PRIVATE: mode = "private"; break;
			case MACVLAN_MODE_VEPA: mode = "vepa"; break;
			case MACVLAN_MODE_BRIDGE: mode = "bridge"; break;
			default: mode = "(invalid)"; break;
			}
			fprintf(fout, "lxc.network.macvlan.mode = %s\n", mode);
		} else if (n->type == LXC_NET_VETH) {
			if (n->priv.veth_attr.pair)
				fprintf(fout, "lxc.network.veth.pair = %s\n",
					n->priv.veth_attr.pair);
		} else if (n->type == LXC_NET_VLAN) {
			fprintf(fout, "lxc.network.vlan.id = %d\n", n->priv.vlan_attr.vid);
		}
		if (n->upscript)
			fprintf(fout, "lxc.network.script.up = %s\n", n->upscript);
		if (n->hwaddr)
			fprintf(fout, "lxc.network.hwaddr = %s\n", n->hwaddr);
		if (n->mtu)
			fprintf(fout, "lxc.network.mtu = %s\n", n->mtu);
		if (n->ipv4_gateway_auto)
			fprintf(fout, "lxc.network.ipv4.gateway = auto\n");
		else if (n->ipv4_gateway) {
			char buf[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, n->ipv4_gateway, buf, sizeof(buf));
			fprintf(fout, "lxc.network.ipv4.gateway = %s\n", buf);
		}
		lxc_list_for_each(it2, &n->ipv4) {
			struct lxc_inetdev *i = it2->elem;
			char buf[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &i->addr, buf, sizeof(buf));
			fprintf(fout, "lxc.network.ipv4 = %s\n", buf);
		}
		if (n->ipv6_gateway_auto)
			fprintf(fout, "lxc.network.ipv6.gateway = auto\n");
		else if (n->ipv6_gateway) {
			char buf[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, n->ipv6_gateway, buf, sizeof(buf));
			fprintf(fout, "lxc.network.ipv6.gateway = %s\n", buf);
		}
		lxc_list_for_each(it2, &n->ipv6) {
			struct lxc_inet6dev *i = it2->elem;
			char buf[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET, &i->addr, buf, sizeof(buf));
			fprintf(fout, "lxc.network.ipv6 = %s\n", buf);
		}
	}
	lxc_list_for_each(it, &c->caps)
		fprintf(fout, "lxc.cap.drop = %s\n", (char *)it->elem);
	for (i=0; i<NUM_LXC_HOOKS; i++) {
		lxc_list_for_each(it, &c->hooks[i])
			fprintf(fout, "lxc.hook.%s = %s\n",
				lxchook_names[i], (char *)it->elem);
	}
	if (c->console.path)
		fprintf(fout, "lxc.console = %s\n", c->console.path);
	if (c->rootfs.path)
		fprintf(fout, "lxc.rootfs = %s\n", c->rootfs.path);
	if (c->rootfs.mount && strcmp(c->rootfs.mount, LXCROOTFSMOUNT) != 0)
		fprintf(fout, "lxc.rootfs.mount = %s\n", c->rootfs.mount);
	if (c->rootfs.pivot)
		fprintf(fout, "lxc.pivotdir = %s\n", c->rootfs.pivot);
}
