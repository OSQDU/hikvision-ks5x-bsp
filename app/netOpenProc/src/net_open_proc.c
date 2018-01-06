/**
* @file net_open_src_proc.c

*
* @note V1.0 initial
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sched.h>
#include <dirent.h>
#include <sys/un.h>
#include <pthread.h>
#include <errno.h>

#include "net_open_proc.h"
#include "net_open_proc_debug.h"



#ifdef CONFIG_SUPPORT_NUI_GPL_PROC
pthread_mutex_t s_iptablesMutex = PTHREAD_MUTEX_INITIALIZER;

extern int ip4tables_main(int argc, char *argv[]);
extern int iproute_main(int argc, char **argv);

static int net_open_parse_argv(const char *pszCmd, int *pArgc, char (*pArgv)[NET_OPEN_PROC_CMD_PER_PARAM_LEN])
{
    int  iArgc = 0;
    const char * pStart = NULL;
    const char * pEnd = NULL;

    if(!pszCmd || !pArgc || !pArgv)
    {
        return -1;
    }

    pStart = pszCmd;
    while(' ' == pStart[0])
    {
        pStart++;
    }

    while(1)
    {
        pEnd = strstr(pStart, " ");
        if(NULL == pEnd)
        {
            if(pStart[0])
            {
                if(strlen(pStart) >=  NET_OPEN_PROC_CMD_MAX_PARAMS)
                {
                    break;
                }
                strncpy(pArgv[iArgc], pStart, NET_OPEN_PROC_CMD_MAX_PARAMS);
                iArgc++;
            }
            break;
        }

        if(pEnd - pStart >= NET_OPEN_PROC_CMD_PER_PARAM_LEN)
        {
            NET_OPEN_PROC_ERR("Param[%s] is too long.", pStart);
            break;
        }
        memcpy(pArgv[iArgc], pStart, pEnd-pStart);
        iArgc++;
        if(iArgc >= NET_OPEN_PROC_CMD_MAX_PARAMS)
        {
            break;
        }

        while(' ' == pEnd[0])
        {
            pEnd++;
        }
        pStart = pEnd;
    }

    if(0 == iArgc)
    {
        return -1;
    }

    *pArgc = iArgc;

    return 0;
}


static void *net_open_cmd_proc(void *param)
{
    int  iLen;
    unsigned int dwCmdLen;
    unsigned char  abyCmd[NET_OPEN_PROC_CMD_MAX_LEN+4] = {0};
    NET_OPEN_PROC_CMD_HEAD_T *pstruCmdHead = NULL;
    NET_OPEN_PROC_RET_HEAD_T struRet;
    int i, iArgc, iSock;
    char *pArgV[NET_OPEN_PROC_CMD_MAX_PARAMS];
    char aArgv[NET_OPEN_PROC_CMD_MAX_PARAMS][NET_OPEN_PROC_CMD_PER_PARAM_LEN];

    iSock = (int)param;

    if(iSock < 0)
    {
        return NULL;
    }

    bzero(&struRet, sizeof(struRet));
    struRet.dwRet = NET_OPEN_PROC_RET_OK;

    dwCmdLen = 0;
    if(sizeof(dwCmdLen) != net_open_recvn(iSock, (char *)abyCmd, sizeof(dwCmdLen), NET_OPEN_PROC_TIME_OUT))
    {
        struRet.dwRet = NET_OPEN_PROC_RET_ERR;
        NET_OPEN_PROC_ERR("recv cmd head len error, errno=%d\n", errno);
        goto errExit;
    }

    memcpy(&dwCmdLen, abyCmd, sizeof(dwCmdLen));
    if((dwCmdLen > NET_OPEN_PROC_CMD_MAX_LEN) || (dwCmdLen < sizeof(NET_OPEN_PROC_CMD_HEAD_T)))
    {
        struRet.dwRet = NET_OPEN_PROC_RET_PARAM_TOO_LONG;
        goto errExit;
    }

    bzero(abyCmd, sizeof(abyCmd));
    memcpy(abyCmd, &dwCmdLen, sizeof(dwCmdLen));
    iLen = dwCmdLen-sizeof(dwCmdLen);
    if(iLen != net_open_recvn(iSock, (char *)abyCmd+sizeof(dwCmdLen), iLen, NET_OPEN_PROC_TIME_OUT))
    {
        struRet.dwRet = NET_OPEN_PROC_RET_ERR;
        goto errExit;
    }

    iArgc = 0;
    bzero(aArgv, sizeof(aArgv));
    if(0 != net_open_parse_argv((char *)abyCmd+sizeof(NET_OPEN_PROC_CMD_HEAD_T)
        , &iArgc, aArgv))
    {
        struRet.dwRet = NET_OPEN_PROC_RET_PARAM_TOO_LONG;
        goto errExit;
    }

    for(i=0; i<iArgc; i++)
    {
        pArgV[i] = aArgv[i];
    }

    NET_OPEN_PROC_INFO("cmd is: %s!\n", (char *)abyCmd + sizeof(NET_OPEN_PROC_CMD_HEAD_T));

    pstruCmdHead = (NET_OPEN_PROC_CMD_HEAD_T *)abyCmd;
    switch(pstruCmdHead->dwCmd)
    {
        case NET_OPEN_PROC_CMD_IPTABLE:
            pthread_mutex_lock(&s_iptablesMutex);
            (void)ip4tables_main(iArgc, pArgV);
            pthread_mutex_unlock(&s_iptablesMutex);
            struRet.dwRet = NET_OPEN_PROC_RET_OK;
            break;

        case NET_OPEN_PROC_CMD_IPROUTE:
            iproute_main(iArgc, pArgV);
            struRet.dwRet = NET_OPEN_PROC_RET_OK;
            break;

        default:
            struRet.dwRet = NET_OPEN_PROC_RET_CMD_UNKOWN;
            break;
    }


errExit:
    struRet.dwLen = sizeof(struRet);
    (void)net_open_sendn(iSock, (char *)&struRet, sizeof(struRet), NET_OPEN_PROC_TIME_OUT);
    close(iSock);

    return NULL;
}

int main(int argc, char *argv[])
{
    int iRet;
    int iSrvSock, iCliSock;
    socklen_t uCliLen;
    struct sockaddr_un struSrvAddr, struCliAddr;
    pthread_t      Tipthread;
    pthread_attr_t pAttr;


    bzero(&struSrvAddr, sizeof(struSrvAddr));
    iSrvSock = socket(AF_UNIX, SOCK_STREAM, 0);
    if(iSrvSock < 0)
    {
        NET_OPEN_PROC_ERR("create socket error, errno=%d\n", errno);
        return -1;
    }

    unlink(NET_OPEN_PROC_SRV_PATH);
    struSrvAddr.sun_family = AF_UNIX;
    strcpy(struSrvAddr.sun_path, NET_OPEN_PROC_SRV_PATH);
    if(bind(iSrvSock, (struct sockaddr *)&struSrvAddr, sizeof(struSrvAddr)) != 0)
    {
        NET_OPEN_PROC_ERR("bind error, errno=%d\n", errno);
        return -1;
    }

    if(listen(iSrvSock, 5) != 0)
    {
        NET_OPEN_PROC_ERR("listen error, errno=%d\n", errno);
        return -1;
    }

    pthread_attr_init (&pAttr);
    pthread_attr_setdetachstate (&pAttr, PTHREAD_CREATE_DETACHED);

    while(1)
    {
        uCliLen = sizeof(struct sockaddr_storage);
        bzero(&struCliAddr, sizeof(struCliAddr));

        iCliSock = accept(iSrvSock, (struct sockaddr *)&struCliAddr, &uCliLen);
        if(iCliSock >= 0)
        {
            iRet = pthread_create (&Tipthread, &pAttr, net_open_cmd_proc, (void *)iCliSock);
            if(0 != iRet)
            {
                NET_OPEN_PROC_FATAL("create net tool proc task error!\n");
                close(iCliSock);
            }
        }
        else
        {
            NET_OPEN_PROC_ERR("accept error errno=%d\n", errno);
        }

    }

    close(iSrvSock);


    return 0;
}


#endif

