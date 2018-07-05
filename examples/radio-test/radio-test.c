#include "contiki.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "sys/node-id.h"
#include "net/netstack.h"

#include <stdio.h> /* For printf() */

PROCESS(radio_process, "Radio test process");
AUTOSTART_PROCESSES(&radio_process);

PROCESS_THREAD(radio_process, ev, data)
{
  static uint8_t packet[125] = {0, 1, 2, 3, 4};
  radio_value_t radio_tx_mode;
  static struct etimer et;
  uint8_t channel;

  PROCESS_BEGIN();

#if 0
  /* node 11 gets channel 11, node 12 gets 12 and so on... */
  channel = node_id;
  if(channel < 11 || channel > 26) {
    channel = (channel % 16) + 11;
  }
#else
  channel = 26;
#endif

  NETSTACK_RADIO.init();

  NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, channel);

  /* Unset CCA */
  NETSTACK_RADIO.get_value(RADIO_PARAM_TX_MODE, &radio_tx_mode);
  radio_tx_mode &= ~RADIO_TX_MODE_SEND_ON_CCA;
  NETSTACK_RADIO.set_value(RADIO_PARAM_TX_MODE, radio_tx_mode);

  NETSTACK_RADIO.on();

  watchdog_stop();

#if 0 /* rtimer based */

// #define SEND_DURATION (RTIMER_SECOND / 100)  /* 10 ms */
// #define SEND_DELAY    (RTIMER_SECOND / 2000) /* around 0.5 ms */
#define SEND_DURATION (RTIMER_SECOND / 10)  /* 100ms */
#define SEND_DELAY    (RTIMER_SECOND / 1) /* 1s */


  for (;;) {
    rtimer_clock_t end = RTIMER_NOW() + SEND_DURATION;

    /* send packet burst */
    while(RTIMER_CLOCK_LT(RTIMER_NOW(), end)) {
      /* send to radio */
      NETSTACK_RADIO.send(&packet, sizeof(packet));

      /* extra delay */
      /* rtimer_clock_t delay = RTIMER_NOW() + 10; */
      /*  while(RTIMER_CLOCK_LT(RTIMER_NOW(), delay)); */
    }

    end = RTIMER_NOW() + SEND_DELAY;

    /* delay between packet bursts */
    while(RTIMER_CLOCK_LT(RTIMER_NOW(), end));
  }

#else /* etimer based */

  for (;;) {
    etimer_set(&et, (CLOCK_SECOND / 10));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    NETSTACK_RADIO.send(&packet, sizeof(packet));
    etimer_restart(&et);
  }
#endif

  PROCESS_END();
}
