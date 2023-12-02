/*
 * ethconn.cpp
 *
 *  Created on: Mar 31, 2023
 *      Author: anh
 */


#include "ethconn/ethconn.h"

#include "stdio.h"
#include "log/log.h"


/**
 * Extern ethernet, net interface struct and variable.
 */
static const char *TAG = "ETHCONN";
extern struct netif gnetif;
extern ETH_HandleTypeDef heth;

extern ip4_addr_t ipaddr;
extern ip4_addr_t netmask;
extern ip4_addr_t gw;

void (*link_handler)(ethernet_event_t) = NULL;


static __IO uint32_t _waitIP_timeout = 0xFFFFFFFF;
static bool connected = false;

static void link_event_handler(struct netif *);
static void tcpip_init_done(void *);
/**
 * Get global net interface and ethernet handler API.
 */
struct netif *ethernet_get_netif(void){
	return (&gnetif);
}
ETH_HandleTypeDef *ethernet_get_ethernet(void){
	return (&heth);
}

void ethernet_register_event_handler(void (*link_event_handler_function)(ethernet_event_t)){
	link_handler = link_event_handler_function;
}

static void link_event_handler(struct netif *netif){
	if (netif_is_up(netif) || netif_is_link_up(netif)){
		netif_set_up(&gnetif);

		if(link_handler) link_handler(ETHERNET_EVENT_LINKUP);

		dhcp_start(&gnetif);

		ethernet_wait_ip_address();
	}
	else {
//		netif_set_down(&gnetif);

		if(link_handler) link_handler(ETHERNET_EVENT_LINKDOWN);
	}
}


/**
 * Initialize and link.
 */
void ethernet_init_link(uint32_t waitIP_timeout){
	_waitIP_timeout = waitIP_timeout;
	/* Initialize the LwIP stack */
	tcpip_init(tcpip_init_done, NULL);

	/* IP addresses initialization with DHCP (IPv4) */
	ipaddr.addr = 0;
	netmask.addr = 0;
	gw.addr = 0;

	/* Add the netconn interface (IPv4/IPv6) */
	netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);

	/* Registers the default netconn interface */
	netif_set_default(&gnetif);

	/* Set the link event handler function*/
	netif_set_link_callback(&gnetif, link_event_handler);

	/* Start task check link event */
	xTaskCreate(ethernet_link_thread, "ethernet_link_thread", ETHERNET_LINK_TASK_SIZE_KBYTE*1024/4, &gnetif, ETHERNET_LINK_TASK_PRIORITY, NULL);

	LOG_INFO(TAG, "Ethernet initialized, waiting for link up.");
}

bool ethernet_linked_up(void){
	return connected;
}


static void show_ip_address(void){
	static const char *TAG = "ETHIP";

	LOG_INFO(TAG, "Device MAC : %02x:%02x:%02x:%02x:%02x:%02x",
					gnetif.hwaddr[0], gnetif.hwaddr[1], gnetif.hwaddr[2], gnetif.hwaddr[3], gnetif.hwaddr[4], gnetif.hwaddr[5]);
	char ip_str[16];
	sprintf(ip_str, "%s", ip4addr_ntoa(netif_ip4_addr(&gnetif)));
	LOG_INFO(TAG, "IP address : %s", ip_str);
	sprintf(ip_str, "%s", ip4addr_ntoa(netif_ip4_netmask(&gnetif)));
	LOG_INFO(TAG, "Subnet mask: %s", ip_str);
	sprintf(ip_str, "%s", ip4addr_ntoa(netif_ip4_gw(&gnetif)));
	LOG_INFO(TAG, "Gateway    : %s", ip_str);

}

void ethernet_wait_ip_address(void){
	__IO uint32_t tick = HAL_GetTick();
	while(!dhcp_supplied_address(&gnetif)){
		vTaskDelay(100);
		if(HAL_GetTick() - tick > _waitIP_timeout){
			LOG_ERROR(TAG, "Can't get IP address form dhcp, access to network failed.");

			netif_set_down(&gnetif);
			if(link_handler) link_handler(ETHERNET_EVENT_LINKDOWN);

			return;
		}
	}

	show_ip_address();
	if(link_handler) link_handler(ETHERNET_EVENT_GOTIP);
}

static void tcpip_init_done(void *){
	LOG_INFO(TAG, "TCP/IP Stack initialize done.");
}

/**
 * DHCP API.
 */
void ethernet_dhcp_start(void){
	dhcp_start(&gnetif);
}
void ethernet_dhcp_stop(void){
	dhcp_stop(&gnetif);
}

/**
 * Address API.
 */
// MAC address.
uint8_t *ethernet_get_mac_address(void){
	return (uint8_t *)(&gnetif.hwaddr[0]);
}
char *ethernet_get_mac_address_str(void){
	char *mac = (char *)malloc(18);

	sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
			gnetif.hwaddr[0], gnetif.hwaddr[1], gnetif.hwaddr[2], gnetif.hwaddr[3], gnetif.hwaddr[4], gnetif.hwaddr[5]);

	return mac;
}

// IP address.
void ethernet_set_ip_address(ip_addr_t ip, ip_addr_t netmask, ip_addr_t gateway){
	netif_set_addr(&gnetif, (ip4_addr_t *)&ip, (ip4_addr_t *)&netmask, (ip4_addr_t *)&gateway);

	show_ip_address();
	if(link_handler) link_handler(ETHERNET_EVENT_GOTIP);
}
void ethernet_set_ip_address_str(char *ip, char *netmask, char *gateway){
	ip4_addr_t _ip, _netmask, _gateway;

	ip4addr_aton(ip,      &_ip);
	ip4addr_aton(netmask, &_netmask);
	ip4addr_aton(gateway, &_gateway);

	netif_set_addr(&gnetif, &_ip, &_netmask, &_gateway);

	show_ip_address();
	if(link_handler) link_handler(ETHERNET_EVENT_GOTIP);
}
ip_addr_t *ethernet_get_ip_address(void){
	return (ip_addr_t *)netif_ip4_addr(&gnetif);
}
char *ethernet_get_ip_address_str(void){
	return ip4addr_ntoa(netif_ip4_addr(&gnetif));
}

// Subnet mask.
ip_addr_t *ethernet_get_netmask(void){
	return (ip_addr_t *)netif_ip4_netmask(&gnetif);
}
char *ethernet_get_netmask_str(void){
	return ip4addr_ntoa(netif_ip4_netmask(&gnetif));
}

// Gateway address.
ip_addr_t *ethernet_get_gateway(void){
	return (ip_addr_t *)netif_ip4_gw(&gnetif);
}
char *ethernet_get_gateway_str(void){
	return ip4addr_ntoa(netif_ip4_gw(&gnetif));
}




