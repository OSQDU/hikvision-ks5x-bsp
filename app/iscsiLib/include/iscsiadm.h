/*
 * iSCSI Administration Utility
 *
 * Copyright (C) 2004 Dmitry Yusupov, Alex Aizman
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
#ifndef ISCSIADM_H
#define ISCSIADM_H

#include "types.h"
#include "strings.h"
#include "config.h"

/* discovery.c */
struct discovery_rec;
struct list_head;
enum iscsiadm_mode {
	MODE_DISCOVERY,
	MODE_NODE,
	MODE_SESSION,
	MODE_HOST,
	MODE_IFACE,
	MODE_FW,
};

enum iscsiadm_op {
	OP_NOOP		= 0x0,
	OP_NEW		= 0x1,
	OP_DELETE	= 0x2,
	OP_UPDATE	= 0x4,
	OP_SHOW		= 0x8,
};

extern int discovery_sendtargets(struct discovery_rec *drec,
				 struct list_head *rec_list);
extern int discovery_offload_sendtargets(int host_no, int do_login,
					 discovery_rec_t *drec);
extern char *get_config_file(void);char *get_config_file(void);

#endif /* ISCSIADM_H */
