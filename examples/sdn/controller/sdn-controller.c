/*
 * Copyright (c) 2018, Toshiba Research Europe Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */
/**
 * \file
 *         uSDN: A low-overhead SDN stack and embedded SDN controller for Contiki.
 *         See "Evolving SDN for Low-Power IoT Networks", IEEE NetSoft'18
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include "node-id.h"
#include "contiki.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ip/simple-udp.h"

#include "sys/node-id.h"
#include "sys/ctimer.h"
#include "lib/random.h"

#include "net/rpl/rpl.h"
#include "net/rpl/rpl-private.h"
#include "net/rpl/rpl-dag-root.h"

#if UIP_CONF_IPV6_SDN
#include "net/sdn/sdn.h"
#include "net/sdn/sdn-conf.h"
#include "net/sdn/usdn/usdn.h"
#include "atom.h"
#endif

#if WITH_SDN_STATS
#include "net/sdn/sdn-stats.h"
#endif

#define DEBUG DEBUG_FULL
#include "net/ip/uip-debug.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "NODE"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_LPORT	8765
#define UDP_RPORT	5678

#define CONTROLLER_LPORT 1234
#define CONTROLLER_RPORT 4321

#define UIP_IP_BUF   ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])

static struct simple_udp_connection udp_conn;

PROCESS(sdn_controller_process, "SDN Controller");
AUTOSTART_PROCESSES(&sdn_controller_process);
/*---------------------------------------------------------------------------*/
static void
configure_rpl(uip_ipaddr_t *ipaddr) {
  struct uip_ds6_addr *root_if;

  LOG_INFO("Configuring RPL...\n");
#if RPL_CONF_MOP == RPL_MOP_NON_STORING
  LOG_INFO("RPL is in NON-STORING mode\n");
#else
  LOG_INFO("RPL is in STORING mode\n");
#endif

  /* Make ourselves the root */
  root_if = uip_ds6_addr_lookup(ipaddr);
  if(root_if != NULL) {
    rpl_dag_t *dag;
    dag = rpl_set_root(RPL_DEFAULT_INSTANCE,(uip_ip6addr_t *)ipaddr);
    uip_ip6addr(ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, ipaddr, 64);
    LOG_INFO("Created a new RPL DAG\n");
  }else{
    LOG_ERR("Failed to create RPL DAG!\n");
  }
}

#if UIP_CONF_IPV6_SDN
static void
configure_sdn() {
  uip_ipaddr_t ipaddr;

  LOG_INFO("Configuring SDN...\n");
  sdn_init();
  /* Construct a SDN Controller IP Address. */
  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);
  /* Initialise Atom */
  atom_init(&ipaddr);

  uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0x200, 0, 0, 1);
  SDN_DRIVER.add_accept_on_src(FLOWTABLE, &ipaddr);          /* Ensure our own outbound packets aren't checked by SDN */
  SDN_DRIVER.add_accept_on_icmp6_type(FLOWTABLE, ICMP6_RPL); /* Accept RPL ICMP messages */

}
#endif

/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  char buf[15] = {0};
  int pos = 0;
#if UIP_CONF_IPV6_SDN
  usdn_hdr_t hdr;
  memcpy(&hdr, data, USDN_H_LEN);
  pos += USDN_H_LEN;
#endif
  memcpy(&buf, data + pos, datalen);
  LOG_STAT("RX APP s:%d d:%d %s h:%d\n",
    sender_addr->u8[15],
    node_id,
    (char *)buf,
    uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl + 1);
#if SERVER_REPLY
  LOG_STAT("Sending response %s to %u\n",
    (char *)buf,
    sender_addr->u8[15]);
  simple_udp_sendto(&udp_conn, data, datalen, sender_addr);
#endif /* SERVER_REPLY */
}
/*-------------------------------------------FTS--------------------------------*/
PROCESS_THREAD(sdn_controller_process, ev, data)
{
  static uip_ipaddr_t ipaddr;

  PROCESS_BEGIN();
  LOG_INFO("Starting sdn_controller_process\n");

  /* Construct an IPv6 address from eight 16-bit words. */
  uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
  /* Set the IPv6 address based on the node's MAC address */
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  configure_rpl(&ipaddr);

  /* Initialize SDN */
#if UIP_CONF_IPV6_SDN
  configure_sdn();
#endif

  /* Spit out SDN statistics */
#if WITH_SDN_STATS
  sdn_stats_start(CLOCK_SECOND * SDN_STATS_PERIOD);
#endif

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_LPORT, NULL,
                      UDP_RPORT, udp_rx_callback);

#if UIP_CONF_IPV6_SDN
  LOG_DBG("Forcing controller update!\n");
  SDN_ENGINE.controller_update(SDN_TMR_STATE_IMMEDIATE);
#endif

    /* Main loop for events */
  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {
      // rpl_dag_t *dag = rpl_get_any_dag();
      // ANNOTATE("#A rt=%d/%d, rnk=%d\n", uip_ds6_route_num_routes(),
      //                                   UIP_CONF_MAX_ROUTES,
      //                                   DAG_RANK(dag->instance->current_dag->rank, dag->instance));
      LOG_INFO("Received tcpip_event\n");
    }
  }

  LOG_INFO("Ending sdn_controller_process\n");
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
