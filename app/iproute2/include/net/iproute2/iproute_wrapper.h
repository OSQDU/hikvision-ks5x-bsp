/** 
* @file iproute_wrapper.h
* @brief ip route wrapperͷ�ļ�
*
*
* @note V1.0 initial
*/

#ifndef __IP_ROUTE_WRAPPER_H__
#define __IP_ROUTE_WRAPPER_H__

#include <arpa/inet.h>

#ifdef  __cplusplus
extern "C" {
#endif

/** @fn  ip_route_set_rule
* @brief ����ip ·�ɹ���
* @param  [in] pszIfName ��������
* @param  [in] iIdx �������������0 ��ʼ
* @param  [in] pStruIp, pStruMask, pStruGw ��ַ��Ϣ
* @return  0:ok other: error               
*/
int ip_route_set_rule(const char *pszIfName, int iIdx
    , struct in_addr *pStruIp, struct in_addr *pStruMask, struct in_addr *pStruGw);

#ifdef  __cplusplus
}
#endif

#endif


