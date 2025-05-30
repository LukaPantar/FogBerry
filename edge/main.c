/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Library includes. */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#if ( mainRUN_ON_CORE == 1 )
    #include "pico/multicore.h"
#endif
// Pico W devices use a GPIO on the WIFI chip for the LED,
// so when building for Pico W, CYW43_WL_GPIO_LED_PIN will be defined
#include "pico/cyw43_arch.h"
#include "hardware/spi.h"
#include "mfrc522.h"
#include "lwipopts.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h" // needed to set hostname
#include "pico/unique_id.h"
#include "hardware/irq.h"

#include "main.h"

/*-----------------------------------------------------------*/

/*
 * Configure the hardware as necessary to run this demo.
 */
static void prvSetupHardware( void );

static void prvLightSensorTask( void *pvParameters );
static void prvGasSensorTask( void *pvParameters );
static void prvQueueSendTask( void *pvParameters );

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );

/*-----------------------------------------------------------*/

static QueueHandle_t lightQueue = NULL;
static QueueHandle_t gasQueue = NULL;

/*-----------------------------------------------------------*/

MFRC522Ptr_t mfrc;
static MQTT_CLIENT_DATA_T state;
char client_id_buf[sizeof(MQTT_DEVICE_NAME) + 5];

/*-----------------------------------------------------------*/

uint16_t read_analog_pin(uint8_t adc_pin)
{
    adc_select_input(adc_pin);
    uint16_t raw_value = adc_read();
    return raw_value;
}

bool authenticate_card()
{
    // Declare card UID's
    uint8_t card_tag[] = {mainMFRC522_CARD_TAG_0,
                          mainMFRC522_CARD_TAG_1,
                          mainMFRC522_CARD_TAG_2,
                          mainMFRC522_CARD_TAG_3};

    //Authorization with uid
    if(memcmp(mfrc->uid.uidByte, card_tag, 4) == 0) {
        printf("Authentication Success\n\r");
        return true;
    } else {
        printf("Authentication Failed\n\r");
        return false;
    }
}

void read_new_card( void )
{
    //Wait for new card
    printf("Waiting for card\n\r");
    while(!PICC_IsNewCardPresent(mfrc));

    //Select the card
    printf("Selecting card\n\r");
    PICC_ReadCardSerial(mfrc);

    //Show UID on serial monitor
    printf("PICC dump: \n\r");
    PICC_DumpToSerial(mfrc, &(mfrc->uid));

    printf("Uid is: ");
    for(int i = 0; i < 4; i++) {
        printf("%x ", mfrc->uid.uidByte[i]);
    } printf("\n\r");
}

static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name) {
    static char full_topic[MQTT_TOPIC_LEN];
    snprintf(full_topic, sizeof(full_topic), "/%s%s", state->mqtt_client_info.client_id, name);
    return full_topic;
    return name;
}

static void pub_request_cb(__unused void *arg, err_t err) {
    if (err != 0) {
        printf("pub_request_cb failed %d", err);
        exit(1);
    }
}

static void publish_value(MQTT_CLIENT_DATA_T *state, char topic[], uint32_t value) {
    const char *topic_key = full_topic(state, topic);
    char temp_str[32];
    snprintf(temp_str, sizeof(temp_str), "%ld", value);
    printf("Publishing %s to %s\n", temp_str, topic_key);
    mqtt_publish(state->mqtt_client_inst, topic_key, temp_str, strlen(temp_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        state->connect_done = true;

        // indicate online
        if (state->mqtt_client_info.will_topic) {
            mqtt_publish(state->mqtt_client_inst, state->mqtt_client_info.will_topic, "1", 1, MQTT_WILL_QOS, true, pub_request_cb, state);
        }

    } else if (status == MQTT_CONNECT_DISCONNECTED) {
        if (!state->connect_done) {
            panic("Failed to connect to mqtt server");
        }
    }
    else {
        panic("Unexpected status");
    }
}

static void start_client(MQTT_CLIENT_DATA_T *state) {
    printf("Warning: Not using TLS\n");

    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst) {
        panic("MQTT client instance creation error");
    }
    printf("Connecting to mqtt server at %s\n", ipaddr_ntoa(&state->mqtt_server_address));

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, MQTT_PORT, mqtt_connection_cb, state, &state->mqtt_client_info) != ERR_OK) {
        panic("MQTT broker connection error");
    }
    cyw43_arch_lwip_end();
}

void init_mqtt()
{
    printf("MQTT client starting\n");

    // Use board unique id
    char unique_id_buf[5];
    pico_get_unique_board_id_string(unique_id_buf, sizeof(unique_id_buf));
    for(int i=0; i < sizeof(unique_id_buf) - 1; i++) {
        unique_id_buf[i] = tolower(unique_id_buf[i]);
    }

    // Generate a unique name, e.g. pico1234
    memcpy(&client_id_buf[0], MQTT_DEVICE_NAME, sizeof(MQTT_DEVICE_NAME) - 1);
    memcpy(&client_id_buf[sizeof(MQTT_DEVICE_NAME) - 1], unique_id_buf, sizeof(unique_id_buf) - 1);
    client_id_buf[sizeof(client_id_buf) - 1] = 0;
    printf("Device name %s\n", client_id_buf);

    state.mqtt_client_info.client_id = client_id_buf;
    state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S; // Keep alive in sec
    state.mqtt_client_info.client_user = NULL;
    state.mqtt_client_info.client_pass = NULL;

    static char will_topic[MQTT_TOPIC_LEN];
    strncpy(will_topic, full_topic(&state, MQTT_WILL_TOPIC), sizeof(will_topic));
    state.mqtt_client_info.will_topic = will_topic;
    state.mqtt_client_info.will_msg = MQTT_WILL_MSG;
    state.mqtt_client_info.will_qos = MQTT_WILL_QOS;
    state.mqtt_client_info.will_retain = true;

    ip_addr_t addr;
    if (!ip4addr_aton(mainMQTT_SERVER, &addr)) {
        printf("ip error\n");
        return;
    }
    state.mqtt_server_address = addr;

    if (cyw43_arch_wifi_connect_timeout_ms(mainWIFI_SSID, mainWIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000))
    {
        printf("Failed to connect\n");
        return;
    }
    else
    {
        printf("Connected.\n");
        // Read the ip address in a human readable way
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("IP address %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
    }

    start_client(&state);
}

void vLaunch( void )
{
    /* Unlock the device after card authentication. */
    read_new_card();
    if (!authenticate_card())
    {
        return;
    }

    init_mqtt();

    /* Create the queue. */
	lightQueue = xQueueCreate( mainQUEUE_LENGTH, sizeof( uint32_t ) );
	gasQueue = xQueueCreate( mainQUEUE_LENGTH, sizeof( uint32_t ) );

    printf("Creating tasks.\n");
    
    xTaskCreate(prvLightSensorTask,		     		/* The function that implements the task. */
                "LightSensor",   					/* The text name assigned to the task - for debug only as it is not used by the kernel. */
                configMINIMAL_STACK_SIZE, 			/* The size of the stack to allocate to the task. */
                NULL, 								/* The parameter passed to the task - not used in this case. */
                mainLIGHT_SENSOR_TASK_PRIORITY, 	/* The priority assigned to the task. */
                NULL);								/* The task handle is not required, so NULL is passed. */

    xTaskCreate(prvGasSensorTask,
                "GasSensor",
                configMINIMAL_STACK_SIZE,
                NULL,
                mainGAS_SENSOR_TASK_PRIORITY,
                NULL);

    xTaskCreate(prvQueueSendTask,
                "QueueSend",
                configMINIMAL_STACK_SIZE,
                NULL,
                mainQUEUE_SEND_TASK_PRIORITY,
                NULL);

    /* Start the tasks and timer running. */
    vTaskStartScheduler();


    // We should not get here after the scheduler takes over 
    // (or there is not enough heap memory for tasks to be created)
    for( ;; );
}

int main( void )
{
    /* Configure the hardware ready to run the demo. */
    prvSetupHardware();
    const char *rtos_name;
    rtos_name = "FreeRTOS SMP";

#if ( portSUPPORT_SMP == 1 ) && ( configNUMBER_OF_CORES == 2 )
    printf("%s on both cores:\n", rtos_name);
    vLaunch();
#endif

    printf("%s on core 0:\n", rtos_name);
    vLaunch();

    return 0;
}
/*-----------------------------------------------------------*/

// TASKS
static void prvLightSensorTask( void *pvParameters )
{
    TickType_t xNextWakeTime;

	/* Remove compiler warning about unused parameter. */
	( void ) pvParameters;

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();

	for( ;; )
	{
		/* Place this task in the blocked state until it is time to run again. */
		vTaskDelayUntil( &xNextWakeTime, mainSENSOR_SAMPLE_FREQUENCY_MS );

        uint32_t light_sensor_value = read_analog_pin(mainADA161_LIGHT_SENSOR_ADC_PIN);
        printf("Light Sensor Value: %ld\n", light_sensor_value);
		// /* Send to the queue */
		xQueueSendToBack( lightQueue, &light_sensor_value, 0U );
	}
}

static void prvGasSensorTask( void *pvParameters )
{
    TickType_t xNextWakeTime;

	/* Remove compiler warning about unused parameter. */
	( void ) pvParameters;

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();

	for( ;; )
	{
		/* Place this task in the blocked state until it is time to run again. */
		vTaskDelayUntil( &xNextWakeTime, mainSENSOR_SAMPLE_FREQUENCY_MS );

        uint32_t gas_sensor_value = read_analog_pin(mainMQ7_GAS_SENSOR_ADC_PIN);
        printf("Gas Sensor Value: %ld\n", gas_sensor_value);
		/* Send to the queue */
		xQueueSendToBack( gasQueue, &gas_sensor_value, 0U );
	}
}

static void publishQueue(QueueHandle_t queue, char topic[])
{
    int num_elements = uxQueueMessagesWaiting(queue);
    for (int idx = 0; idx < num_elements; idx++)
    {
        uint32_t value;
        xQueueReceive(queue, &value, portMAX_DELAY);
        vTaskDelay( mainSEND_DELAY_MS );  // Small delay to deliver everything
        publish_value(&state, topic, value);
    }
}

static void prvQueueSendTask( void *pvParameters )
{
	( void ) pvParameters;
    TickType_t xNextWakeTime;
	xNextWakeTime = xTaskGetTickCount();

	for( ;; )
    {
        if (uxQueueMessagesWaiting(lightQueue) >= mainQUEUE_THRESHOLD)
        {
            publishQueue(lightQueue, "/ADA161/light");
            xQueueReset(lightQueue);
        }
        else if (uxQueueMessagesWaiting(gasQueue) >= mainQUEUE_THRESHOLD)
        {
            publishQueue(gasQueue, "/MQ-7/gas");
            xQueueReset(gasQueue);
        }

        /* Wait for the messages */
		vTaskDelayUntil( &xNextWakeTime, mainQUEUE_SEND_FREQUENCY_MS );
    }
}

/*-----------------------------------------------------------*/

static void prvSetupHardware( void )
{
    stdio_init_all();
    adc_init();

    // if (cyw43_arch_init_with_country(CYW43_COUNTRY_SLOVENIA))
    if (cyw43_arch_init())
    {
        printf("Failed to initialise wifi chip\n");
        return;
    }
    
    // On board LED is ON
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);

    // ADA161 Light sensor on ADC0
    adc_gpio_init(mainADA161_LIGHT_SENSOR_PIN); // Enable ADC on GPIO26

    // MQ-7 Gas sensor on ADC1
    adc_gpio_init(mainADA161_LIGHT_SENSOR_PIN); // Enable ADC on GPIO27

    mfrc = MFRC522_Init();
    PCD_Init(mfrc, spi0);

    sleep_ms(mainHW_INIT_DELAY_MS);

    // Enable wifi station
    cyw43_arch_enable_sta_mode();

    printf("Initialised HW.\n");
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
    free memory available in the FreeRTOS heap.  pvPortMalloc() is called
    internally by FreeRTOS API functions that create tasks, queues, software
    timers, and semaphores.  The size of the FreeRTOS heap is set by the
    configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */

    /* Force an assert. */
    configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
    volatile size_t xFreeHeapSpace;

    /* This is just a trivial example of an idle hook.  It is called on each
    cycle of the idle task.  It must *NOT* attempt to block.  In this case the
    idle task just queries the amount of FreeRTOS heap that remains.  See the
    memory management section on the http://www.FreeRTOS.org web site for memory
    management options.  If there is a lot of heap memory free then the
    configTOTAL_HEAP_SIZE value in FreeRTOSConfig.h can be reduced to free up
    RAM. */
    xFreeHeapSpace = xPortGetFreeHeapSize();

    /* Remove compiler warning about xFreeHeapSpace being set but never used. */
    ( void ) xFreeHeapSpace;
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{

}
