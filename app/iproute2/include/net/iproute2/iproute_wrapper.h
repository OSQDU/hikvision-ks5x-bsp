/** 
* @file iproute_wrapper.h
* @brief ip route wrapper头文件
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
* @brief 设置ip 路由规则
* @param  [in] pszIfName 网卡名称
* @param  [in] iIdx 规则表索引，从0 开始
* @param  [in] pStruIp, pStruMask, pStruGw 地址信息
* @return  0:ok other: error               
*/
int ip_route_set_rule(const char *pszIfName, int iIdx
    , struct in_addr *pStruIp, struct in_addr *pStruMask, struct in_addr *pStruGw);

#ifdef  __cplusplus
}
#endif

#endif


