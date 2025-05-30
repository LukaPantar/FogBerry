
#ifndef MAIN_H
#define MAIN_H

#define mainHW_INIT_DELAY_MS 5000

#define mainADA161_LIGHT_SENSOR_PIN 26
#define mainADA161_LIGHT_SENSOR_ADC_PIN 0

#define mainMQ7_GAS_SENSOR_PIN 27
#define mainMQ7_GAS_SENSOR_ADC_PIN 1

#define mainMFRC522_CARD_TAG_0 0x22
#define mainMFRC522_CARD_TAG_1 0x41
#define mainMFRC522_CARD_TAG_2 0xCC
#define mainMFRC522_CARD_TAG_3 0x10

//#define mainLED_PIN 

// Defined in Cmake
#define mainWIFI_SSID ""
#define mainWIFI_PASSWORD ""
#define mainMQTT_SERVER "192.168.60.120"
#define mainMQTT_PORT 1883

#define MQTT_TOPIC_LEN 100

// keep alive in seconds
#define MQTT_KEEP_ALIVE_S 60

// qos passed to mqtt_subscribe
// At most once (QoS 0)
// At least once (QoS 1)
// Exactly once (QoS 2)
#define MQTT_SUBSCRIBE_QOS 1
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 0

// topic used for last will and testament
#define MQTT_WILL_TOPIC "/online"
#define MQTT_WILL_MSG "0"
#define MQTT_WILL_QOS 1

#define MQTT_DEVICE_NAME "pico"

// Set to 1 to add the client name to topics, to support multiple devices using the same server
#define MQTT_UNIQUE_TOPIC 1

typedef struct {
    mqtt_client_t* mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    int subscribe_count;
    bool stop_client;
} MQTT_CLIENT_DATA_T;

/* Priorities at which the tasks are created. */
#define mainLIGHT_SENSOR_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
#define	mainGAS_SENSOR_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
#define	mainQUEUE_SEND_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )

#define mainSENSOR_SAMPLE_FREQUENCY_MS	    ( 2000 / portTICK_PERIOD_MS )
#define mainQUEUE_SEND_FREQUENCY_MS	        ( 3000 / portTICK_PERIOD_MS )
#define mainSEND_DELAY_MS	                ( 300 / portTICK_PERIOD_MS )

/* The number of items the queue can hold. */
#define mainQUEUE_LENGTH					( 10 )
/* When is the queue send. */
#define mainQUEUE_THRESHOLD                 ( 3 )

#endif /* MAIN_H */