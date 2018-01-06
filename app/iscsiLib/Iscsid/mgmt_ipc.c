/*
 * iSCSI Administrator Utility Socket Interface
 *
 * Copyright (C) 2004 Dmitry Yusupov, Alex Aizman
 * Copyright (C) 2006 Mike Christie
 * Copyright (C) 2006 Red Hat, Inc. All rights reserved.
 * maintained by open-iscsi@googlegroups.com
 *
 * Originally based on:
 * (C) 2004 FUJITA Tomonori <tomof@acm.org>
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
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdio.h>

#include "iscsid.h"
#include "idbm.h"
#include "mgmt_ipc.h"
#include "iscsi_ipc.h"
#include "log.h"
#include "transport.h"
#include "sysfs.h"
#include "iscsi_sysfs.h"
#include "../include/iscsiadm.h"

static int  leave_event_loop = 0;

#define PEERUSER_MAX    64
#define EXTMSG_MAX  (64 * 1024)
#ifndef  HPR_INFINITE
#define HPR_INFINITE 0xFFFFFFFF
#endif

struct session_info g_info ;

int
mgmt_ipc_listen(void)
{
    int fd, err;
    struct sockaddr_un addr;

    fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (fd < 0) {
        log_error("Can not create IPC socket");
        return fd;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    memcpy((char *) &addr.sun_path + 1, ISCSIADM_NAMESPACE,
        strlen(ISCSIADM_NAMESPACE));

    if ((err = bind(fd, (struct sockaddr *) &addr, sizeof(addr))) < 0) {
        log_error("Can not bind IPC socket");
        close(fd);
        return err;
    }

    if ((err = listen(fd, 32)) < 0) {
        log_error("Can not listen IPC socket");
        close(fd);
        return err;
    }

    return fd;
}

void
mgmt_ipc_close(int fd)
{
    leave_event_loop = 1;
    if (fd >= 0)
        close(fd);
}

static mgmt_ipc_err_e
mgmt_ipc_session_login(queue_task_t *qtask)
{
    return session_login_task(&qtask->req.u.session.rec, qtask);
}

static mgmt_ipc_err_e
mgmt_ipc_session_getstats(queue_task_t *qtask)
{
    int sid = qtask->req.u.session.sid;
    iscsi_session_t *session;
    int rc;

    if (!(session = session_find_by_sid(sid)))
        return MGMT_IPC_ERR_NOT_FOUND;

    rc = ipc->get_stats(session->t->handle,
        session->id, session->conn[0].id,
        (void *)&qtask->rsp.u.getstats,
        MGMT_IPC_GETSTATS_BUF_MAX);
    if (rc) {
        log_error("get_stats(): IPC error %d "
            "session [%02d]", rc, sid);
        return MGMT_IPC_ERR_INTERNAL;
    }

    mgmt_ipc_write_rsp(qtask, MGMT_IPC_OK);
    return MGMT_IPC_OK;
}

static mgmt_ipc_err_e
mgmt_ipc_send_targets(queue_task_t *qtask)
{
    iscsiadm_req_t *req = &qtask->req;
    mgmt_ipc_err_e err;

    err = iscsi_host_send_targets(qtask, req->u.st.host_no,
                      req->u.st.do_login,
                      &req->u.st.ss);
    mgmt_ipc_write_rsp(qtask, err);
    return MGMT_IPC_OK;
}

static mgmt_ipc_err_e
mgmt_ipc_session_logout(queue_task_t *qtask)
{
    return session_logout_task(qtask->req.u.session.sid, qtask);
}

static mgmt_ipc_err_e
mgmt_ipc_session_sync(queue_task_t *qtask)
{
    struct ipc_msg_session *session= &qtask->req.u.session;

    return iscsi_sync_session(&session->rec, qtask, session->sid);
}

static mgmt_ipc_err_e
mgmt_ipc_cfg_initiatorname(queue_task_t *qtask)
{
    if (dconfig->initiator_name)
        strcpy(qtask->rsp.u.config.var, dconfig->initiator_name);
    mgmt_ipc_write_rsp(qtask, MGMT_IPC_OK);
    return MGMT_IPC_OK;
}

static mgmt_ipc_err_e
mgmt_ipc_session_info(queue_task_t *qtask)
{
    int sid = qtask->req.u.session.sid;
    iscsi_session_t *session;
    struct ipc_msg_session_state *info;

    if (!(session = session_find_by_sid(sid))) {
        log_debug(1, "session with sid %d not found!", sid);
        return MGMT_IPC_ERR_NOT_FOUND;
    }

    info = &qtask->rsp.u.session_state;
    info->conn_state = session->conn[0].state;
    info->session_state = session->r_stage;

    mgmt_ipc_write_rsp(qtask, MGMT_IPC_OK);
    return MGMT_IPC_OK;
}

static mgmt_ipc_err_e
mgmt_ipc_cfg_initiatoralias(queue_task_t *qtask)
{
    strcpy(qtask->rsp.u.config.var, dconfig->initiator_alias);
    mgmt_ipc_write_rsp(qtask, MGMT_IPC_OK);
    return MGMT_IPC_OK;
}

static mgmt_ipc_err_e
mgmt_ipc_cfg_filename(queue_task_t *qtask)
{
    strcpy(qtask->rsp.u.config.var, dconfig->config_file);
    mgmt_ipc_write_rsp(qtask, MGMT_IPC_OK);
    return MGMT_IPC_OK;
}

static mgmt_ipc_err_e
mgmt_ipc_conn_add(queue_task_t *qtask)
{
    return MGMT_IPC_ERR;
}

static mgmt_ipc_err_e
mgmt_ipc_immediate_stop(queue_task_t *qtask)
{
    leave_event_loop = 1;
    mgmt_ipc_write_rsp(qtask, MGMT_IPC_OK);
    return MGMT_IPC_OK;
}

static mgmt_ipc_err_e
mgmt_ipc_conn_remove(queue_task_t *qtask)
{
    return MGMT_IPC_ERR;
}

static mgmt_ipc_err_e
mgmt_ipc_isns_dev_attr_query(queue_task_t *qtask)
{
    return 0;
}


static mgmt_ipc_err_e
mgmt_ipc_host_set_param(queue_task_t *qtask)
{
    struct ipc_msg_set_host_param *hp = &qtask->req.u.set_host_param;
    int err;

    err = iscsi_host_set_param(hp->host_no, hp->param, hp->value);
    mgmt_ipc_write_rsp(qtask, err);
    return MGMT_IPC_OK;
}

/*
 * Parse a list of strings, encoded as a 32bit
 * length followed by the string itself (not necessarily
 * NUL-terminated).
 */
static int
mgmt_ipc_parse_strings(queue_task_t *qtask, char ***result)
{
    char        *data, *endp, **argv = NULL;
    unsigned int    left, argc;

again:
    data = qtask->payload;
    left = qtask->req.payload_len;
    endp = NULL;
    argc = 0;

    while (left) {
        uint32_t len;

        if (left < 4)
            return -1;
        memcpy(&len, data, 4);
        data += 4;

        if (endp)
            *endp = '\0';

        if (len > left)
            return -1;

        if (argv) {
            argv[argc] = (char *) data;
            endp = data + len;
        }
        data += len;
        argc++;
    }

    if (endp)
        *endp = '\0';

    if (argv == NULL) {
        argv = malloc((argc + 1) * sizeof(char *));
        *result = argv;
        goto again;
    }

    argv[argc] = NULL;
    return argc;
}

static mgmt_ipc_err_e
mgmt_ipc_notify_common(queue_task_t *qtask,
        mgmt_ipc_err_e (*handler)(int, char **))
{
    char    **argv = NULL;
    int argc, err = MGMT_IPC_ERR;

    argc = mgmt_ipc_parse_strings(qtask, &argv);
    if (argc > 0)
        err = handler(argc, argv);

    if (argv)
        free(argv);
    mgmt_ipc_write_rsp(qtask, err);
    return MGMT_IPC_OK;
}

/* Replace these dummies as you implement them
   elsewhere */
static mgmt_ipc_err_e
iscsi_discovery_add_node(int argc, char **argv)
{
    return MGMT_IPC_OK;
}

static mgmt_ipc_err_e
iscsi_discovery_del_node(int argc, char **argv)
{
    return MGMT_IPC_OK;
}

static mgmt_ipc_err_e
iscsi_discovery_add_portal(int argc, char **argv)
{
    return MGMT_IPC_OK;
}

static mgmt_ipc_err_e
iscsi_discovery_del_portal(int argc, char **argv)
{
    return MGMT_IPC_OK;
}

static mgmt_ipc_err_e
mgmt_ipc_notify_add_node(queue_task_t *qtask)
{
    return mgmt_ipc_notify_common(qtask, iscsi_discovery_add_node);
}

static mgmt_ipc_err_e
mgmt_ipc_notify_del_node(queue_task_t *qtask)
{
    return mgmt_ipc_notify_common(qtask, iscsi_discovery_del_node);
}

static mgmt_ipc_err_e
mgmt_ipc_notify_add_portal(queue_task_t *qtask)
{
    return mgmt_ipc_notify_common(qtask, iscsi_discovery_add_portal);
}

static mgmt_ipc_err_e
mgmt_ipc_notify_del_portal(queue_task_t *qtask)
{
    return mgmt_ipc_notify_common(qtask, iscsi_discovery_del_portal);
}

static int
mgmt_peeruser(int sock, char *user)
{
#if defined(SO_PEERCRED)
    /* Linux style: use getsockopt(SO_PEERCRED) */
    struct ucred peercred;
    socklen_t so_len = sizeof(peercred);
    struct passwd *pass;

    errno = 0;
    if (getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &peercred,
        &so_len) != 0 || so_len != sizeof(peercred)) {
        /* We didn't get a valid credentials struct. */
        log_error("peeruser_unux: error receiving credentials: %m");
        return 0;
    }

    pass = getpwuid(peercred.uid);
    if (pass == NULL) {
        log_error("peeruser_unix: unknown local user with uid %d",
                (int) peercred.uid);
        return 0;
    }

    strncpy(user, pass->pw_name, PEERUSER_MAX);
    return 1;

#elif defined(SCM_CREDS)
    struct msghdr msg;
    typedef struct cmsgcred Cred;
#define cruid cmcred_uid
    Cred *cred;

    /* Compute size without padding */
    /* for NetBSD */
    char cmsgmem[_ALIGN(sizeof(struct cmsghdr)) + _ALIGN(sizeof(Cred))];

    /* Point to start of first structure */
    struct cmsghdr *cmsg = (struct cmsghdr *) cmsgmem;

    struct iovec iov;
    char buf;
    struct passwd *pw;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = (char *) cmsg;
    msg.msg_controllen = sizeof(cmsgmem);
    memset(cmsg, 0, sizeof(cmsgmem));

    /*
     * The one character which is received here is not meaningful; its
     * purposes is only to make sure that recvmsg() blocks long enough for
     * the other side to send its credentials.
     */
    iov.iov_base = &buf;
    iov.iov_len = 1;

    if (recvmsg(sock, &msg, 0) < 0 || cmsg->cmsg_len < sizeof(cmsgmem) ||
            cmsg->cmsg_type != SCM_CREDS) {
        log_error("ident_unix: error receiving credentials: %m");
        return 0;
    }

    cred = (Cred *) CMSG_DATA(cmsg);

    pw = getpwuid(cred->cruid);
    if (pw == NULL) {
        log_error("ident_unix: unknown local user with uid %d",
                (int) cred->cruid);
        return 0;
    }

    strncpy(user, pw->pw_name, PEERUSER_MAX);
    return 1;

#else
    log_error("'mgmg_ipc' auth is not supported on local connections "
        "on this platform");
    return 0;
#endif
}

static void
mgmt_ipc_destroy_queue_task(queue_task_t *qtask)
{
    if (qtask->mgmt_ipc_fd >= 0)
        close(qtask->mgmt_ipc_fd);
    if (qtask->payload)
        free(qtask->payload);
    if (qtask->allocated)
        free(qtask);
}

/*
 * Send the IPC response and destroy the queue_task.
 * The recovery code uses a qtask which is allocated as
 * part of a larger structure, and we don't want it to
 * get freed when we come here. This is what qtask->allocated
 * is for.
 */
void
mgmt_ipc_write_rsp(queue_task_t *qtask, mgmt_ipc_err_e err)
{
    if (!qtask)
        return;
    log_debug(4, "%s: rsp to fd %d", __FUNCTION__,
         qtask->mgmt_ipc_fd);

    if (qtask->mgmt_ipc_fd < 0) {
        mgmt_ipc_destroy_queue_task(qtask);
        return;
    }

    qtask->rsp.err = err;
    write(qtask->mgmt_ipc_fd, &qtask->rsp, sizeof(qtask->rsp));
    close(qtask->mgmt_ipc_fd);
    mgmt_ipc_destroy_queue_task(qtask);
}

static int
mgmt_ipc_read_data(int fd, void *ptr, size_t len)
{
    int n;

    while (len) {
        n = read(fd, ptr, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -EIO;
        }
        if (n == 0) {
            /* Client closed connection */
            return -EIO;
        }
        ptr += n;
        len -= n;
    }
    return 0;
}

static int
mgmt_ipc_read_req(queue_task_t *qtask)
{
    iscsiadm_req_t *req = &qtask->req;
    int rc;

    rc = mgmt_ipc_read_data(qtask->mgmt_ipc_fd, req, sizeof(*req));
    if (rc >= 0 && req->payload_len > 0) {
        /* Limit what we accept */
        if (req->payload_len > EXTMSG_MAX)
            return -EIO;

        /* Remember the allocated pointer in the
         * qtask - it will be freed by write_rsp.
         * Note: we allocate one byte in excess
         * so we can append a NUL byte. */
        qtask->payload = malloc(req->payload_len + 1);
        rc = mgmt_ipc_read_data(qtask->mgmt_ipc_fd,
                qtask->payload,
                req->payload_len);
    }
    return rc;
}

static mgmt_ipc_fn_t *  mgmt_ipc_functions[__MGMT_IPC_MAX_COMMAND] = {
[MGMT_IPC_SESSION_LOGIN]    = mgmt_ipc_session_login,
[MGMT_IPC_SESSION_LOGOUT]   = mgmt_ipc_session_logout,
[MGMT_IPC_SESSION_SYNC]     = mgmt_ipc_session_sync,
[MGMT_IPC_SESSION_STATS]    = mgmt_ipc_session_getstats,
[MGMT_IPC_SEND_TARGETS]     = mgmt_ipc_send_targets,
[MGMT_IPC_SESSION_INFO]     = mgmt_ipc_session_info,
[MGMT_IPC_CONN_ADD]     = mgmt_ipc_conn_add,
[MGMT_IPC_CONN_REMOVE]      = mgmt_ipc_conn_remove,
[MGMT_IPC_CONFIG_INAME]     = mgmt_ipc_cfg_initiatorname,
[MGMT_IPC_CONFIG_IALIAS]    = mgmt_ipc_cfg_initiatoralias,
[MGMT_IPC_CONFIG_FILE]      = mgmt_ipc_cfg_filename,
[MGMT_IPC_IMMEDIATE_STOP]   = mgmt_ipc_immediate_stop,
[MGMT_IPC_ISNS_DEV_ATTR_QUERY]  = mgmt_ipc_isns_dev_attr_query,
[MGMT_IPC_SET_HOST_PARAM]   = mgmt_ipc_host_set_param,
[MGMT_IPC_NOTIFY_ADD_NODE]  = mgmt_ipc_notify_add_node,
[MGMT_IPC_NOTIFY_DEL_NODE]  = mgmt_ipc_notify_del_node,
[MGMT_IPC_NOTIFY_ADD_PORTAL]    = mgmt_ipc_notify_add_portal,
[MGMT_IPC_NOTIFY_DEL_PORTAL]    = mgmt_ipc_notify_del_portal,
};

static void
mgmt_ipc_handle(int accept_fd)
{
    unsigned int command;
    int fd, err;
    queue_task_t *qtask = NULL;
    mgmt_ipc_fn_t *handler = NULL;
    char user[PEERUSER_MAX];

    qtask = calloc(1, sizeof(queue_task_t));
    if (!qtask)
        return;

    if ((fd = accept(accept_fd, NULL, NULL)) < 0) {
        free(qtask);
        return;
    }

    qtask->allocated = 1;
    qtask->mgmt_ipc_fd = fd;

    if (!mgmt_peeruser(fd, user) || strncmp(user, "root", PEERUSER_MAX)) {
        err = MGMT_IPC_ERR_ACCESS;
        goto err;
    }

    if (mgmt_ipc_read_req(qtask) < 0) {
        mgmt_ipc_destroy_queue_task(qtask);
        return;
    }

    command = qtask->req.command;
    qtask->rsp.command = command;

    if (0 <= command && command < __MGMT_IPC_MAX_COMMAND)
        handler = mgmt_ipc_functions[command];
    if (handler != NULL) {
        /* If the handler returns OK, this means it
         * already sent the reply. */
        err = handler(qtask);
        if (err == MGMT_IPC_OK)
            return;
    } else {
        log_error("unknown request: %s(%d) %u",
              __FUNCTION__, __LINE__, command);
        err = MGMT_IPC_ERR_INVALID_REQ;
    }

err:
    /* This will send the response, close the
     * connection and free the qtask */
    mgmt_ipc_write_rsp(qtask, err);
}

static int reap_count;

void
need_reap(void)
{
    reap_count++;
}

static void
reaper(void)
{
    int rc;

    /*
     * We don't really need reap_count, but calling wait() all the
     * time seems execessive.
     */
    if (reap_count) {
        rc = waitpid(0, NULL, WNOHANG);
        if (rc > 0) {
            reap_count--;
            log_debug(6, "reaped pid %d, reap_count now %d",
                  rc, reap_count);
        }
    }
}

/*
 * Function: setPthreadAttr
 * Description: set the pthread's attribute: priority and the stack size in details
 * Input:    priority - [minPriority, maxPriority] 
 *            stacksize - the pthread's stack size
 * Output:  attr - the pthread's attribute
 * Return:  0 if successful, otherwise return -1
 *
 */
 int set_pthread_attr
(
    pthread_attr_t *attr, 
    int priority, 
    size_t stacksize
)
{
    int rval;
    struct sched_param    params;

    rval = pthread_attr_init(attr);
    if (rval != 0)
    {
        printf("set_pthread_attr error %d\n",__LINE__);
        return rval;
    }

    /* normally, need not to set */

    /* use the round robin scheduling algorithm */
    rval = pthread_attr_setschedpolicy(attr, SCHED_RR);
    if (rval != 0)
    {
        pthread_attr_destroy(attr);
        printf("set_pthread_attr error %d\n",__LINE__);
        return rval;
    }

    /* set the thread to be detached */
    rval = pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED);
    if (rval != 0)
    {
        pthread_attr_destroy(attr);
        printf("set_pthread_attr error %d\n",__LINE__);
        return rval;
    }

    /* first get the scheduling parameter, then set the new priority */
    rval = pthread_attr_getschedparam(attr, &params);
    if (rval != 0)
    {
        pthread_attr_destroy(attr);
        printf("set_pthread_attr error %d\n",__LINE__);
        return rval;
    }
    if (rval != 0)
    {
        pthread_attr_destroy(attr);
        printf("set_pthread_attr error %d\n",__LINE__);
        return rval;
    }
    params.sched_priority = priority;
    rval = pthread_attr_setschedparam(attr, &params);
    if (rval != 0)
    {
        pthread_attr_destroy(attr);
        printf("set_pthread_attr error %d\n",__LINE__);
        return rval;
    }

    /* when set stack size, we define a minmum value to avoid fail */
    rval = pthread_attr_setstacksize(attr, stacksize);
    if (rval != 0)
    {
        pthread_attr_destroy(attr);
        printf("set_pthread_attr error %d\n",__LINE__);
        return rval;
    }

    return 0;
}

/* @fn  unsigned int iscsi_util_get_check_sum (const void *pBuf, unsigned int iLen)
 *
 *  @brief             计算校验和
 *  @return          HPR_INFINITE 失败，否则返回
 *                 校验和
 */
static unsigned int iscsi_get_check_sum (const void *pBuf, unsigned int iLen)
{
    unsigned int iCheckSum = 0;
    unsigned char   *pChar = NULL;

    if (NULL == pBuf)
    {
        printf ("input error.\n");
        return HPR_INFINITE;
    }

    pChar = (unsigned char *)pBuf;
    while (iLen>0) 
    {
        iCheckSum += (*pChar);
        pChar++;
        iLen--;
    }
    return iCheckSum;
}

/* @fn      iscsi_create_sock
* @brief    创建iscsid相关域套接字并建立连接
* @param    pFd 套接字描述符 
* @return     成功: 0 失败: <0
*/
static int  iscsi_create_sock(int *pFd)
{
    //创建套接字
    int iErr = 0;
    struct sockaddr_un struAddr;
    memset(&g_info,0,sizeof(g_info));
    g_info.sid = -1 ;
    if(pFd == NULL)
    {        
        printf("iFd error\n");
        return -1 ;
    }
    *pFd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (*pFd < 0) {
      printf("Can not create iscsid socket\n");
      return *pFd;
    }

    memset(&struAddr, 0, sizeof(struAddr));
    struAddr.sun_family = AF_LOCAL;
    memcpy((char *) &struAddr.sun_path + 1, ISCSID_NAMESPACE,
      strlen(ISCSID_NAMESPACE));

    if ((iErr = bind(*pFd, (struct sockaddr *) &struAddr, sizeof(struAddr))) < 0)
    {
      printf("Can not bind iscsid socket\n");
      close(*pFd);
      return iErr;
    }

    if ((iErr = listen(*pFd, 32)) < 0) 
    {
      printf("Can not listen iscsid socket\n");
      close(*pFd);
      return iErr;
    }
    return 0;    
}
/* @fn      iscsi_operate
* @brief    创建套接字并等待新的连接
* @param    无
* @return     成功: 0 失败: <0
*/
int  iscsi_operate(void)
{    
    int iRet = 0;
    int fd  = 0;
    int iFd = 0;
    pthread_t iPid = 0;  
    g_info.sid = -1;      
    pthread_attr_t attr;
    int iSize = 256<<10 ;//最小256K
    int priority=40;    
    if (0 != set_pthread_attr(&attr, priority, iSize))  
    {
        return -1;
    }
    iRet = iscsi_create_sock(&iFd);
    if(0 != iRet)
    {
        printf("iscsi_create_sock faild errno%d(%s)\n",errno,strerror(errno));
        return iRet; 
    }
    while(1)
    {        
        if ((fd = accept(iFd, NULL, NULL)) < 0) 
        {
            printf("iscsid accept error fd %d %s\n",fd,strerror(errno));        
            continue ;
        }
        printf("iscsid accepted \n");
        iRet = pthread_create(&iPid,&attr,iscsi_util_handle,fd);
        if(0 != iRet)
        {
            printf("iscsid pthread_create faild errno%d(%s)\n",errno,strerror(errno));
            break ;
        }
    }
    return -1 ;
}
/* @fn      iscsi_util_recvcmd
* @brief    接收IPSAN发送的命令及数据
* @param[in]    iFd 套接字描述符               
* @param[out]    pstruRcv  接受的数据
* @return     成功: 0 失败: <0
*/
int iscsi_util_recvcmd(int iFd,ISCSID_UTIL_REQ_T *pstruRcv)
{
    int iErr = 0;
    if(pstruRcv == NULL)
    {       
        printf("iscsi_util_recvcmd arg error\n");
        return -1;
    }
    memset(pstruRcv,0,sizeof(ISCSID_UTIL_REQ_T));
    if ((iErr = recv(iFd, pstruRcv, sizeof(ISCSID_UTIL_REQ_T), MSG_WAITALL)) != sizeof(ISCSID_UTIL_REQ_T)) 
    {
        printf("got recv error (%d/%d), client may not send data\n", iErr, errno);
        return iErr;
    } 
    if(pstruRcv->iCheckSum != iscsi_get_check_sum(&(pstruRcv->u),pstruRcv->uPayloadLen))
    {
        printf("iscsid checksum error iCheckSum%d.checked %d\n",pstruRcv->iCheckSum , iscsi_get_check_sum(&(pstruRcv->u),pstruRcv->uPayloadLen));
        return 0 ;
    }
    return iErr;
}
/* @fn      iscsi_util_anscmd
* @brief    发送给IPSAN的命令及数据
* @param[in]    iFd 套接字描述符               
* @param[in]    pstruSnd  待发送的数据
* @return     成功: 0 失败: <0
*/
int iscsi_util_anscmd(int iFd, ISCSID_UTIL_RSP_T *pstruSnd)
{
    int iErr;
    if(pstruSnd == NULL)
    {       
        printf("iscsi_util_anscmd arg error\n");
        return -1;
    } 
    pstruSnd->iCheckSum = iscsi_get_check_sum(&(pstruSnd->u),pstruSnd->uPayloadLen);
    if ((iErr = write(iFd, pstruSnd, sizeof(ISCSID_UTIL_RSP_T))) != sizeof(ISCSID_UTIL_RSP_T)) 
    {
        printf("got write error (%d/%d) on cmd %d, client may not work\n",
            iErr, errno, pstruSnd->enumCommand);
        return -1;
    }
    return 0;
}

/* @fn      iscsi_util_recv_and_send
* @brief    接收IPSAN发来的的命令，处理并返回
* @param[in]    iFd 套接字描述符               
* @param[in]    pstruSnd  待发送的数据
* @param[in]    pstruSnd  待发送的数据
* @return     成功: 对应函数的返回值 失败:HPR_INFINITE
*/
int iscsi_util_recv_and_send(int iFd,ISCSID_UTIL_REQ_T *pstruRcv,ISCSID_UTIL_RSP_T *pstruSnd)
{
    int iRes = 0;
    char state[16];
    int iErr = 0;
    char acCmdString[MAX_NAME] = {0} ;
    
    struct session_info info ;
    bzero(state, sizeof(state));
    if((pstruRcv == NULL)||(pstruSnd == NULL))
    {       
        printf("iscsi_util_recv_and_send arg error\n");
        return HPR_INFINITE;
    }   
    iRes = iscsi_util_recvcmd(iFd,pstruRcv);
    if(iRes < 0 )
    {
        printf("iscsi_util_recv_and_send  recv error  iRes=%d\n",iRes);
        return HPR_INFINITE ;
    }
    if(iRes == 0)
    {
        return IPSAN_NEEDRESEND ;
    }
    switch (pstruRcv->enumCommand)
    {
        case ISCSID_UTIL_SYSFS_INIT :
            if(pstruRcv->uPayloadLen != 0)
            {
                iRes = IPSAN_NEEDRESEND ;
            }
            else
            {
                iRes = sysfs_init();
            }
            pstruSnd->enumCommand = ISCSID_UTIL_SYSFS_INIT ;
            pstruSnd->uPayloadLen = 0 ;
            pstruSnd->iSockRet = iRes ;
            break ;
        case ISCSID_UTIL_GET_SESSION_STATE:
            if(pstruRcv->uPayloadLen != 0)
            {
                iRes = IPSAN_NEEDRESEND ;
            }
            else
            {
                memcpy(state , pstruRcv->u.struSession.acState ,sizeof(state));
                g_info.sid = pstruRcv->u.struSession.iSid ;
                iRes = iscsi_sysfs_get_session_state(state, g_info.sid);
              
            }
            pstruSnd->enumCommand = ISCSID_UTIL_GET_SESSION_STATE ;
            pstruSnd->uPayloadLen = sizeof(pstruSnd->u.struSession) ;
            pstruSnd->iSockRet = iRes ;
            pstruSnd->u.struSession.iSid = g_info.sid ;
            memcpy(pstruSnd->u.struSession.acState ,state,sizeof(pstruSnd->u.struSession.acState));
            break; 
        case ISCSID_UTIL_GET_SESSIONINFO_BY_ID:
            if(pstruRcv->uPayloadLen != strlen((char *)pstruRcv->u.acDirName))
            {
                iRes = IPSAN_NEEDRESEND ;
            }
            else
            {
                iRes = iscsi_sysfs_get_sessioninfo_by_id(&g_info, (char *)pstruRcv->u.acDirName);
            }
            pstruSnd->enumCommand = ISCSID_UTIL_GET_SESSIONINFO_BY_ID ; 
            pstruSnd->uPayloadLen = sizeof(pstruSnd->u.struSessionInfoRsp) ;
            pstruSnd->iSockRet = iRes ;
            memcpy(pstruSnd->u.struSessionInfoRsp.acTargetName,g_info.targetname, sizeof(g_info.targetname));
            memcpy(pstruSnd->u.struSessionInfoRsp.acAddress,g_info.address, sizeof(g_info.address));
            pstruSnd->u.struSessionInfoRsp.sid = g_info.sid ;
            break ;
        case ISCSID_UTIL_GET_HOST_NO_FROM_SID:
            if(pstruRcv->uPayloadLen != sizeof(pstruRcv->u.struTargetParam))
            {
                iRes = IPSAN_NEEDRESEND ;
            }
            else
            {
                iRes = iscsi_sysfs_get_host_no_from_sid(pstruRcv->u.struTargetParam.iSid, &iErr);
            }
            pstruSnd->enumCommand = ISCSID_UTIL_GET_HOST_NO_FROM_SID ;
            pstruSnd->u.iErr = iErr ;
            pstruSnd->uPayloadLen = sizeof( pstruSnd->u.iErr ) ;
            pstruSnd->iSockRet = iRes ;
            

            break ;            
        case ISCSID_UTIL_GET_TARGET_NO_FROM_SID:
            if(pstruRcv->uPayloadLen != sizeof(pstruRcv->u.struTargetParam))
            {
                iRes = IPSAN_NEEDRESEND ;
            }
            else
            {
                iRes = get_target_no_from_sid((unsigned int)pstruRcv->u.struTargetParam.iSid, &iErr);
            }
            pstruSnd->enumCommand = ISCSID_UTIL_GET_TARGET_NO_FROM_SID ;
            pstruSnd->u.iErr = iErr ;
            pstruSnd->uPayloadLen = sizeof( pstruSnd->u.iErr ) ;
            pstruSnd->iSockRet = iRes ;

            break ; 
        case ISCSID_UTIL_KILL_CLIENT :
            if(pstruRcv->uPayloadLen != 0)
            {
                iRes = IPSAN_NEEDRESEND ;
            }
            else
            {
                g_info.sid = -1 ;
                iRes = ISCSID_NEED_CANCLE;
            }
            pstruSnd->enumCommand = ISCSID_UTIL_KILL_CLIENT ;
            pstruSnd->uPayloadLen = 0 ;
            pstruSnd->iSockRet = iRes ;
            break;
        default:      
            iRes = IPSAN_NEEDRESEND ;         
    }
    iRes = iscsi_util_anscmd(iFd,pstruSnd);
    if(iRes != 0)
    {
        printf("iscsi_util_send_and_recv falid iRes=%d\n",iRes);
        return  HPR_INFINITE;
    }
    return pstruSnd->iSockRet ;

}

/* @fn      iscsi_util_handle
* @brief    等待命令并处理
* @param[in]    accept_fd accept返回的读写描述符               
* @return     成功: >=0 失败: <0
*/
int iscsi_util_handle(int accept_fd)
{
    int iRet;
    ISCSID_UTIL_REQ_T struRcv;
    ISCSID_UTIL_RSP_T struSnd;    
    struct pollfd poll_array[1];    
    poll_array[0].fd = accept_fd;    
    poll_array[0].events = POLLIN;  
    memset(&struRcv,0,sizeof(ISCSID_UTIL_REQ_T));
    memset(&struSnd,0,sizeof(ISCSID_UTIL_RSP_T));
    while(1)    
    {  
        iRet = poll(poll_array, 1, -1);//参数-1 无限等待            
        if (iRet > 0)            
        {                
            if (poll_array[0].revents)                 
            {                    
                iRet = iscsi_util_recv_and_send(accept_fd,&struRcv,&struSnd);
                if(iRet == HPR_INFINITE)
                {
                    printf("iscsi_util_recv_and_send ret error\n");
                    break ;
                }
                else if(iRet == ISCSID_NEED_CANCLE)
                {
                    close(accept_fd);
                    return 0 ;        
                }
            }            
        }             
        else        
        {                
            printf("got poll() error (%d), errno (%d),exiting\n", iRet, errno);               
            break;            
        }  
    }
    return iRet ;
}

#define POLL_CTRL   0
#define POLL_IPC    1
#define POLL_ISNS	2
#define POLL_MAX	3

/* TODO: this should go somewhere else */
void event_loop(struct iscsi_ipc *ipc, int control_fd, int mgmt_ipc_fd,
		int isns_fd)
{
    struct pollfd poll_array[POLL_MAX];
    int res;

    poll_array[POLL_CTRL].fd = control_fd;
    poll_array[POLL_CTRL].events = POLLIN;
    poll_array[POLL_IPC].fd = mgmt_ipc_fd;
    poll_array[POLL_IPC].events = POLLIN;

    if (isns_fd < 0)
        poll_array[POLL_ISNS].fd = poll_array[POLL_ISNS].events = 0;
    else {
        poll_array[POLL_ISNS].fd = isns_fd;
        poll_array[POLL_ISNS].events = POLLIN;
    }

    leave_event_loop = 0;
    while (!leave_event_loop) {
        res = poll(poll_array, POLL_MAX, ACTOR_RESOLUTION);
        if (res > 0) {
            log_debug(6, "poll result %d", res);
            /*
             * flush sysfs cache since kernel objs may
             * have changed as a result of handling op
             */
            sysfs_cleanup();
            if (poll_array[POLL_CTRL].revents)
                ipc->ctldev_handle();

            if (poll_array[POLL_IPC].revents)
                mgmt_ipc_handle(mgmt_ipc_fd);

        } else if (res < 0) {
            if (errno == EINTR) {
                log_debug(1, "event_loop interrupted");
            } else {
                log_error("got poll() error (%d), errno (%d), "
                      "exiting", res, errno);
                break;
            }
        } else
            actor_poll();
        reaper();
    }
}
