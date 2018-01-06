/*
 * ip.c		"ip" utility frontend.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 *
 * Changes:
 *
 * Rani Assaf <rani@magic.metawire.com> 980929:	resolve addresses
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>

#include <linux/rtnetlink.h>

#include "net/iproute2/libnetlink.h"
#include "net/iproute2/utils.h"
#include "net/iproute2/fib_rules.h"
#include "net/iproute2/rt_names.h"

#define NLM_F_REPLACE            0x100   /* Override existing        */
#define NLM_F_EXCL               0x200   /* Do not touch, if it exists   */
#define NLM_F_CREATE             0x400   /* Create, if it does not exist */
#define NLM_F_APPEND             0x800   /* Add to end of list       */


#ifdef CONFIG_SUPPORT_NUI_GPL_PROC
static void usage(void);
#endif


/*************************************************
 * Function     :   iprule_modify
 * Description  :  修改网卡路由策略
 * Input        :   pRth 网络底层接口
                       cmd   命令
                       argc     参数个数
                       argv     参数
 * Output       : 无
 * Return       :  结果
 *************************************************/
//#define AF_UNSPEC 0
//#define AF_INET       2   /* Internet IP Protocol     */


static int iprule_modify(struct rtnl_handle *pRth, int cmd, int argc, char **argv)
{
    int table_ok = 0;
    struct {
        struct nlmsghdr     n;
        struct rtmsg        r;
        char            buf[1024];
    } req;
    int preferred_family = AF_UNSPEC;
    
    memset(&req, 0, sizeof(req));

    req.n.nlmsg_type = cmd;
    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    req.n.nlmsg_flags = NLM_F_REQUEST;
    req.r.rtm_family = AF_UNSPEC;
    req.r.rtm_protocol = RTPROT_BOOT;
    req.r.rtm_scope = RT_SCOPE_UNIVERSE;
    req.r.rtm_table = 0;
    req.r.rtm_type = RTN_UNSPEC;
    req.r.rtm_flags = 0;

    if (cmd == RTM_NEWRULE) {
        req.n.nlmsg_flags |= NLM_F_CREATE|NLM_F_EXCL;
        req.r.rtm_type = RTN_UNICAST;
    }

    while (argc > 0) {
        if (strcmp(*argv, "not") == 0) {
            req.r.rtm_flags |= FIB_RULE_INVERT;
        } else if (strcmp(*argv, "from") == 0) {
            inet_prefix dst;
            NEXT_ARG();
            get_prefix(&dst, *argv, req.r.rtm_family);
            req.r.rtm_src_len = dst.bitlen;
            addattr_l(&req.n, sizeof(req), FRA_SRC, &dst.data, dst.bytelen);
        } else if (strcmp(*argv, "to") == 0) {
            inet_prefix dst;
            NEXT_ARG();
            get_prefix(&dst, *argv, req.r.rtm_family);
            req.r.rtm_dst_len = dst.bitlen;
            addattr_l(&req.n, sizeof(req), FRA_DST, &dst.data, dst.bytelen);
        } else if (matches(*argv, "preference") == 0 ||
               matches(*argv, "order") == 0 ||
               matches(*argv, "priority") == 0) {
            __u32 pref;
            NEXT_ARG();
            if (get_u32(&pref, *argv, 0))
                invarg("preference value is invalid\n", *argv);
            addattr32(&req.n, sizeof(req), FRA_PRIORITY, pref);
        } else if (strcmp(*argv, "tos") == 0) {
            __u32 tos;
            NEXT_ARG();
            if (rtnl_dsfield_a2n(&tos, *argv))
                invarg("TOS value is invalid\n", *argv);
            req.r.rtm_tos = tos;
        } else if (strcmp(*argv, "fwmark") == 0) {
            char *slash;
            __u32 fwmark, fwmask;
            NEXT_ARG();
            if ((slash = strchr(*argv, '/')) != NULL)
                *slash = '\0';
            if (get_u32(&fwmark, *argv, 0))
                invarg("fwmark value is invalid\n", *argv);
            addattr32(&req.n, sizeof(req), FRA_FWMARK, fwmark);
            if (slash) {
                if (get_u32(&fwmask, slash+1, 0))
                    invarg("fwmask value is invalid\n", slash+1);
                addattr32(&req.n, sizeof(req), FRA_FWMASK, fwmask);
            }
        } /*else if (matches(*argv, "realms") == 0) {
            __u32 realm;
            NEXT_ARG();
            if (get_rt_realms(&realm, *argv))
                invarg("invalid realms\n", *argv);
            addattr32(&req.n, sizeof(req), FRA_FLOW, realm);
        }*/ else if (matches(*argv, "table") == 0 ||
               strcmp(*argv, "lookup") == 0) {
            __u32 tid;
            NEXT_ARG();
            if (rtnl_rttable_a2n(&tid, *argv))
                invarg("invalid table ID\n", *argv);
            if (tid < 256)
                req.r.rtm_table = tid;
            else {
                req.r.rtm_table = RT_TABLE_UNSPEC;
                addattr32(&req.n, sizeof(req), FRA_TABLE, tid);
            }
            table_ok = 1;
        } else if (strcmp(*argv, "dev") == 0 ||
               strcmp(*argv, "iif") == 0) {
            NEXT_ARG();
            addattr_l(&req.n, sizeof(req), FRA_IFNAME, *argv, strlen(*argv)+1);
        } else if (strcmp(*argv, "nat") == 0 ||
               matches(*argv, "map-to") == 0) {
            NEXT_ARG();
            fprintf(stderr, "Warning: route NAT is deprecated\n");
            addattr32(&req.n, sizeof(req), RTA_GATEWAY, get_addr32(*argv));
            req.r.rtm_type = RTN_NAT;
        } else {
            int type = 0;

            if (strcmp(*argv, "type") == 0) {
                NEXT_ARG();
            }
            if (matches(*argv, "help") == 0)
                fprintf (stderr,"ip rule modify.\n");//usage();
            else if (matches(*argv, "goto") == 0) {
                __u32 target;
                type = FR_ACT_GOTO;
                NEXT_ARG();
                if (get_u32(&target, *argv, 0))
                    invarg("invalid target\n", *argv);
                addattr32(&req.n, sizeof(req), FRA_GOTO, target);
            } else if (matches(*argv, "nop") == 0)
                type = FR_ACT_NOP;
            /*else if (rtnl_rtntype_a2n(&type, *argv))
                invarg("Failed to parse rule type", *argv);*/
            req.r.rtm_type = type;
            table_ok = 1;
        }
        argc--;
        argv++;
    }

    if (req.r.rtm_family == AF_UNSPEC)
        req.r.rtm_family = AF_INET;

    if (!table_ok && cmd == RTM_NEWRULE)
        req.r.rtm_table = RT_TABLE_MAIN;

    if (rtnl_talk(pRth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
        return 2;

    return 0;
}

/*************************************************
 * Function     :   parse_one_nh
 * Description  :  修改网卡路由策略相关函数
 * Input        :   rta   struct rtattr *
            rtnh    struct rtnexthop *
            argcp  int *
            argvp  char ***
 * Output       :   无
 * Return       :   结果
 *************************************************/

int parse_one_nh(struct rtattr *rta, struct rtnexthop *rtnh, int *argcp, char ***argvp)
{
    int argc = *argcp;
    char **argv = *argvp;

    while (++argv, --argc > 0) {
        if (strcmp(*argv, "via") == 0) {
            NEXT_ARG();
            rta_addattr32(rta, 4096, RTA_GATEWAY, get_addr32(*argv));
            rtnh->rtnh_len += sizeof(struct rtattr) + 4;
        } else if (strcmp(*argv, "dev") == 0) {
            NEXT_ARG();
            if ((rtnh->rtnh_ifindex = ll_name_to_index(*argv)) == 0) {
                fprintf(stderr, "Cannot find device \"%s\"\n", *argv);
                exit(1);
            }
        } else if (strcmp(*argv, "weight") == 0) {
            unsigned w;
            NEXT_ARG();
            if (get_unsigned(&w, *argv, 0) || w == 0 || w > 256)
                invarg("\"weight\" is invalid\n", *argv);
            rtnh->rtnh_hops = w - 1;
        } else if (strcmp(*argv, "onlink") == 0) {
            rtnh->rtnh_flags |= RTNH_F_ONLINK;
        } else if (matches(*argv, "realms") == 0) {
            __u32 realm = 0;
            NEXT_ARG();
            /*if (get_rt_realms(&realm, *argv))
                invarg("\"realm\" value is invalid\n", *argv);*/
            rta_addattr32(rta, 4096, RTA_FLOW, realm);
            rtnh->rtnh_len += sizeof(struct rtattr) + 4;
        } else
            break;
    }
    *argcp = argc;
    *argvp = argv;
    return 0;
}


/*************************************************
 * Function     :   parse_nexthops
 * Description  :  修改网卡路由策略相关函数
 * Input        :   n          struct nlmsghdr *
                        r         struct rtmsg *
                        argc    int
                        argv    char **
 * Output       :   无
 * Return       :   结果
 *************************************************/

int parse_nexthops(struct nlmsghdr *n, struct rtmsg *r, int argc, char **argv)
{
    char buf[1024];
    struct rtattr *rta = (void*)buf;
    struct rtnexthop *rtnh;

    rta->rta_type = RTA_MULTIPATH;
    rta->rta_len = RTA_LENGTH(0);
    rtnh = RTA_DATA(rta);

    while (argc > 0) {
        if (strcmp(*argv, "nexthop") != 0) {
            fprintf(stderr, "Error: \"nexthop\" or end of line is expected instead of \"%s\"\n", *argv);
            exit(-1);
        }
        if (argc <= 1) {
            fprintf(stderr, "Error: unexpected end of line after \"nexthop\"\n");
            exit(-1);
        }
        memset(rtnh, 0, sizeof(*rtnh));
        rtnh->rtnh_len = sizeof(*rtnh);
        rta->rta_len += rtnh->rtnh_len;
        parse_one_nh(rta, rtnh, &argc, &argv);
        rtnh = RTNH_NEXT(rtnh);
    }

    if (rta->rta_len > RTA_LENGTH(0))
        addattr_l(n, 1024, RTA_MULTIPATH, RTA_DATA(rta), RTA_PAYLOAD(rta));
    return 0;
}


/*************************************************
 * Function     :   iproute_modify
 * Description  :  修改路由表信息
 * Input        :  pRth   struct rtnl_handle *
             cmd     命令
             flags    标志
             argc    参数个数
             argv    参数
 * Output       :   无
 * Return       :   结果
 *************************************************/

static int iproute_modify(struct rtnl_handle *pRth, int cmd, unsigned flags, int argc, char **argv)
{
    struct {
        struct nlmsghdr     n;
        struct rtmsg        r;
        char            buf[1024];
    } req;
    char  mxbuf[256];
    struct rtattr * mxrta = (void*)mxbuf;
    unsigned mxlock = 0;
    char  *d = NULL;
    int gw_ok = 0;
    int dst_ok = 0;
    int nhs_ok = 0;
    int scope_ok = 0;
    int table_ok = 0;
    int proto_ok = 0;
    int type_ok = 0;
    int preferred_family = AF_UNSPEC;
    
    memset(&req, 0, sizeof(req));

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    req.n.nlmsg_flags = NLM_F_REQUEST|flags;
    req.n.nlmsg_type = cmd;
    req.r.rtm_family = AF_UNSPEC;
    req.r.rtm_table = RT_TABLE_MAIN;
    req.r.rtm_scope = RT_SCOPE_NOWHERE;

    if (cmd != RTM_DELROUTE) {
        req.r.rtm_protocol = RTPROT_BOOT;
        req.r.rtm_scope = RT_SCOPE_UNIVERSE;
        req.r.rtm_type = RTN_UNICAST;
    }

    mxrta->rta_type = RTA_METRICS;
    mxrta->rta_len = RTA_LENGTH(0);
    //fprintf (stderr, "iproute_modify1.\n");
    while (argc > 0) {
        //fprintf (stderr, "iproute_modify argc %d argv %s.\n", argc, *argv);
        if (strcmp(*argv, "src") == 0) {
            inet_prefix addr;
            NEXT_ARG();
            get_addr(&addr, *argv, req.r.rtm_family);
            if (req.r.rtm_family == AF_UNSPEC)
                req.r.rtm_family = addr.family;
            addattr_l(&req.n, sizeof(req), RTA_PREFSRC, &addr.data, addr.bytelen);
        } else if (strcmp(*argv, "via") == 0) {
            inet_prefix addr;
            gw_ok = 1;
            NEXT_ARG();
            get_addr(&addr, *argv, req.r.rtm_family);
            if (req.r.rtm_family == AF_UNSPEC)
                req.r.rtm_family = addr.family;
            addattr_l(&req.n, sizeof(req), RTA_GATEWAY, &addr.data, addr.bytelen);
        } else if (strcmp(*argv, "from") == 0) {
            inet_prefix addr;
            NEXT_ARG();
            get_prefix(&addr, *argv, req.r.rtm_family);
            if (req.r.rtm_family == AF_UNSPEC)
                req.r.rtm_family = addr.family;
            if (addr.bytelen)
                addattr_l(&req.n, sizeof(req), RTA_SRC, &addr.data, addr.bytelen);
            req.r.rtm_src_len = addr.bitlen;
        } else if (strcmp(*argv, "tos") == 0 ||
               matches(*argv, "dsfield") == 0) {
            __u32 tos;
            NEXT_ARG();
            if (rtnl_dsfield_a2n(&tos, *argv))
                invarg("\"tos\" value is invalid\n", *argv);
            req.r.rtm_tos = tos;
        } else if (matches(*argv, "metric") == 0 ||
               matches(*argv, "priority") == 0 ||
               matches(*argv, "preference") == 0) {
            __u32 metric;
            NEXT_ARG();
            if (get_u32(&metric, *argv, 0))
                invarg("\"metric\" value is invalid\n", *argv);
            addattr32(&req.n, sizeof(req), RTA_PRIORITY, metric);
        } else if (strcmp(*argv, "scope") == 0) {
            __u32 scope = 0;
            NEXT_ARG();
            if (rtnl_rtscope_a2n(&scope, *argv))
                invarg("invalid \"scope\" value\n", *argv);
            req.r.rtm_scope = scope;
            scope_ok = 1;
        } else if (strcmp(*argv, "mtu") == 0) {
            unsigned mtu;
            NEXT_ARG();
            if (strcmp(*argv, "lock") == 0) {
                mxlock |= (1<<RTAX_MTU);
                NEXT_ARG();
            }
            if (get_unsigned(&mtu, *argv, 0))
                invarg("\"mtu\" value is invalid\n", *argv);
            rta_addattr32(mxrta, sizeof(mxbuf), RTAX_MTU, mtu);
#ifdef RTAX_ADVMSS
        } else if (strcmp(*argv, "advmss") == 0) {
            unsigned mss;
            NEXT_ARG();
            if (strcmp(*argv, "lock") == 0) {
                mxlock |= (1<<RTAX_ADVMSS);
                NEXT_ARG();
            }
            if (get_unsigned(&mss, *argv, 0))
                invarg("\"mss\" value is invalid\n", *argv);
            rta_addattr32(mxrta, sizeof(mxbuf), RTAX_ADVMSS, mss);
#endif
#ifdef RTAX_REORDERING
        } else if (matches(*argv, "reordering") == 0) {
            unsigned reord;
            NEXT_ARG();
            if (strcmp(*argv, "lock") == 0) {
                mxlock |= (1<<RTAX_REORDERING);
                NEXT_ARG();
            }
            if (get_unsigned(&reord, *argv, 0))
                invarg("\"reordering\" value is invalid\n", *argv);
            rta_addattr32(mxrta, sizeof(mxbuf), RTAX_REORDERING, reord);
#endif
        } else if (strcmp(*argv, "rtt") == 0) {
            unsigned rtt;
            NEXT_ARG();
            if (strcmp(*argv, "lock") == 0) {
                mxlock |= (1<<RTAX_RTT);
                NEXT_ARG();
            }
            if (get_unsigned(&rtt, *argv, 0))
                invarg("\"rtt\" value is invalid\n", *argv);
            rta_addattr32(mxrta, sizeof(mxbuf), RTAX_RTT, rtt);
        } else if (matches(*argv, "window") == 0) {
            unsigned win;
            NEXT_ARG();
            if (strcmp(*argv, "lock") == 0) {
                mxlock |= (1<<RTAX_WINDOW);
                NEXT_ARG();
            }
            if (get_unsigned(&win, *argv, 0))
                invarg("\"window\" value is invalid\n", *argv);
            rta_addattr32(mxrta, sizeof(mxbuf), RTAX_WINDOW, win);
        } else if (matches(*argv, "cwnd") == 0) {
            unsigned win;
            NEXT_ARG();
            if (strcmp(*argv, "lock") == 0) {
                mxlock |= (1<<RTAX_CWND);
                NEXT_ARG();
            }
            if (get_unsigned(&win, *argv, 0))
                invarg("\"cwnd\" value is invalid\n", *argv);
            rta_addattr32(mxrta, sizeof(mxbuf), RTAX_CWND, win);
        } else if (matches(*argv, "initcwnd") == 0) {
            unsigned win;
            NEXT_ARG();
            if (strcmp(*argv, "lock") == 0) {
                mxlock |= (1<<RTAX_INITCWND);
                NEXT_ARG();
            }
            if (get_unsigned(&win, *argv, 0))
                invarg("\"initcwnd\" value is invalid\n", *argv);
            rta_addattr32(mxrta, sizeof(mxbuf), RTAX_INITCWND, win);
        } else if (matches(*argv, "rttvar") == 0) {
            unsigned win;
            NEXT_ARG();
            if (strcmp(*argv, "lock") == 0) {
                mxlock |= (1<<RTAX_RTTVAR);
                NEXT_ARG();
            }
            if (get_unsigned(&win, *argv, 0))
                invarg("\"rttvar\" value is invalid\n", *argv);
            rta_addattr32(mxrta, sizeof(mxbuf), RTAX_RTTVAR, win);
        } else if (matches(*argv, "ssthresh") == 0) {
            unsigned win;
            NEXT_ARG();
            if (strcmp(*argv, "lock") == 0) {
                mxlock |= (1<<RTAX_SSTHRESH);
                NEXT_ARG();
            }
            if (get_unsigned(&win, *argv, 0))
                invarg("\"ssthresh\" value is invalid\n", *argv);
            rta_addattr32(mxrta, sizeof(mxbuf), RTAX_SSTHRESH, win);
        } /*else if (matches(*argv, "realms") == 0) {
            __u32 realm;
            NEXT_ARG();
            if (get_rt_realms(&realm, *argv))
                invarg("\"realm\" value is invalid\n", *argv);
            addattr32(&req.n, sizeof(req), RTA_FLOW, realm);
        }*/ else if (strcmp(*argv, "onlink") == 0) {
            req.r.rtm_flags |= RTNH_F_ONLINK;
        } else if (matches(*argv, "equalize") == 0 ||
               strcmp(*argv, "eql") == 0) {
            req.r.rtm_flags |= RTM_F_EQUALIZE;
        } else if (strcmp(*argv, "nexthop") == 0) {
            nhs_ok = 1;
            break;
        } else if (matches(*argv, "protocol") == 0) {
            __u32 prot;
            NEXT_ARG();
            if (rtnl_rtprot_a2n(&prot, *argv))
                invarg("\"protocol\" value is invalid\n", *argv);
            req.r.rtm_protocol = prot;
            proto_ok =1;
        } else if (matches(*argv, "table") == 0) {
            __u32 tid;
            NEXT_ARG();
            
            if (rtnl_rttable_a2n(&tid, *argv))
                invarg("\"table\" value is invalid\n", *argv);
            if (tid < 256)
                req.r.rtm_table = tid;
            else {
                req.r.rtm_table = RT_TABLE_UNSPEC;
                addattr32(&req.n, sizeof(req), RTA_TABLE, tid);
            }
            table_ok = 1;
        } else if (strcmp(*argv, "dev") == 0 ||
               strcmp(*argv, "oif") == 0) {
            NEXT_ARG();
            d = *argv;
        } /*else if (strcmp(*argv, "mpath") == 0 ||
               strcmp(*argv, "mp") == 0) {
            int i;
            __u32 mp_alg = IP_MP_ALG_NONE;

            NEXT_ARG();
            for (i = 1; i < ARRAY_SIZE(mp_alg_names); i++)
                if (strcmp(*argv, mp_alg_names[i]) == 0)
                    mp_alg = i;
            if (mp_alg == IP_MP_ALG_NONE)
                invarg("\"mpath\" value is invalid\n", *argv);
            addattr_l(&req.n, sizeof(req), RTA_MP_ALGO, &mp_alg, sizeof(mp_alg));
        }*/else {
            int type;
            inet_prefix dst;
            
            if (strcmp(*argv, "to") == 0) {
                NEXT_ARG();
            }
            
            if ((**argv < '0' || **argv > '9') &&
                rtnl_rtntype_a2n(&type, *argv) == 0) {
                
                NEXT_ARG();
                req.r.rtm_type = type;
                type_ok = 1;
            }
            
            /*if (matches(*argv, "help") == 0)
                usage();*/
            if (dst_ok)
                duparg2("to", *argv);
            
            get_prefix(&dst, *argv, req.r.rtm_family);
            
            if (req.r.rtm_family == AF_UNSPEC)
                req.r.rtm_family = dst.family;
            req.r.rtm_dst_len = dst.bitlen;
            dst_ok = 1;
            
            if (dst.bytelen)
                addattr_l(&req.n, sizeof(req), RTA_DST, &dst.data, dst.bytelen);
            
        }
        argc--; argv++;
    }

    if (d || nhs_ok)  {
        int idx;

        ll_init_map(pRth);

        if (d) {
            if ((idx = ll_name_to_index(d)) == 0) {
                fprintf(stderr, "Cannot find device \"%s\"\n", d);
                return -1;
            }
            addattr32(&req.n, sizeof(req), RTA_OIF, idx);
        }
    }

    if (mxrta->rta_len > RTA_LENGTH(0)) {
        if (mxlock)
            rta_addattr32(mxrta, sizeof(mxbuf), RTAX_LOCK, mxlock);
        addattr_l(&req.n, sizeof(req), RTA_METRICS, RTA_DATA(mxrta), RTA_PAYLOAD(mxrta));
    }

    if (nhs_ok)
        parse_nexthops(&req.n, &req.r, argc, argv);

    if (!table_ok) {
        if (req.r.rtm_type == RTN_LOCAL ||
            req.r.rtm_type == RTN_BROADCAST ||
            req.r.rtm_type == RTN_NAT ||
            req.r.rtm_type == RTN_ANYCAST)
            req.r.rtm_table = RT_TABLE_LOCAL;
    }
    if (!scope_ok) {
        if (req.r.rtm_type == RTN_LOCAL ||
            req.r.rtm_type == RTN_NAT)
            req.r.rtm_scope = RT_SCOPE_HOST;
        else if (req.r.rtm_type == RTN_BROADCAST ||
             req.r.rtm_type == RTN_MULTICAST ||
             req.r.rtm_type == RTN_ANYCAST)
            req.r.rtm_scope = RT_SCOPE_LINK;
        else if (req.r.rtm_type == RTN_UNICAST ||
             req.r.rtm_type == RTN_UNSPEC) {
            if (cmd == RTM_DELROUTE)
                req.r.rtm_scope = RT_SCOPE_NOWHERE;
            else if (!gw_ok && !nhs_ok)
                req.r.rtm_scope = RT_SCOPE_LINK;
        }
    }

    if (req.r.rtm_family == AF_UNSPEC)
        req.r.rtm_family = AF_INET;

    if (rtnl_talk(pRth, &req.n, 0, 0, NULL, NULL, NULL) < 0)
        return (-2);

    return 0;
}

#ifdef CONFIG_SUPPORT_NUI_GPL_PROC
static int do_iprule(int argc, char **argv)
{
    int iRet = 0;
    struct rtnl_handle rth;

	if (argc < 1 || !argv)
	{
        return -1;
	}

    if(rtnl_open(&rth, 0) < 0)
    {
        fprintf (stderr, "rtnl_open error.\n");
        return -1;
    }

    if (matches(argv[0], "add") == 0) 
    {
		iRet = iprule_modify(&rth, RTM_NEWRULE, argc-1, argv+1);
	} 
    else if (matches(argv[0], "delete") == 0) 
    {
		iRet = iprule_modify(&rth, RTM_DELRULE, argc-1, argv+1);
	} 
    else if (matches(argv[0], "help") == 0)
	{
		usage();
        return 0;
	}
    else
    {
        fprintf(stderr, "Command \"%s\" is unknown, try \"ip rule help\".\n", *argv);
    }

    rtnl_close(&rth);

	return iRet;
}


static int do_iproute(int argc, char **argv)
{
    int iRet = 0;
    struct rtnl_handle rth;

	if (argc < 1 || !argv)
	{
        return -1;
	}

    if (rtnl_open(&rth, 0) < 0)
    {
        fprintf (stderr, "rtnl_open error.\n");
        return -1;
    }

	if(matches(*argv, "add") == 0)
	{
		iRet = iproute_modify(&rth, RTM_NEWROUTE, NLM_F_CREATE|NLM_F_EXCL,
				      argc-1, argv+1);
    }
	else if(matches(*argv, "change") == 0 || strcmp(*argv, "chg") == 0)
	{
		iRet = iproute_modify(&rth, RTM_NEWROUTE, NLM_F_REPLACE,
				      argc-1, argv+1);
	}
	else if(matches(*argv, "replace") == 0)
	{
		iRet = iproute_modify(&rth, RTM_NEWROUTE, NLM_F_CREATE|NLM_F_REPLACE,
				      argc-1, argv+1);
	}
	else if(matches(*argv, "prepend") == 0)
	{
		iRet = iproute_modify(&rth, RTM_NEWROUTE, NLM_F_CREATE,
				      argc-1, argv+1);
	}
	else if(matches(*argv, "append") == 0)
	{
		iRet = iproute_modify(&rth, RTM_NEWROUTE, NLM_F_CREATE|NLM_F_APPEND,
				      argc-1, argv+1);
	}
	else if(matches(*argv, "delete") == 0)
	{
		iRet = iproute_modify(&rth, RTM_DELROUTE, 0,
				      argc-1, argv+1);
	}
	else if(matches(*argv, "help") == 0)
	{
		usage();
        return 0;
	}
    else
    {
        fprintf(stderr, "Command \"%s\" is unknown, try \"ip route help\".\n", *argv);
    }
  
    rtnl_close(&rth);
	return iRet;
}


static void usage(void)
{
	fprintf(stderr,
"Usage: ip [ OPTIONS ] OBJECT { COMMAND | help }\n"
"       ip [ -force ] [-batch filename\n"
"where  OBJECT := { link | addr | route | rule | neigh | ntable | tunnel |\n"
"                   maddr | mroute | monitor | xfrm }\n"
"       OPTIONS := { -V[ersion] | -s[tatistics] | -d[etails] | -r[esolve] |\n"
"                    -f[amily] { inet | inet6 | ipx | dnet | link } |\n"
"                    -o[neline] | -t[imestamp] }\n");
	return;
}

static void do_help(int argc, char **argv)
{
	usage();
}

static const struct cmd {
	const char *cmd;
	int (*func)(int argc, char **argv);
} cmds[] = {
/*	{ "address", 	do_ipaddr },
	{ "maddress",	do_multiaddr },*/
	{ "route",	do_iproute },
	{ "rule",	do_iprule },
/*	{ "neighbor",	do_ipneigh },
	{ "neighbour",	do_ipneigh },
	{ "ntable",	do_ipntable },
	{ "ntbl",	do_ipntable },
	{ "link",	do_iplink },
	{ "tunnel",	do_iptunnel },
	{ "tunl",	do_iptunnel },
	{ "monitor",	do_ipmonitor },
	{ "xfrm",	do_xfrm },
	{ "mroute",	do_multiroute },*/
	{ "help",	do_help },
	{ 0 }
};

int iproute_main(int argc, char **argv)
{
	const struct cmd *c;

    if(argc < 2)
    {
        fprintf(stderr, "try \"ip help\"\n");
        return -1;
    }

    /*skip argv[1]*/
	for (c = cmds; c->cmd; ++c) {
		if (matches(argv[1], c->cmd) == 0)
			return c->func(argc-2, argv+2);
	}


    fprintf(stderr, "try \"ip help\"\n");
	return -1;
}

#else

/*************************************************
 * Function     :   do_iproute_add
 * Description  : 添加路由表
 * Input        :   pIp   ip地址
                       pDev 设备
                       pTable  路由表
                       bDef 是否为默认路由
                       
 * Output       :   无
 * Return       :   结果
 *desc      :   ip route add %s dev lo table %s
                ip route add default via %s dev eth0 table %s
 *************************************************/

int do_iproute_add(char *pIp, const char *pDev, char *pTable, int bDef)
{
    struct rtnl_handle rth;
    char* argv[16] = {0};
    char netIp[32] = {0};
    
    strncpy(netIp, pIp, sizeof(netIp));

    fprintf (stderr, "do_iproute_add %s.\n", pIp);
    int argc = 0;

    if(!bDef)
    {
        argv[argc++] = netIp;
    }
    else
    {
        argv[argc++] = "default";
        argv[argc++] = "via";
        argv[argc++] = netIp;
    }
    argv[argc++] = "dev";
    argv[argc++] = (char *)pDev;
    argv[argc++] = "table";
    argv[argc++] = pTable;

    memset (&rth, 0, sizeof(struct rtnl_handle));
    rth.fd = -1;
    
    if (rtnl_open(&rth, 0) < 0)
    {
        fprintf (stderr, "do_iprule_del, rtnl_open error.\n");
        return -1;
    }
    
    iproute_modify(&rth, 24,NLM_F_CREATE|NLM_F_EXCL, argc, argv);

    rtnl_close(&rth);
    return 0;
}



/*************************************************
 * Function     :   do_iprule_add
 * Description  : 添加路由策略
 * Input        :     pFrom  地址
                pTable   路由表
                pPrio      优先级
 * Output       :   无
 * Return       :   结果
 *desc      : ip rule add from %s table %s pref 1000
 *************************************************/

int do_iprule_add(char *pFrom, char *pTable, char *pPrio)
{
    struct rtnl_handle rth;
    char* argv[16] = {0};
    int argc = 0;

    argv[argc++] = "from";
    argv[argc++] = pFrom;
    argv[argc++] = "table";
    argv[argc++] = pTable;
    argv[argc++] = "pref";
    argv[argc++] = pPrio;

    memset (&rth, 0, sizeof(struct rtnl_handle));
    rth.fd = -1;
    
    if (rtnl_open(&rth, 0) < 0)
    {
        fprintf (stderr, "do_iprule_del, rtnl_open error.\n");
        return -1;
    }
    
    iprule_modify (&rth, RTM_NEWRULE, argc, argv);

    rtnl_close(&rth);
    return 0;
}


/*************************************************
 * Function     :   do_iprule_del
 * Description  : 删除路由策略
 * Input        :   pTable 路由表
 * Output       :   无
 * Return       :   结果
 *desc      : ip rule del table tab0
 *************************************************/

int do_iprule_del(char *pTable)
{
    struct rtnl_handle rth;
    char* argv[16]= {0};
    int argc = 0;

    argv[argc++] = "table";
    argv[argc++] = pTable;

    memset (&rth, 0, sizeof(struct rtnl_handle));
    rth.fd = -1;
    
    if (rtnl_open(&rth, 0) < 0)
    {
        fprintf (stderr, "do_iprule_del, rtnl_open error.\n");
        return -1;
    }
    
    iprule_modify (&rth, RTM_DELRULE, argc, argv);

    rtnl_close(&rth);
    return 0;
}

/*************************************************
 * Function     :   do_iproute_del
 * Description  : 删除路由表
 * Input        :          pIp   设备地址
                   pTable  路由表
 * Output       :     无
 * Return       :     结果
 *desc      :   ip route del 127.0.0.0/8 table tab0
                ip route del default table tab0
 *************************************************/

int do_iproute_del(char *pIp, char *pTable)
{
    struct rtnl_handle rth;
    char* argv[16]={0};
    int argc = 0;
    char netIp[32] = {0};
    
    strncpy(netIp, pIp, sizeof(netIp));

    argv[argc++] = netIp;
    argv[argc++] = "table";
    argv[argc++] = pTable;

    fprintf (stderr, "do_iproute_del %s.\n", pIp);
    memset (&rth, 0, sizeof(struct rtnl_handle));
    rth.fd = -1;
    
    if (rtnl_open(&rth, 0) < 0)
    {
        fprintf (stderr, "do_iprule_del, rtnl_open error.\n");
        return -1;
    }
    
    iproute_modify(&rth, 25, 0, argc, argv);

    rtnl_close(&rth);
    return 0;
}
#endif

/*************************************************
 * Function     :   rtnl_rtntype_n2a
 * Description  :  修改网卡路由策略相关函数
 * Input        :   id 路由表
                       buf    缓冲
                       len     缓冲长度
 * Output       :   无
 * Return       :   结果
 *************************************************/

char *rtnl_rtntype_n2a(int id, char *buf, int len)
{
    switch (id) {
    case RTN_UNSPEC:
        return "none";
    case RTN_UNICAST:
        return "unicast";
    case RTN_LOCAL:
        return "local";
    case RTN_BROADCAST:
        return "broadcast";
    case RTN_ANYCAST:
        return "anycast";
    case RTN_MULTICAST:
        return "multicast";
    case RTN_BLACKHOLE:
        return "blackhole";
    case RTN_UNREACHABLE:
        return "unreachable";
    case RTN_PROHIBIT:
        return "prohibit";
    case RTN_THROW:
        return "throw";
    case RTN_NAT:
        return "nat";
    case RTN_XRESOLVE:
        return "xresolve";
    default:
        snprintf(buf, len, "%d", id);
        return buf;
    }
}


/*************************************************
 * Function     :   rtnl_rtntype_a2n
 * Description  :  修改网卡路由策略相关函数
 * Input        :   id   路由表
                       arg 参数
 * Output       :   无
 * Return       :   结果
 *************************************************/

int rtnl_rtntype_a2n(int *id, char *arg)
{
    char *end;
    unsigned long res;

    if (strcmp(arg, "local") == 0)
        res = RTN_LOCAL;
    else if (strcmp(arg, "nat") == 0)
        res = RTN_NAT;
    else if (matches(arg, "broadcast") == 0 ||
         strcmp(arg, "brd") == 0)
        res = RTN_BROADCAST;
    else if (matches(arg, "anycast") == 0)
        res = RTN_ANYCAST;
    else if (matches(arg, "multicast") == 0)
        res = RTN_MULTICAST;
    else if (matches(arg, "prohibit") == 0)
        res = RTN_PROHIBIT;
    else if (matches(arg, "unreachable") == 0)
        res = RTN_UNREACHABLE;
    else if (matches(arg, "blackhole") == 0)
        res = RTN_BLACKHOLE;
    else if (matches(arg, "xresolve") == 0)
        res = RTN_XRESOLVE;
    else if (matches(arg, "unicast") == 0)
        res = RTN_UNICAST;
    else if (strcmp(arg, "throw") == 0)
        res = RTN_THROW;
    else {
        res = strtoul(arg, &end, 0);
        if (!end || end == arg || *end || res > 255)
            return -1;
    }
    *id = res;
    return 0;
}


