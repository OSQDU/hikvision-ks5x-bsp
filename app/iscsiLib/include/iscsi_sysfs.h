/*
 * iSCSI sysfs
 *
 * Copyright (C) 2006 Mike Christie
 * Copyright (C) 2006 Red Hat, Inc. All rights reserved.
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
 */
#ifndef ISCSI_SYSFS_H
#define ISCSI_SYSFS_H

#include <sys/types.h>
#include <dirent.h>

#include "sysfs.h"
#include "types.h"
#include "iscsi_proto.h"
#include "config.h"

struct iscsi_session;
struct iscsi_conn;
struct iscsi_session_operational_config;
struct iscsi_conn_operational_config;
struct iscsi_auth_config;

#define SCSI_MAX_STATE_VALUE 32

struct session_info {
    struct list_head list;
    /* local info */
    struct iface_rec iface;
    int sid;

    /* remote info */
    char targetname[TARGET_NAME_MAXLEN + 1];
    int tpgt;
    char address[NI_MAXHOST + 1];
    int port;
    char persistent_address[NI_MAXHOST + 1];
    int persistent_port;
};

struct host_info {
    struct iface_rec iface;
    int host_no;
};

extern int direntcmp(const struct dirent **d1, const struct dirent **d2);
extern void free_transports(void);
extern char *iscsi_sysfs_get_iscsi_kernel_version(void);
extern int iscsi_sysfs_check_class_version(void);
extern int iscsi_sysfs_get_sessioninfo_by_id(struct session_info *info,
                         char *sys_session);
extern int iscsi_sysfs_session_has_leadconn(uint32_t sid);

typedef int (iscsi_sysfs_session_op_fn)(void *, struct session_info *);
typedef int (iscsi_sysfs_host_op_fn)(void *, struct host_info *);

extern int iscsi_sysfs_for_each_session(void *data, int *nr_found,
                    iscsi_sysfs_session_op_fn *fn);
extern int iscsi_sysfs_for_each_host(void *data, int *nr_found,
                     iscsi_sysfs_host_op_fn *fn);
extern uint32_t iscsi_sysfs_get_host_no_from_sid(uint32_t sid, int *err);
extern uint32_t iscsi_sysfs_get_host_no_from_iface(struct iface_rec *iface,
                           int *rc);
extern int iscsi_sysfs_get_sid_from_path(char *session);
extern char *iscsi_sysfs_get_blockdev_from_lun(int hostno, int target, int sid);

static inline int is_valid_operational_value(int value)
{
    return value != -1;
}

extern void iscsi_sysfs_get_auth_conf(int sid, struct iscsi_auth_config *conf);
extern void iscsi_sysfs_get_negotiated_session_conf(int sid,
                struct iscsi_session_operational_config *conf);
extern void iscsi_sysfs_get_negotiated_conn_conf(int sid,
                struct iscsi_conn_operational_config *conf);
extern pid_t iscsi_sysfs_scan_host(int hostno, int async);
extern int iscsi_sysfs_get_session_state(char *state, int sid);
extern int iscsi_sysfs_get_host_state(char *state, int host_no);
extern int iscsi_sysfs_get_device_state(char *state, int host_no, int target,
                    int lun);
extern int iscsi_sysfs_get_exp_statsn(int sid);
extern void iscsi_sysfs_set_device_online(int hostno, int target, int lun);
extern void iscsi_sysfs_rescan_device(int hostno, int target, int lun);
extern int iscsi_sysfs_for_each_device(int host_no, uint32_t sid,
                void (* fn)(int host_no, int target, int lun));
extern struct iscsi_transport *iscsi_sysfs_get_transport_by_hba(long host_no);
extern struct iscsi_transport *iscsi_sysfs_get_transport_by_session(char *sys_session);
extern struct iscsi_transport *iscsi_sysfs_get_transport_by_sid(uint32_t sid);
extern struct iscsi_transport *iscsi_sysfs_get_transport_by_name(char *transport_name);

extern struct list_head transports;
extern uint32_t get_target_no_from_sid(uint32_t sid, int *err);

#endif
