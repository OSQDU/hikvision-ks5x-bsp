/** 
* @file net_open_proc.h
*
* @note V1.0 initial
*/

#ifndef __NET_OPEN_PROC_H__
#define __NET_OPEN_PROC_H__
                                        
#define NET_OPEN_PROC_SRV_PATH               "/var/nettool.socket"
                                        
#define NET_OPEN_PROC_TIME_OUT               (5*1000) /*5000ms*/
#define NET_OPEN_PROC_CMD_BODY_MAX_LEN       (256)    /*������Ϣ����󳤶�*/
#define NET_OPEN_PROC_CMD_MAX_LEN            (NET_OPEN_PROC_CMD_BODY_MAX_LEN+sizeof(NET_OPEN_PROC_CMD_HEAD_T)) /*��������*/
#define NET_OPEN_PROC_CMD_MAX_PARAMS         32          /*����������*/
#define NET_OPEN_PROC_CMD_PER_PARAM_LEN      32          /*ÿ����������󳤶�*/
#define NET_OPEN_PROC_CMD_IPTABLE            0x00000001  /*IP table  �������*/
#define NET_OPEN_PROC_CMD_IPROUTE            0x00000002  /*IP router �������*/

#define NET_OPEN_PROC_RET_ERR                0xFFFFFFFF  /*error*/
#define NET_OPEN_PROC_RET_OK                 0x00000000  /*ok*/
#define NET_OPEN_PROC_RET_PARAM_TOO_LONG     0x00000001  /*�������ȴ���*/
#define NET_OPEN_PROC_RET_CMD_UNKOWN         0x00000002  /*δ֪��������*/

typedef struct{
    unsigned int  dwLen;             /* total length */
    unsigned int  dwCmd;             /* client request command */
    unsigned char abyRes[24];
}NET_OPEN_PROC_CMD_HEAD_T;

typedef struct
{
    unsigned int  dwLen;             /* total length */
    unsigned int  dwRet;             /* ����ֵ*/
}NET_OPEN_PROC_RET_HEAD_T;

int net_open_recvn(int iSockFd, char *pBuf, int iBufCount, int nTimeOut);
int net_open_sendn(int iSockFd, char *pBuf, int iBufLen, int nTimeOut);


#endif

