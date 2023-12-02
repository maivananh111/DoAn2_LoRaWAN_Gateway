/*
 * mqttc.h
 *
 *  Created on: Nov 8, 2023
 *      Author: anh
 */

#ifndef MQTTC_MQTTC_H_
#define MQTTC_MQTTC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "apps/mqtt.h"
#include "lwip/err.h"

typedef enum {
	MQTTC_EVENT_CONNECTED = 0,
	MQTTC_EVENT_DISCONNECTED = 1,

	MQTTC_EVENT_ERROR_TIMEOUT = 2,
	MQTTC_EVENT_ERROR_IDENTIFIER = 3,
	MQTTC_EVENT_ERROR_REFUSED_SERVER = 4,
	MQTTC_EVENT_ERROR_AUTHORIZE = 5,
	MQTTC_EVENT_ERROR_ACCESS_DENIED = 6,
	MQTTC_EVENT_ERROR_REFUSED_PROTOCOL_VERSION = 7,

	MQTTC_EVENT_TOPIC_SUBSCRIBED = 8,
	MQTTC_EVENT_TOPIC_UNSUBSCRIBED = 9,

	MQTTC_EVENT_TOPIC_PUBLISHED = 10,

	MQTTC_EVENT_TOPIC_DATA = 11,
} mqttc_eventid_t;

typedef struct {
	const char *broker_hostname = NULL;
	ip_addr_t broker_ipaddr;
	uint16_t port_number = MQTT_PORT;
	const char *client_id = NULL;
	const uint16_t keep_alive_s = 0U;
} mqttc_config_t;

typedef struct {
	const char *username = NULL;
	const char *password = NULL;
} mqttc_auth_t;

typedef struct {
	mqttc_eventid_t eventid = MQTTC_EVENT_DISCONNECTED;
	void *data = NULL;
} mqttc_event_t;

typedef void (*mqttc_event_handler_f)(mqttc_event_t event, void *param);

class mqttc_t {
public:
	mqttc_t(void);

	err_t initialize(mqttc_config_t *pconfig = NULL);
	err_t deinitialize(void);

	err_t config_client_auth(mqttc_auth_t *pauth = NULL);
	err_t register_event_handler(mqttc_event_handler_f event_handler_function =
			NULL, void *event_arg = NULL);
	err_t unregister_event_handler(void);

	err_t connect_to_broker(bool auto_reconnect = false);
	err_t disconnect_to_broker(void);

	bool isconnected(void);

	err_t set_option(uint8_t qos = 0, uint8_t retain = 0);

	err_t publish(const char *topic, const char *topic_data,
			uint16_t topic_data_len);
	err_t publish(const char *topic, const char *topic_data);

	err_t subcribe(const char *topic);
	err_t unsubcribe(const char *topic);

	void event_handler(mqttc_event_t event);

private:
	mqtt_client_t *_mqttc = NULL;
	struct mqtt_connect_client_info_t *_mqttc_info = NULL;

	mqttc_config_t *_conf = NULL;
	mqttc_auth_t *_auth = NULL;

	mqttc_event_handler_f _event_handler = NULL;
	void *_evparam = NULL;
	void *_evdata = NULL;

	uint8_t _qos = 0;
	uint8_t _retain = 0;

	bool _reconnect = false;
};

#ifdef __cplusplus
}
#endif

#endif /* MQTTC_MQTTC_H_ */
