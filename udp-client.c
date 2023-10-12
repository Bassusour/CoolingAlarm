#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <stdint.h>
#include "dht11-sensor.h"
#include <inttypes.h>

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define SEND_INTERVAL		  (10 * CLOCK_SECOND)

#define DHT11_GPIO_PORT (1)
#define DHT11_GPIO_PIN  (12)

static struct simple_udp_connection udp_conn;
static uint32_t rx_count = 0;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
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

  LOG_INFO("Received response '%.*s' from ", datalen, (char *) data);
  LOG_INFO_6ADDR(sender_addr);
#if LLSEC802154_CONF_ENABLED
  LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#endif
  LOG_INFO_("\n");
  rx_count++;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer temp_timer;
  static char str[32];
  uip_ipaddr_t dest_ipaddr;

  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  dht11_sensor.configure(DHT11_CONFIGURE_GPIO_PORT, DHT11_GPIO_PORT);
  dht11_sensor.configure(DHT11_CONFIGURE_GPIO_PIN, DHT11_GPIO_PIN);
  dht11_sensor.configure(SENSORS_HW_INIT, 0);

  /* Wait one second for the DHT11 sensor to be ready */
  etimer_set(&temp_timer, CLOCK_SECOND * 1);

  /* Wait for the periodic timer to expire */
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&temp_timer));

  /* Setup a periodic timer that expires after 5 seconds. */
  etimer_set(&temp_timer, CLOCK_SECOND * 5);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&temp_timer));
    printf("After timer");

    SENSORS_ACTIVATE(dht11_sensor);
    if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
      printf("Wagwan blud");
      // /* Print statistics every 10th TX */
      // if(tx_count % 10 == 0) {
      //   LOG_INFO("Tx/Rx/MissedTx: %" PRIu32 "/%" PRIu32 "/%" PRIu32 "\n",
      //            tx_count, rx_count, missed_tx_count);
      // }

      uint32_t tmp = 0;

      switch(dht11_sensor.status(0)) {
        case DHT11_STATUS_OKAY:
          tmp = dht11_sensor.value(DHT11_VALUE_TEMPERATURE_INTEGER);
          LOG_INFO("Sending temperature %"PRIu32" to ", tmp);
          LOG_INFO_6ADDR(&dest_ipaddr);
          LOG_INFO_("\n");
          snprintf(str, sizeof(str), "%" PRIu32 "", tmp);
          simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);
          break;
        case DHT11_STATUS_CHECKSUM_FAILED:
          printf("Check sum failed\n");
          break;
        case DHT11_STATUS_TIMEOUT:
          printf("Reading timed out\n");
          break;
        default:
          break;
      }

      /* Send to DAG root */
      // LOG_INFO("Sending request %"PRIu32" to ", tx_count);
      // LOG_INFO_6ADDR(&dest_ipaddr);
      // LOG_INFO_("\n");
      // snprintf(str, sizeof(str), "%" PRIu32 "", tx_count);
      // simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);
      // tx_count++;
    } else {
      LOG_INFO("Not reachable yet\n");
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
