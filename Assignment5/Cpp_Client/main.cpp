#include <stdio.h>
#include <string.h>
#include <iostream>
#include <cmath>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "uart/PicoUart.h"
#include "json.hpp"

#include "IPStack.h"
#include "Countdown.h"
#include "MQTTClient.h"
#include "ModbusClient.h"
#include "ModbusRegister.h"
#include "ssd1306.h"
#include "LED.h"
#include "temp_drawing.h"

using json = nlohmann::json;

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#if 0
#define UART_NR 0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#else
#define UART_NR 1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#endif

#define BAUD_RATE 9600
#define STOP_BITS 1 // for simulator
//#define STOP_BITS 2 // for real system

//#define USE_MODBUS
#define USE_MQTT
#define USE_SSD1306

// LEDs
#define LED_D3 20
#define LED_D2 21
#define LED_D1 22

// Button
#define ROT_SW 12
#define BUTTON_PERIOD 10
#define BUTTON_FILTER 5
#define RELEASED 1

// Onboard temp sensor
#define PIN_TEMP_SENSOR 4

// Message buffer
#define MAX_BUFFER_SIZE 128

// Network settings
#if 0
#define SSID ""			// Fill in
#define PASSWORD ""		// Fill in
#define IP_ADDRESS ""	// Fill in
#define PORT 21883		// Change
#else
#define SSID ""			// Fill in
#define PASSWORD ""		// Fill in
#define IP_ADDRESS ""	// Fill in
#define PORT 1883		// Change
#endif

// Initialize LED objects
static LED D3(LED_D3);
static LED D2(LED_D2);
static LED D1(LED_D1);

// Global variables
static const char *topic = "gemma/temp";        // TO CHANGE: subscribed topic
static volatile bool publishTempMsg = false;    // Flag to publish message
static volatile int min_temp = 20;              // Below: D1 ON
static volatile int max_temp = 30;              // Above: D3 ON

// Function declarations
bool repeatingTimerCallback(struct repeating_timer *t);
void messageArrived(MQTT::MessageData &md);

int main() {
    // Initialize button
    gpio_init(ROT_SW);
    gpio_set_dir(ROT_SW, GPIO_IN);
    gpio_pull_up(ROT_SW);

    // Initialize ADC
    adc_init();
    adc_gpio_init(PIN_TEMP_SENSOR);
    adc_select_input(PIN_TEMP_SENSOR);
    adc_set_temp_sensor_enabled(true);

    // Initialize chosen serial port
    stdio_init_all();

    struct repeating_timer timer;
    add_repeating_timer_ms(BUTTON_PERIOD, repeatingTimerCallback, NULL, &timer);

    printf("\nBoot\n");

#ifdef USE_SSD1306
    // I2C is "open drain",
    // pull ups to keep signal high when no data is being sent
    i2c_init(i2c1, 400 * 1000);
    gpio_set_function(14, GPIO_FUNC_I2C); // the display has external pull-ups
    gpio_set_function(15, GPIO_FUNC_I2C); // the display has external pull-ups
    ssd1306 display(i2c1);

    display.fill(0);
    display.text("topic:gemma/temp", 0, 0);
    mono_vlsb rb(temperature_drawing, 72, 39);
    display.blit(rb, 30, 14);
    display.show();
#if 0
    for(int i = 0; i < 128; ++i) {
        sleep_ms(50);
        display.scroll(1, 0);
        display.show();
    }
    display.text("Done", 20, 20);
    display.show();
#endif

#endif

#ifdef USE_MQTT
    IPStack ipstack(SSID, PASSWORD);
    auto client = MQTT::Client<IPStack, Countdown>(ipstack);
    int rc = ipstack.connect(IP_ADDRESS, PORT);
    if (rc != 1) {
        printf("rc from TCP connect is %d\n", rc);
    }

    printf("MQTT connecting\n");
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = (char *) "gemma";
    data.username.cstring = (char *) "SmartIoT";
    data.password.cstring = (char *) "SmartIoTMQTT";
    rc = client.connect(data);
    if (rc != 0) {
        printf("rc from MQTT connect is %d\n", rc);
        while (true) {
            tight_loop_contents();
        }
    }
    printf("MQTT connected\n");

    // We subscribe QoS2. Messages sent with lower QoS will be delivered using the QoS they were sent with
    rc = client.subscribe(topic, MQTT::QOS2, messageArrived);
    if (rc != 0) {
        printf("rc from MQTT subscribe is %d\n", rc);
    }
    printf("MQTT subscribed\n");

    auto mqtt_send = make_timeout_time_ms(2000);
#endif

#ifdef USE_MODBUS
    auto uart{std::make_shared<PicoUart>(UART_NR, UART_TX_PIN, UART_RX_PIN, BAUD_RATE, STOP_BITS)};
    auto rtu_client{std::make_shared<ModbusClient>(uart)};
    ModbusRegister rh(rtu_client, 241, 256);
    auto modbus_poll = make_timeout_time_ms(3000);
    ModbusRegister produal(rtu_client, 1, 0);
    produal.write(100);
    sleep_ms((100));
    produal.write(100);
#endif

    while (true) {
#ifdef USE_MODBUS
        if (time_reached(modbus_poll)) {
            gpio_put(led_pin, !gpio_get(led_pin)); // toggle  led
            modbus_poll = delayed_by_ms(modbus_poll, 3000);
            printf("RH=%5.1f%%\n", rh.read() / 10.0);
        }
#endif
#ifdef USE_MQTT
        if (time_reached(mqtt_send)) {
            mqtt_send = delayed_by_ms(mqtt_send, 2000);
            if (!client.isConnected()) {
                printf("Not connected...\n");
                rc = client.connect(data);
                if (rc != 0) {
                    printf("rc from MQTT connect is %d\n", rc);
                }
            }
        }

        cyw43_arch_poll(); // obsolete? - see below
        client.yield(100); // socket that client uses calls cyw43_arch_poll()
#endif
        ////////////////////////////////////
        //          SAMPLING ADC          //
        ////////////////////////////////////
        D2.toggle();
        /* Read from ADC */
        uint16_t read_value = adc_read();
        /* Convert read value to ADC voltage */
        double ADC_voltage = read_value * 3.3 / (4096 - 1);
        /*  Convert ADC voltage to temperature */
        double temperature = 27 - (ADC_voltage - 0.706) / 0.001721;
        printf("Temp: %.2f C, Read: %d\n", temperature, read_value);

        /* D1, D3: ON or OFF based on threshold values */
        if (temperature < min_temp) {
            D1.on();
        } else if (temperature >= min_temp) {
            D1.off();
        } else if (temperature > max_temp) {
            D3.on();
        } else if (temperature <= max_temp) {
            D3.off();
        }

        sleep_ms(1000);
#if 1
        if (publishTempMsg) {
            // Prepare JSON message
            char button_msg[MAX_BUFFER_SIZE];
            sprintf(button_msg, "{\"topic\":\"%s\",\"msg\":\"Temp.: %.2f C\"}", topic, temperature);
            // Publish the message
            MQTT::Message button_message;
            button_message.qos = MQTT::QOS0;
            button_message.retained = false;
            button_message.dup = false;
            button_message.payload = (void *)button_msg;
            button_message.payloadlen = strlen(button_msg) + 1;
            client.publish(topic, button_message);
            publishTempMsg = false;
        }
#endif
    }
}

bool repeatingTimerCallback(struct repeating_timer *t) {
    // For ROT_SW:
    static uint button_state = 0, filter_counter = 0;
    uint new_state = 1;

    new_state = gpio_get(ROT_SW);
    if (button_state != new_state) {
        if (++filter_counter >= BUTTON_FILTER) {
            button_state = new_state;
            filter_counter = 0;
            if (new_state != RELEASED) {
                publishTempMsg = true;
            }
        }
    } else {
        filter_counter = 0;
    }
    return true;
}

void messageArrived(MQTT::MessageData &md) {
    MQTT::Message &message = md.message;
    char *payload = (char *)message.payload;

    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\n",
           message.qos, message.retained, message.dup, message.id);
    printf("Payload %s\n", payload);

    // Parse the JSON string
    json j = json::parse(payload);
    // Extract the 'msg' part
    std::string received_message = j["msg"];

    // Check the received message and take appropriate actions
    if (received_message == "temp") {
        publishTempMsg = true;  // Set flag to send out message
    } else if (received_message.compare(0, 5, "High:") == 0) {
        max_temp = std::stoi(received_message.substr(5));
        std::cout << "New max_temp: " << max_temp << std::endl;
    } else if (received_message.compare(0, 4, "Low:") == 0) {
        min_temp = std::stoi(received_message.substr(4));
        std::cout << "New min_temp: " << min_temp << std::endl;
    }

    // Clear the payload buffer
    memset(payload, 0, message.payloadlen);
}