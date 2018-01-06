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
#include <poll.h>
#include <time.h>
#include <errno.h>

#include "net_open_proc_debug.h"



#ifdef CONFIG_SUPPORT_NUI_GPL_PROC

unsigned int net_open_get_time_tick()
{
    struct timespec spec;
    memset(&spec, 0x0, sizeof(struct timespec));
    clock_gettime(CLOCK_MONOTONIC, &spec);
    
    return (unsigned int)((spec.tv_sec * 1000) + (spec.tv_nsec + 1000000 / 2) / 1000000);
}


int net_open_poll(struct pollfd* fds,int iFds, int* iTimeO)
{
    int iRet = -1;
    int iStartTime;
    int iTimeDiff;

    if ((iTimeO == NULL) || (*iTimeO == -1))
    {
RETRY:
        iRet = poll(fds, iFds, -1);
        if(iRet == -1 && errno == EINTR)
        {
            usleep(10*1000);
            goto RETRY;
        }

        return iRet;
    }

REPOLL:
    iStartTime = net_open_get_time_tick();
    iRet = poll(fds, iFds, *iTimeO);
    iTimeDiff = (int)net_open_get_time_tick() - iStartTime;

    if(iRet == -1 && errno == EINTR)
    {
        *iTimeO -= iTimeDiff;
        goto REPOLL;
    }

    if (*iTimeO > iTimeDiff)
    {
        *iTimeO -= iTimeDiff;
    }
    else
    {
        *iTimeO = 0;
    }

    return iRet;
}

int net_open_recvn(int iSockFd, char *pBuf, int iBufCount, int nTimeOut)
{
    int iRecvLen = 0;
    int iTmpLen = 0;
    int iPollRet = -1;
    struct pollfd fds[1];

    do
    {
        memset(&fds[0], 0, sizeof(fds));
        fds[0].fd = iSockFd;
        fds[0].events = POLLRDNORM;
        iPollRet = net_open_poll(fds, 1, &nTimeOut);
        if ((iPollRet > 0) && (fds[0].revents & POLLRDNORM))
        {
            iTmpLen = recv(iSockFd, (char*)pBuf+iRecvLen, iBufCount-iRecvLen, 0);
            if (iTmpLen > 0)
            {
                iRecvLen += iTmpLen;
                if (iRecvLen == iBufCount)//855gm
                {
                    break;
                }
            }
            else
            {
                return -1;
            }
        }
        else if (iPollRet == 0)
        {
            break;
        }
        else
        {
            return -1;
        }

    }while(nTimeOut > 0);

    return iRecvLen;
}

int net_open_sendn(int iSockFd, char *pBuf, int iBufLen, int nTimeOut)
{
    int iSendLen = 0;
    int iTmpLen = 0;
    int iPollRet = -1;
    struct pollfd fds[1];

    do
    {
        memset(&fds[0], 0, sizeof(fds));
        fds[0].fd = iSockFd;
        fds[0].events = POLLOUT;//POLLWRNORM值与POLLOUT内核中的定义是一个值
        iPollRet = net_open_poll(fds,1,&nTimeOut);
        if ( (iPollRet > 0) && (fds[0].revents & POLLOUT) )
        {
            iTmpLen = send(iSockFd, (char*)pBuf+iSendLen, iBufLen-iSendLen, 0);
            if (iTmpLen > 0)
            {
                iSendLen += iTmpLen;
                if (iSendLen == iBufLen)
                {
                    break;
                }
            }
            else
            {
                //已经判断可写但是写出错说明可能revents有其他异常直接跳出循环
                break;
            }
        }
        else if(0 == iPollRet)
        {
            //超时时仍然循环,超时后nTimeOut==0,因此会跳出循环
            break;
        }
        else
        {
            /* poll出错则需要退出循环 */ 
            break;
        }
    }while(nTimeOut > 0);

    return iSendLen;
}



#endif

