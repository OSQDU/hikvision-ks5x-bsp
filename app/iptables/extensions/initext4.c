
extern void libipt_CLUSTERIP_init(void);
extern int libipt_DNAT_init(void);
extern void libipt_ECN_init(void);
extern void libipt_LOG_init(void);
extern int libipt_MASQUERADE_init(void);
extern void libipt_MIRROR_init(void);
extern void libipt_NETMAP_init(void);
extern void libipt_REDIRECT_init(void);
extern void libipt_REJECT_init(void);
extern void libipt_SAME_init(void);
extern int libipt_SNAT_init(void);
//extern void libipt_TTL_init(void);
extern void libipt_ULOG_init(void);
extern void libipt_ah_init(void);
extern void libipt_icmp_init(void);
extern void libipt_realm_init(void);
extern void libipt_ttl_init(void);
extern void libipt_unclean_init(void);
int init_extensions4(void);
int init_extensions4(void)
{
    if(libipt_DNAT_init() != 0)
    {
        return -1;
    }
    if(libipt_MASQUERADE_init() != 0)
    {
        return -1;
    }
    if(libipt_SNAT_init() != 0)
    {
        return -1;
    }
    #if 0
    libipt_CLUSTERIP_init();
    libipt_ECN_init();
    libipt_LOG_init();
    libipt_MIRROR_init();
    libipt_NETMAP_init();
    libipt_REDIRECT_init();
    libipt_REJECT_init();
    libipt_SAME_init();
    //libipt_TTL_init();
    libipt_ULOG_init();
    libipt_ah_init();
    libipt_icmp_init();
    libipt_realm_init();
    libipt_ttl_init();
    libipt_unclean_init();
    #endif

    return 0;
}

