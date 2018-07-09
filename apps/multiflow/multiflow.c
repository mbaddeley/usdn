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
 *         Multiflow: A flow generator for uSDN testing.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#include <stdio.h>
#include <string.h>

#include "contiki.h"
#include "lib/memb.h"
#include "lib/list.h"
#include "lib/random.h"
#include "net/ip/uip.h"
#include "net/ip/udp-socket.h"
#include "net/ip/uip-udp-packet.h"

#include "multiflow.h"

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "USDN-APP"
#define LOG_LEVEL LOG_LEVEL_SDN

static uint8_t databuffer[UIP_BUFSIZE];
#define UIP_IP_BUF   ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])

LIST(flowlist);
MEMB(flow_memb, mflow_t, USDN_CONF_MAX_APPS);
static uint8_t a_memb_len = 0;

PROCESS(multiflow_process, "Multiflow Process");
/*---------------------------------------------------------------------------*/
/* Initialization */
/*---------------------------------------------------------------------------*/
void
mflow_init(void)
{
  list_init(flowlist);
  memb_init(&flow_memb);
  process_start(&multiflow_process, NULL);
}

/*---------------------------------------------------------------------------*/
/* Memory management */
/*---------------------------------------------------------------------------*/
static mflow_t *
app_allocate()
{
  mflow_t *a;
  a = memb_alloc(&flow_memb);
  if(a == NULL) {
    LOG_ERR("FAILED to allocate a match! (%d/%d)\n",
            a_memb_len, USDN_CONF_MAX_APPS);
  }
  a_memb_len++;
  return a;
}

// /*---------------------------------------------------------------------------*/
// static int
// app_free(mflow_t *a)
// {
//   int res;
//   res = memb_free(&flow_memb, a);
//   if (res != 0){
//     LOG_ERR("FAILED to free a match! Reference count: %d\n",res);
//   } else {
//     a_memb_len--;
//   }
//   return res;
// }

/*---------------------------------------------------------------------------*/
/* Timers*/
/*---------------------------------------------------------------------------*/
static clock_time_t
rand_send_interval(float max, float min)
{
  clock_time_t interval = (min * CLOCK_SECOND) + \
                           ((((max-min) * CLOCK_SECOND) \
                           * (uint32_t)random_rand()) / RANDOM_RAND_MAX);
  return interval;
}

/*---------------------------------------------------------------------------*/
static void
sendtimer_expired(void *ptr) {
  mflow_t *a = (mflow_t *)ptr;
  /* call callback */
  a->timer_callback(a);
}

/*---------------------------------------------------------------------------*/
/* Application functions */
/*---------------------------------------------------------------------------*/
static mflow_t *
new_app(uint8_t id, uip_ipaddr_t *remote_addr, uint16_t lport, uint16_t rport,
        float brdelay, float brmin, float brmax,
        mflow_rx_callback_t receive_callback,
        mflow_timer_callback_t timer_callback)
{
  mflow_t *a;
  if ((a = app_allocate()) != NULL) {
    list_add(flowlist, a);
    a->id = id;
    a->seq = 0;

    a->lport = lport;
    a->rport = rport;
    if(remote_addr != NULL) {
      uip_ipaddr_copy(&a->remote_addr, remote_addr);
    }

    a->brdelay = brdelay;
    a->brmin = brmin;
    a->brmax = brmax;
    a->receive_callback = receive_callback;
    a->timer_callback = timer_callback;;
  }
  return a;
}

/*---------------------------------------------------------------------------*/
static void
connect(mflow_t *a, uint16_t lport, uint16_t rport)
{
  LOG_DBG("app:%d udp conn on ports %d/%d\n", a->id, lport, rport);
  a->state = mflow_STATE_CONNECTING;

  PROCESS_CONTEXT_BEGIN(&multiflow_process);
  a->udp = udp_new(NULL, UIP_HTONS(rport), a); /* set remote port */
  if(a->udp == NULL){
    LOG_ERR("app:%d No udp conn available!\n", a->id);
    a->state = mflow_STATE_ERROR;
    return;
  }
  PROCESS_CONTEXT_END();

  LOG_DBG("app:%d binding udp conn to port %d\n", a->id, lport);
  udp_bind(a->udp, UIP_HTONS(lport)); /* set local port */
  /* Set connector state */
  a->state = mflow_STATE_CONNECTED;
}

/*---------------------------------------------------------------------------*/
/* uSDN Controller Connection */
/*---------------------------------------------------------------------------*/
mflow_t *
mflow_new(uint8_t id, uip_ipaddr_t *remote_addr,
             uint16_t lport, uint16_t rport,
             float brdelay, float brmin, float brmax,
             mflow_rx_callback_t receive_callback,
             mflow_timer_callback_t timer_callback)
{
  mflow_t *a;
  a = new_app(id, remote_addr, lport, rport,
              brdelay, brmin, brmax,
              receive_callback, timer_callback);
  if(a != NULL) {
    connect(a, lport, rport);
  }
  return a;
}

/*---------------------------------------------------------------------------*/
void
mflow_send(mflow_t *a,
              void *data,
              uint8_t datalen)
{
  if(a != NULL && a->udp != NULL) {
    LOG_DBG("app:%d Sending data to [", a->id);
    LOG_DBG_6ADDR(&a->remote_addr);
    LOG_DBG_("]\n");
    a->seq++;
    uip_udp_packet_sendto(a->udp,
                          data,
                          datalen,
                          &a->remote_addr,
                          UIP_HTONS(a->rport));
  } else {
    LOG_ERR("The udp connection was NULL!\n");
  }

}

/*---------------------------------------------------------------------------*/
void
mflow_new_sendinterval(mflow_t *a, uint8_t delay)
{
  clock_time_t interval;

  /* variable br OR connected, but not sending yet */
  if((a->brmin != a->brmax) || (a->state == mflow_STATE_CONNECTED)) {
    interval = rand_send_interval(a->brmax, a->brmin);
    if(delay) {
      interval += (a->brdelay * CLOCK_SECOND);
    }
    LOG_DBG("app:%d new interval:%d.%02lu(s)\n", a->id,
            (int)(interval/CLOCK_SECOND),
            (int)(100L * interval/CLOCK_SECOND) - \
            ((unsigned long)(interval)/CLOCK_SECOND *100));
    ctimer_set(&a->sendtimer, interval, sendtimer_expired, a);
    a->state = mflow_STATE_SENDING;
  /* constant br AND already sending */
  } else {
    ctimer_reset(&a->sendtimer);
  }
}

/*---------------------------------------------------------------------------*/
uint8_t
mflow_num_flows(void)
{
  return list_length(flowlist);
}

/*---------------------------------------------------------------------------*/
/* Multiflow Process */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(multiflow_process, ev, data)
{
  PROCESS_BEGIN();
  LOG_INFO("uSDN-APP process started!\n");

  while(1) {
    /* Wait for event */
    PROCESS_YIELD();
    if (ev == tcpip_event) {
      mflow_t *a = (mflow_t *)data;
      /* Defensive coding: although the appstate *should* be non-null
         here, we make sure to avoid the program crashing on us. */
      if (a != NULL) {
        /* Copy the data from the uIP data buffer into our own
           buffer to avoid the uIP buffer being messed with by the
           callee. */
        if(uip_newdata()) {
          memcpy(databuffer, uip_appdata, uip_datalen());
          /* Call the client process. We use the PROCESS_CONTEXT
             mechanism to temporarily switch process context to the
             client process. */
          PROCESS_CONTEXT_BEGIN(a->process);
          a->receive_callback(a,
                              &(UIP_IP_BUF->srcipaddr),
                              UIP_HTONS(UIP_IP_BUF->srcport),
                              &(UIP_IP_BUF->destipaddr),
                              UIP_HTONS(UIP_IP_BUF->destport),
                              databuffer, uip_datalen());
          PROCESS_CONTEXT_END();
        } else {
          LOG_ERR("No new data at uip...\n");
        }
      } else {
        LOG_ERR("appstate is null!\n");
      }
    /*********************************************/
    } else {
      LOG_ERR("Unknown event type! %02x", ev);
    }
  }
  PROCESS_END();
}
