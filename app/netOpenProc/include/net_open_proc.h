/** 
* @file net_open_proc.h
*
* @note V1.0 initial
*/

#ifndef __NET_OPEN_PROC_H__
#define __NET_OPEN_PROC_H__
                                        
#define NET_OPEN_PROC_SRV_PATH               "/var/nettool.socket"
                                        
#define NET_OPEN_PROC_TIME_OUT               (5*1000) /*5000ms*/
#define NET_OPEN_PROC_CMD_BODY_MAX_LEN       (256)    /*命令消息体最大长度*/
#define NET_OPEN_PROC_CMD_MAX_LEN            (NET_OPEN_PROC_CMD_BODY_MAX_LEN+sizeof(NET_OPEN_PROC_CMD_HEAD_T)) /*最大命令长度*/
#define NET_OPEN_PROC_CMD_MAX_PARAMS         32          /*最大参数个数*/
#define NET_OPEN_PROC_CMD_PER_PARAM_LEN      32          /*每个参数的最大长度*/
#define NET_OPEN_PROC_CMD_IPTABLE            0x00000001  /*IP table  相关命令*/
#define NET_OPEN_PROC_CMD_IPROUTE            0x00000002  /*IP router 相关命令*/

#define NET_OPEN_PROC_RET_ERR                0xFFFFFFFF  /*error*/
#define NET_OPEN_PROC_RET_OK                 0x00000000  /*ok*/
#define NET_OPEN_PROC_RET_PARAM_TOO_LONG     0x00000001  /*参数长度错误*/
#define NET_OPEN_PROC_RET_CMD_UNKOWN         0x00000002  /*未知的命令字*/

typedef struct{
    unsigned int  dwLen;             /* total length */
    unsigned int  dwCmd;             /* client request command */
    unsigned char abyRes[24];
}NET_OPEN_PROC_CMD_HEAD_T;

typedef struct
{
    unsigned int  dwLen;             /* total length */
    unsigned int  dwRet;             /* 返回值*/
}NET_OPEN_PROC_RET_HEAD_T;

int net_open_recvn(int iSockFd, char *pBuf, int iBufCount, int nTimeOut);
int net_open_sendn(int iSockFd, char *pBuf, int iBufLen, int nTimeOut);


#endif

