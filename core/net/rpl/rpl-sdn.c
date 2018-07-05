/**
* \file
*         Callback functions for SDN / RPL integration.
* \author
*         Michael Baddeley <m.baddeley@bristol.ac.uk>
*/

# if UIP_CONF_IPV6_RPL
#include "net/rpl/rpl.h"
#include "net/rpl/rpl-private.h"

// FIXME: Not the most elegant way to say we have already received a dao for each node...
uint8_t rpl_sdn_dao_received[50] = {0};

#if UIP_CONF_IPV6_SDN

/* Log configuration */
#include "sys/log-ng.h"
#define LOG_MODULE "SDN-RPL"
#define LOG_LEVEL LOG_LEVEL_NONE

rpl_sdn_dag_joined_callback_t dag_joined_callback;

/*---------------------------------------------------------------------------*/
void
register_dag_joined_callback(rpl_sdn_dag_joined_callback_t callback)
{
  dag_joined_callback = callback;
}

/*---------------------------------------------------------------------------*/
void
rpl_sdn_dag_joined_callback(void)
{
  LOG_DBG("SDN-RPL: Calling SDN dag_joined_callback\n");
  dag_joined_callback();
}

/*---------------------------------------------------------------------------*/
void
rpl_sdn_set_instance_properties(uint8_t dio_interval, uint8_t dfrt_lifetime)
{
  rpl_instance_t *instance;
  rpl_instance_t *end;
  LOG_DBG("SDN-RPL: Setting RPL instance properties\n");
  for(instance = &instance_table[0], end = instance + RPL_MAX_INSTANCES;
      instance < end; ++instance) {
    if(instance->used == 1) {
      // instance->dio_intmin = 32;
      // instance->dio_intdoubl = 20;
      // instance->default_lifetime = dfrt_lifetime;
      LOG_DBG("SDN-RPL: RPL properties (temp not... FIXME) updated (dio_intmin:%d, dio_intdbl: %d, dfrt_lt:%d)\n",
              instance->dio_intmin,
              instance->dio_intdoubl,
              instance->default_lifetime);
      // LOG_DBG("SDN-RPL: Turning off dio timer...");
      // ctimer_stop(&instance->dio_timer);
    }
  }
}

#endif /* UIP_CONF_IPV6_SDN */

#endif /* UIP_CONF_IPV6_RPL */
