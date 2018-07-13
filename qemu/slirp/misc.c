/*
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#include "qemu/osdep.h"
#include "slirp.h"
#include "libslirp.h"
#include "monitor/monitor.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"

#ifdef DEBUG
int slirp_debug = DBG_CALL|DBG_MISC|DBG_ERROR;
#endif

inline void
insque(void *a, void *b)
{
	register struct quehead *element = (struct quehead *) a;
	register struct quehead *head = (struct quehead *) b;
	element->qh_link = head->qh_link;
	head->qh_link = (struct quehead *)element;
	element->qh_rlink = (struct quehead *)head;
	((struct quehead *)(element->qh_link))->qh_rlink
	= (struct quehead *)element;
}

inline void
remque(void *a)
{
  register struct quehead *element = (struct quehead *) a;
  ((struct quehead *)(element->qh_link))->qh_rlink = element->qh_rlink;
  ((struct quehead *)(element->qh_rlink))->qh_link = element->qh_link;
  element->qh_rlink = NULL;
}

void slirp_connection_info(Slirp *slirp, Monitor *mon)
{
    const char * const tcpstates[] = {
        [TCPS_CLOSED]       = "CLOSED",
        [TCPS_LISTEN]       = "LISTEN",
        [TCPS_SYN_SENT]     = "SYN_SENT",
        [TCPS_SYN_RECEIVED] = "SYN_RCVD",
        [TCPS_ESTABLISHED]  = "ESTABLISHED",
        [TCPS_CLOSE_WAIT]   = "CLOSE_WAIT",
        [TCPS_FIN_WAIT_1]   = "FIN_WAIT_1",
        [TCPS_CLOSING]      = "CLOSING",
        [TCPS_LAST_ACK]     = "LAST_ACK",
        [TCPS_FIN_WAIT_2]   = "FIN_WAIT_2",
        [TCPS_TIME_WAIT]    = "TIME_WAIT",
    };
    struct in_addr dst_addr;
    struct sockaddr_in src;
    socklen_t src_len;
    uint16_t dst_port;
    struct socket *so;
    const char *state;
    char buf[20];

    monitor_printf(mon, "  Protocol[State]    FD  Source Address  Port   "
                        "Dest. Address  Port RecvQ SendQ\n");

    for (so = slirp->tcb.so_next; so != &slirp->tcb; so = so->so_next) {
        if (so->so_state & SS_HOSTFWD) {
            state = "HOST_FORWARD";
        } else if (so->so_tcpcb) {
            state = tcpstates[so->so_tcpcb->t_state];
        } else {
            state = "NONE";
        }
        if (so->so_state & (SS_HOSTFWD | SS_INCOMING)) {
            src_len = sizeof(src);
            getsockname(so->s, (struct sockaddr *)&src, &src_len);
            dst_addr = so->so_laddr;
            dst_port = so->so_lport;
        } else {
            src.sin_addr = so->so_laddr;
            src.sin_port = so->so_lport;
            dst_addr = so->so_faddr;
            dst_port = so->so_fport;
        }
        snprintf(buf, sizeof(buf), "  TCP[%s]", state);
        monitor_printf(mon, "%-19s %3d %15s %5d ", buf, so->s,
                       src.sin_addr.s_addr ? inet_ntoa(src.sin_addr) : "*",
                       ntohs(src.sin_port));
        monitor_printf(mon, "%15s %5d %5d %5d\n",
                       inet_ntoa(dst_addr), ntohs(dst_port),
                       so->so_rcv.sb_cc, so->so_snd.sb_cc);
    }

    for (so = slirp->udb.so_next; so != &slirp->udb; so = so->so_next) {
        if (so->so_state & SS_HOSTFWD) {
            snprintf(buf, sizeof(buf), "  UDP[HOST_FORWARD]");
            src_len = sizeof(src);
            getsockname(so->s, (struct sockaddr *)&src, &src_len);
            dst_addr = so->so_laddr;
            dst_port = so->so_lport;
        } else {
            snprintf(buf, sizeof(buf), "  UDP[%d sec]",
                         (so->so_expire - curtime) / 1000);
            src.sin_addr = so->so_laddr;
            src.sin_port = so->so_lport;
            dst_addr = so->so_faddr;
            dst_port = so->so_fport;
        }
        monitor_printf(mon, "%-19s %3d %15s %5d ", buf, so->s,
                       src.sin_addr.s_addr ? inet_ntoa(src.sin_addr) : "*",
                       ntohs(src.sin_port));
        monitor_printf(mon, "%15s %5d %5d %5d\n",
                       inet_ntoa(dst_addr), ntohs(dst_port),
                       so->so_rcv.sb_cc, so->so_snd.sb_cc);
    }

    for (so = slirp->icmp.so_next; so != &slirp->icmp; so = so->so_next) {
        snprintf(buf, sizeof(buf), "  ICMP[%d sec]",
                     (so->so_expire - curtime) / 1000);
        src.sin_addr = so->so_laddr;
        dst_addr = so->so_faddr;
        monitor_printf(mon, "%-19s %3d %15s  -    ", buf, so->s,
                       src.sin_addr.s_addr ? inet_ntoa(src.sin_addr) : "*");
        monitor_printf(mon, "%15s  -    %5d %5d\n", inet_ntoa(dst_addr),
                       so->so_rcv.sb_cc, so->so_snd.sb_cc);
    }
}
