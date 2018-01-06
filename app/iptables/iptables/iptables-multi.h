#ifndef _IPTABLES_MULTI_H
#define _IPTABLES_MULTI_H 1

#include "xtables.h"

extern int ip4tables_main(int, char **);
extern int iptables_save_main(int, char **);
extern int iptables_restore_main(int, char **);

#endif /* _IPTABLES_MULTI_H */
