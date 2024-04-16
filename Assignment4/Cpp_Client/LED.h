//
// Created by Gemma on 13/04/2024.
//

#ifndef PICO_MODBUS_LED_H
#define PICO_MODBUS_LED_H

#include <cstdint>

class LED {
public:
    explicit LED(const uint8_t led_pin0) {
        led_pin = led_pin0;
        gpio_init(led_pin);
        gpio_set_dir(led_pin, GPIO_OUT);
        led_on = false;
    }

    LED(const LED &led0) {
        led_pin = led0.led_pin;
    }

    ~LED() = default;

    void on() {
        gpio_put(led_pin, true);
        led_on = true;
    }

    void off() {
        gpio_put(led_pin, false);
        led_on = false;
    }

    void toggle() {
        if (led_on) {
            off();
        } else {
            on();
        }
    }

private:
    uint8_t led_pin;
    bool led_on;
};

#endif //PICO_MODBUS_LED_H
