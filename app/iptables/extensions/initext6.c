
extern int libip6t_DNAT_init(void);
extern void libip6t_DNPT_init(void);
//extern void libip6t_HL_init(void);
extern void libip6t_LOG_init(void);
extern int  libip6t_MASQUERADE_init(void);
extern void libip6t_NETMAP_init(void);
extern void libip6t_REDIRECT_init(void);
extern void libip6t_REJECT_init(void);
extern int  libip6t_SNAT_init(void);
extern void libip6t_SNPT_init(void);
extern void libip6t_ah_init(void);
extern void libip6t_dst_init(void);
extern void libip6t_eui64_init(void);
extern void libip6t_frag_init(void);
extern void libip6t_hbh_init(void);
extern void libip6t_hl_init(void);
extern void libip6t_icmp6_init(void);
extern void libip6t_ipv6header_init(void);
extern void libip6t_mh_init(void);
extern void libip6t_rt_init(void);
int init_extensions6(void);
int init_extensions6(void)
{
    if(libip6t_DNAT_init() != 0)
    {
        return -1;
    }
    if(libip6t_MASQUERADE_init() != 0)
    {
        return -1;
    }
    if(libip6t_SNAT_init() != 0)
    {
        return -1;
    }
    #if 0
    libip6t_DNPT_init();
    //libip6t_HL_init();
    libip6t_LOG_init();
    libip6t_NETMAP_init();
    libip6t_REDIRECT_init();
    libip6t_REJECT_init();
    libip6t_SNPT_init();
    libip6t_ah_init();
    libip6t_dst_init();
    libip6t_eui64_init();
    libip6t_frag_init();
    libip6t_hbh_init();
    libip6t_hl_init();
    libip6t_icmp6_init();
    libip6t_ipv6header_init();
    libip6t_rt_init();
    #endif

    return 0;
}

