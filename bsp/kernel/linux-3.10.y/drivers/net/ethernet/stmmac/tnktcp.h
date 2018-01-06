/*
 *  COPYRIGHT NOTICE
 *
 *  Copyright 2011 Emutex Ltd. All rights reserved.
 *
 *  tnk info structure
 */

#ifndef __TNKTCP_H
#define __TNKTCP_H

#ifndef UNIT_TEST
#include <net/tnkdrv.h>
#endif

#include "tnkinfo.h"
extern unsigned  int tnk_buf_sum[256];

void tnk_tcp_init(void);
void tnk_tcp_shutdown(void);
int tnk_tcp_prepare(struct sock *sk, struct sk_buff *skb);
int tnk_tcp_open(struct sock *sk);
int tnk_tcp_close(struct sock *sk, int graceful);
int tnk_tcp_reset(struct sock *sk);
int tnk_tcp_receive(struct sock *sk, uint32_t *seq, long *timeo,
		    int target, struct iovec *to, int *has_copied,
		    size_t *len, int fin_flag, int sk_flags);
int tnk_tcp_read_sock(struct sock *sk, read_descriptor_t *desc,
		      tnk_read_actor_t recv_actor, int *has_copied);

int tnk_tcp_sendpages(struct sock *sk, struct page *page, int offset,
		      size_t size, int flags);
int tnk_tcp_sendmsg(struct sock *sk, struct iovec *from, int len, int flags);
int tnk_tcp_update(struct sock *sk, int len);
int tnk_tcp_wait_data(struct sock *sk, long *timeo);
int tnk_tcp_check_fin(struct sock *sk);
int tnk_tcp_check_connect_state(struct sock *sk);
void tnk_tcp_disable_rcv(struct sock *sk);
void tnk_tcp_sync_mss(struct sock *sk);
int tnk_tcp_recv_queue_data_size(struct sock *sk);
void tnk_tcp_skb_build_tcpheader(struct sk_buff *skb);
void tnk_tcp_set_nagle_cork(struct sock *sk);
void tnk_tcp_set_thin_linear_timeouts(struct sock *sk);
#if SWITCH_ZERO_PROBE
int tnk_tcp_check_max_retries(struct tnkentry *e, struct sock **sk);
#endif
#if SWITCH_KEEPALIVE
int tnk_tcp_check_tx_queue(struct sock *sk);
int tnk_tcp_keepalive(struct sock *sk);
#endif
#if SWITCH_SEND_FIN
int tnk_tcp_send_fin(struct sock *sk);
#endif
#if SWITCH_MULTI_INTR
void tnk_check_cpu_change(struct sock *sk);
#endif

#endif
