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
 *         uSDN Core: Timer configuration and macros.
 * \author
 *         Michael Baddeley <m.baddeley@bristol.ac.uk>
 */
#ifndef SDN_TIMERS_H_
#define SDN_TIMERS_H_

#include "lib/random.h"
#include "sdn-cd.h"

#define SDN_TIMER_STRING(S) \
  ((S == SDN_TMR_STATE_STOP) ? ("STOP") : \
  (S == SDN_TMR_STATE_START) ? ("START") : \
  (S == SDN_TMR_STATE_RESET) ? ("RESET") : \
  (S == SDN_TMR_STATE_IMMEDIATE) ? ("NOW") : "UNKWN")

/* Timer Delays */
#ifndef  SDN_CONF_NSU_DELAY_MAX
#define  SDN_CONF_NSU_DELAY_MAX  10
#endif

#ifndef  SDN_CONF_JOIN_DELAY_MIN
#define  SDN_CONF_JOIN_DELAY_MIN 10
#endif
#ifndef  SDN_CONF_JOIN_DELAY_MAX
#define  SDN_CONF_JOIN_DELAY_MAX 15
#endif

#define RAND_BETWEEN(MAX, MIN)  (int)((MIN * CLOCK_SECOND) +                  \
                                ((((MAX - MIN) * CLOCK_SECOND) *              \
                                (uint32_t)random_rand()) / RANDOM_RAND_MAX))

#define RAND_UP_TO(n)           (int)(((n * CLOCK_SECOND) * (uint32_t)random_rand()) / RANDOM_RAND_MAX)

#if SDN_CONF_NSU_DELAY_MAX
#define SDN_RANDOM_NSU_DELAY()  RAND_UP_TO(SDN_CONF_NSU_DELAY_MAX)
#else
#define SDN_RANDOM_NSU_DELAY()  0
#endif
#if SDN_CONF_JOIN_DELAY_MAX
#define SDN_RANDOM_JOIN_DELAY() RAND_BETWEEN(SDN_CONF_JOIN_DELAY_MAX, SDN_CONF_JOIN_DELAY_MIN)
#else
#define SDN_RANDOM_JOIN_DELAY() 0
#endif

/* ICMP6 SDN Timers API */
void sdn_tmr_cis(sdn_tmr_state_t state);
void sdn_tmr_cio(sdn_tmr_state_t state, uip_ipaddr_t *dest);

#endif /* SDN_TIMERS_H_ */
