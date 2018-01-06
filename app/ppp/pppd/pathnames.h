/*
 * define path names
 *
 * $Id: pathnames.h,v 1.3 2007/03/24 05:00:11 administrator Exp $
 */

#ifdef HAVE_PATHS_H
#include <paths.h>
#undef _PATH_VARRUN
#define _PATH_VARRUN 	"/home/"
#else /* HAVE_PATHS_H */
#define _PATH_VARRUN 	"/home/"
#define _PATH_DEVNULL	"/dev/null"
#endif /* HAVE_PATHS_H */

#ifndef _ROOT_PATH
#define _ROOT_PATH
#endif

#define _PATH_UPAPFILE 	 _ROOT_PATH "/home/pap-secrets"
#define _PATH_CHAPFILE 	 _ROOT_PATH "/home/chap-secrets"
#define _PATH_SRPFILE 	 _ROOT_PATH "/home/srp-secrets"
#define _PATH_SYSOPTIONS _ROOT_PATH "/home/options"
#define _PATH_IPUP	 _ROOT_PATH "/home/ip-up"
#define _PATH_IPDOWN	 _ROOT_PATH "/home/ip-down"
#define _PATH_AUTHUP	 _ROOT_PATH "/home/auth-up"
#define _PATH_AUTHDOWN	 _ROOT_PATH "/home/auth-down"
#define _PATH_TTYOPT	 _ROOT_PATH "/home/options."
#define _PATH_CONNERRS	 _ROOT_PATH "/home/connect-errors"
#define _PATH_PEERFILES	 _ROOT_PATH "/home/peers/"
#define _PATH_RESOLV	 _ROOT_PATH "/etc/resolv.conf"

#define _PATH_USEROPT	 ".ppprc"
#define	_PATH_PSEUDONYM	 ".ppp_pseudonym"

#ifdef INET6
#define _PATH_IPV6UP     _ROOT_PATH "/home/ipv6-up"
#define _PATH_IPV6DOWN   _ROOT_PATH "/home/ipv6-down"
#endif

#ifdef IPX_CHANGE
#define _PATH_IPXUP	 _ROOT_PATH "/home/ipx-up"
#define _PATH_IPXDOWN	 _ROOT_PATH "/home/ipx-down"
#endif /* IPX_CHANGE */

#ifdef __STDC__
//#define _PATH_PPPDB	_ROOT_PATH _PATH_VARRUN "pppd2.tdb"
#define _PATH_PPPDB	"/home/pppd2.tdb"
#else /* __STDC__ */
#ifdef HAVE_PATHS_H
#define _PATH_PPPDB	"/home/pppd2.tdb"
#else
#define _PATH_PPPDB	"/home/pppd2.tdb"
#endif
#endif /* __STDC__ */

#ifdef PLUGIN
#define _PATH_PLUGIN	"/usr/lib/pppd/" VERSION
#endif /* PLUGIN */
