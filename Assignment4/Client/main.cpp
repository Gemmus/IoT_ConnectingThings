#include <stdio.h>
#include <string.h>
#include <iostream>
#include <cmath>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/timer.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "uart/PicoUart.h"
#include "json.hpp"

#include "IPStack.h"
#include "Countdown.h"
#include "MQTTClient.h"
#include "ModbusClient.h"
#include "ModbusRegister.h"
#include "ssd1306.h"
#include "LED.h"

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
#define SW_0 9
#define BUTTON_PERIOD 10
#define BUTTON_FILTER 5
#define RELEASED 1

#ifdef USE_SSD1306
static const unsigned char lightbulb[] = {
        // font edit begin : monovlsb : 39 : 39 : 39
        0x00, 0x10, 0x60, 0xC0, 0x80, 0x01, 0x06, 0x0C,
        0x18, 0x20, 0x80, 0xC1, 0x62, 0x34, 0x90, 0xC8,
        0xC8, 0x68, 0x28, 0x08, 0x08, 0x08, 0x08, 0x08,
        0x10, 0x30, 0x60, 0xC4, 0x83, 0x21, 0x1C, 0x0E,
        0x83, 0xE0, 0x70, 0x18, 0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x00, 0x00,
        0xFF, 0x00, 0x18, 0x0E, 0x07, 0x03, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x02, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
        0x0C, 0x10, 0x20, 0x40, 0x80, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40,
        0x20, 0x10, 0x0C, 0x07, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x3F, 0x20, 0x40, 0x40,
        0x40, 0x40, 0x40, 0x40, 0x20, 0x3F, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x0A, 0x1F, 0x1F, 0x1F, 0x1F,
        0x1F, 0x1F, 0x1F, 0x1F, 0x0A, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00
        // font edit end
};
#endif

// Initialize LED objects
LED D3(LED_D3);
LED D2(LED_D2);
LED D1(LED_D1);

static const char *topic = "gemma/LED";
static const char *msg = "SW_0 pressed";

volatile bool buttonEvent = false;

bool repeatingTimerCallback(struct repeating_timer *t);
void messageArrived(MQTT::MessageData &md);

int main() {
    // Initialize button
    gpio_init(SW_0);
    gpio_set_dir(SW_0, GPIO_IN);
    gpio_pull_up(SW_0);

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
    display.text("topic: gemma/LED", 0, 0);
    mono_vlsb rb(lightbulb, 39, 39);
    display.blit(rb, 44, 24);
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
    //IPStack ipstack("SSID", "PASSWORD"); // example
    //IPStack ipstack("SmartIotMQTT", "SmartIot");
    
    auto client = MQTT::Client<IPStack, Countdown>(ipstack);

    //int rc = ipstack.connect("18.198.188.151", 21883);
    int rc = ipstack.connect("192.168.1.153", 1883);
    if (rc != 1) {
        printf("rc from TCP connect is %d\n", rc);
    }

    printf("MQTT connecting\n");
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    //data.clientID.cstring = (char *) "PicoW-sample";
    data.clientID.cstring = (char *) "gemma";
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
    //int mqtt_qos = 0;
    //int msg_count = 0;
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
#if 0
            char buf[100];
            int rc = 0;
            MQTT::Message message;
            message.retained = false;
            message.dup = false;
            message.payload = (void *) buf;
            switch (mqtt_qos) {
                case 0:
                    // Send and receive QoS 0 message
                    //sprintf(buf, "Msg nr: %d QoS 0 message", ++msg_count); // Change here the message to publish
                    sprintf(buf, "{\"topic\":\"%s\",\"msg\":\"%s\"}", topic, msg);
                    printf("%s\n", buf);
                    message.qos = MQTT::QOS0;
                    message.payloadlen = strlen(buf) + 1;
                    rc = client.publish(topic, message);
                    printf("Publish rc=%d\n", rc);
                    //++mqtt_qos;
                    break;
                default:
                    mqtt_qos = 0;
                    break;
            }
#endif
        }

        cyw43_arch_poll(); // obsolete? - see below
        client.yield(100); // socket that client uses calls cyw43_arch_poll()
#endif
        if (buttonEvent) {
            D2.toggle();
            // Prepare JSON message
            char button_msg[100];
            sprintf(button_msg, "{\"topic\":\"%s\",\"msg\":\"%s\"}", topic, msg);
            // Publish the message
            MQTT::Message button_message;
            button_message.qos = MQTT::QOS0;
            button_message.retained = false;
            button_message.dup = false;
            button_message.payload = (void *)button_msg;
            button_message.payloadlen = strlen(button_msg) + 1;
            client.publish(topic, button_message);
            buttonEvent = false;
        }
    }
}

bool repeatingTimerCallback(struct repeating_timer *t) {
    // For SW_0:
    static uint button_state = 0, filter_counter = 0;
    uint new_state = 1;

    new_state = gpio_get(SW_0);
    if (button_state != new_state) {
        if (++filter_counter >= BUTTON_FILTER) {
            button_state = new_state;
            filter_counter = 0;
            if (new_state != RELEASED) {
                buttonEvent = true;
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
    std::cout << "Extracted message: " << received_message << std::endl;

    // Compare 'msg' part
    if (received_message == "D1:ON") {
        D1.on();
    } else if (received_message == "D1:OFF") {
        D1.off();
    } else if (received_message == "D1:TOGG") {
        D1.toggle();
    } else if (received_message == "D2:ON") {
        D2.on();
    } else if (received_message == "D2:OFF") {
        D2.off();
    } else if (received_message == "D2:TOGG") {
        D2.toggle();
    } else if (received_message == "D3:ON") {
        D3.on();
    } else if (received_message == "D3:OFF"){
        D3.off();
    } else if (received_message == "D3:TOGG") {
        D3.toggle();
    }

    // Clear the payload buffer
    memset(payload, 0, message.payloadlen);
}
