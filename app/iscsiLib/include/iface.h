/*
 * iSCSI iface helpers
 *
 * Copyright (C) 2008 Mike Christie
 * Copyright (C) 2008 Red Hat, Inc. All rights reserved.
 * maintained by open-iscsi@@googlegroups.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * See the file COPYING included with this distribution for more details.
 */
#ifndef ISCSI_IFACE_H
#define ISCSI_IFACE_H

#define IFACE_CONFIG_DIR	"ifaces"

struct iface_rec;
struct list_head;

extern void iface_copy(struct iface_rec *dst, struct iface_rec *src);
extern int iface_match(struct iface_rec *pattern, struct iface_rec *iface);
extern struct iface_rec *iface_alloc(char *ifname, int *err);
extern int iface_conf_read(struct iface_rec *iface);
extern void iface_setup_defaults(struct iface_rec *iface);
extern int iface_is_bound_by_hwaddr(struct iface_rec *iface);
extern int iface_is_bound_by_netdev(struct iface_rec *iface);
extern int iface_is_bound_by_ipaddr(struct iface_rec *iface);
typedef int (iface_op_fn)(void *data, struct iface_rec *iface);
extern int iface_for_each_iface(void *data, int *nr_found,
				 iface_op_fn *fn);
extern int iface_print_flat(void *data, struct iface_rec *iface);
extern int iface_print_tree(void *data, struct iface_rec *iface);
extern void iface_setup_host_bindings(void);
extern int iface_get_by_net_binding(struct iface_rec *pattern,
				    struct iface_rec *out_rec);
extern int iface_conf_update(struct db_set_param *set_param,
			     struct iface_rec *iface);
extern int iface_conf_write(struct iface_rec *iface);
extern int iface_conf_delete(struct iface_rec *iface);
extern int iface_is_valid(struct iface_rec *iface);
extern void iface_link_ifaces(struct list_head *ifaces);

#define iface_fmt "[hw=%s,ip=%s,net_if=%s,iscsi_if=%s]"
#define iface_str(_iface) \
	(_iface)->hwaddress, (_iface)->ipaddress, (_iface)->netdev, \
	(_iface)->name

#endif
