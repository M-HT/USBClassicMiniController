/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 * Copyright (C) 2026 Roman Pauer
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/hid.h>
#include <libopencm3/stm32/i2c.h>
#include <libopencm3/stm32/timer.h>

// define Sense1 and Sense2 pins
#define GPIO_SENSE1 GPIO4
#define GPIO_SENSE2 GPIO3

static const uint8_t setup_data[6] = {0xF0, 0x55, 0xFB, 0x00, 0xFE, 0x01};
static int8_t controller_state[2] = {0, 0};
static uint8_t dual_report = 0;
static uint8_t last_report = 0;
static uint8_t data_length[2];

static usbd_device *usbd_dev;

const struct usb_device_descriptor dev_descr = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = 0x054c,
    .idProduct = 0x0cda,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 0,
    .bNumConfigurations = 1,
};

static const uint8_t hid_report_descriptor12[] = {
    0x05, 0x01, // Usage Page (Desktop),       ; Generic desktop controls (01h)
    0x09, 0x05, // Usage (Gamepad),            ; Gamepad (05h, application collection)
    0xA1, 0x01, // Collection (Application),
    0x15, 0x00, //     Logical Minimum (0),
    0x25, 0x01, //     Logical Maximum (1),
    0x75, 0x01, //     Report Size (1),
    0x95, 0x0A, //     Report Count (10),
    0x05, 0x09, //     Usage Page (Button),    ; Button (09h)
    0x19, 0x01, //     Usage Minimum (01h),
    0x29, 0x0A, //     Usage Maximum (0Ah),
    0x81, 0x02, //     Input (Variable),
    0x05, 0x01, //     Usage Page (Desktop),   ; Generic desktop controls (01h)
    0x09, 0x30, //     Usage (X),              ; X (30h, dynamic value)
    0x09, 0x31, //     Usage (Y),              ; Y (31h, dynamic value)
    0x15, 0x00, //     Logical Minimum (0),
    0x25, 0x02, //     Logical Maximum (2),
    0x35, 0x00, //     Physical Minimum (0),
    0x45, 0x02, //     Physical Maximum (2),
    0x75, 0x02, //     Report Size (2),
    0x95, 0x02, //     Report Count (2),
    0x81, 0x02, //     Input (Variable),
    0x75, 0x01, //     Report Size (1),
    0x95, 0x02, //     Report Count (2),
    0x81, 0x01, //     Input (Constant),
    0xC0,       // End Collection
};

static const uint8_t hid_report_descriptor3[] = {
    0x05, 0x01, // Usage Page (Desktop),       ; Generic desktop controls (01h)
    0x09, 0x05, // Usage (Gamepad),            ; Gamepad (05h, application collection)
    0xA1, 0x01, // Collection (Application),
    0x85, 0x01, //     Report ID (1),
    0x15, 0x00, //     Logical Minimum (0),
    0x25, 0x01, //     Logical Maximum (1),
    0x75, 0x01, //     Report Size (1),
    0x95, 0x0A, //     Report Count (10),
    0x05, 0x09, //     Usage Page (Button),    ; Button (09h)
    0x19, 0x01, //     Usage Minimum (01h),
    0x29, 0x0A, //     Usage Maximum (0Ah),
    0x81, 0x02, //     Input (Variable),
    0x05, 0x01, //     Usage Page (Desktop),   ; Generic desktop controls (01h)
    0x09, 0x30, //     Usage (X),              ; X (30h, dynamic value)
    0x09, 0x31, //     Usage (Y),              ; Y (31h, dynamic value)
    0x15, 0x00, //     Logical Minimum (0),
    0x25, 0x02, //     Logical Maximum (2),
    0x35, 0x00, //     Physical Minimum (0),
    0x45, 0x02, //     Physical Maximum (2),
    0x75, 0x02, //     Report Size (2),
    0x95, 0x02, //     Report Count (2),
    0x81, 0x02, //     Input (Variable),
    0x75, 0x01, //     Report Size (1),
    0x95, 0x02, //     Report Count (2),
    0x81, 0x01, //     Input (Constant),
    0xC0,       // End Collection

// the second half has the same size and content as the first half, with the exception of report id
    0x05, 0x01, // Usage Page (Desktop),       ; Generic desktop controls (01h)
    0x09, 0x05, // Usage (Gamepad),            ; Gamepad (05h, application collection)
    0xA1, 0x01, // Collection (Application),
    0x85, 0x02, //     Report ID (2),
    0x15, 0x00, //     Logical Minimum (0),
    0x25, 0x01, //     Logical Maximum (1),
    0x75, 0x01, //     Report Size (1),
    0x95, 0x0A, //     Report Count (10),
    0x05, 0x09, //     Usage Page (Button),    ; Button (09h)
    0x19, 0x01, //     Usage Minimum (01h),
    0x29, 0x0A, //     Usage Maximum (0Ah),
    0x81, 0x02, //     Input (Variable),
    0x05, 0x01, //     Usage Page (Desktop),   ; Generic desktop controls (01h)
    0x09, 0x30, //     Usage (X),              ; X (30h, dynamic value)
    0x09, 0x31, //     Usage (Y),              ; Y (31h, dynamic value)
    0x15, 0x00, //     Logical Minimum (0),
    0x25, 0x02, //     Logical Maximum (2),
    0x35, 0x00, //     Physical Minimum (0),
    0x45, 0x02, //     Physical Maximum (2),
    0x75, 0x02, //     Report Size (2),
    0x95, 0x02, //     Report Count (2),
    0x81, 0x02, //     Input (Variable),
    0x75, 0x01, //     Report Size (1),
    0x95, 0x02, //     Report Count (2),
    0x81, 0x01, //     Input (Constant),
    0xC0,       // End Collection
};

static const struct __attribute__((packed)) {
    struct usb_hid_descriptor hid_descriptor;
    struct __attribute__((packed)) {
        uint8_t bReportDescriptorType;
        uint16_t wDescriptorLength;
    } hid_report;
} hid_function12 = {
    .hid_descriptor = {
        .bLength = sizeof(hid_function12),
        .bDescriptorType = USB_HID_DT_HID,
        .bcdHID = 0x0111,
        .bCountryCode = 0,
        .bNumDescriptors = 1,
    },
    .hid_report = {
        .bReportDescriptorType = USB_HID_DT_REPORT,
        .wDescriptorLength = sizeof(hid_report_descriptor12),
    }
}, hid_function3 = {
    .hid_descriptor = {
        .bLength = sizeof(hid_function3),
        .bDescriptorType = USB_HID_DT_HID,
        .bcdHID = 0x0111,
        .bCountryCode = 0,
        .bNumDescriptors = 1,
    },
    .hid_report = {
        .bReportDescriptorType = USB_HID_DT_REPORT,
        .wDescriptorLength = sizeof(hid_report_descriptor3),
    }
};

const struct usb_endpoint_descriptor hid_endpoint1 = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_ADDR_IN(6),
    .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
    .wMaxPacketSize = 2,
    .bInterval = 10,
};

const struct usb_endpoint_descriptor hid_endpoint2 = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_ADDR_IN(7),
    .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
    .wMaxPacketSize = 2,
    .bInterval = 10,
};

const struct usb_endpoint_descriptor hid_endpoint3 = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_ADDR_IN(6),
    .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
    .wMaxPacketSize = 3,
    .bInterval = 10,
};

const struct usb_interface_descriptor hid_iface1 = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = USB_HID_SUBCLASS_NO,
    .bInterfaceProtocol = USB_HID_INTERFACE_PROTOCOL_NONE,
    .iInterface = 3,

    .endpoint = &hid_endpoint1,

    .extra = &hid_function12,
    .extralen = sizeof(hid_function12),
};

const struct usb_interface_descriptor hid_iface2 = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 1,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = USB_HID_SUBCLASS_NO,
    .bInterfaceProtocol = USB_HID_INTERFACE_PROTOCOL_NONE,
    .iInterface = 4,

    .endpoint = &hid_endpoint2,

    .extra = &hid_function12,
    .extralen = sizeof(hid_function12),
};

const struct usb_interface_descriptor hid_iface3 = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = USB_HID_SUBCLASS_NO,
    .bInterfaceProtocol = USB_HID_INTERFACE_PROTOCOL_NONE,
    .iInterface = 3,

    .endpoint = &hid_endpoint3,

    .extra = &hid_function3,
    .extralen = sizeof(hid_function3),
};

const struct usb_interface ifaces12[] = {
    {
        .num_altsetting = 1,
        .altsetting = &hid_iface1,
    },
    {
        .num_altsetting = 1,
        .altsetting = &hid_iface2,
    }
};

const struct usb_interface ifaces3[] = {
    {
        .num_altsetting = 1,
        .altsetting = &hid_iface3,
    }
};

const struct usb_config_descriptor config1 = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 1,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0xA0,
    .bMaxPower = 0x32,

    .interface = ifaces12,
};

const struct usb_config_descriptor config2 = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 2,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0xA0,
    .bMaxPower = 0x32,

    .interface = ifaces12,
};

const struct usb_config_descriptor config3 = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 1,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0xA0,
    .bMaxPower = 0x32,

    .interface = ifaces3,
};

static const char *usb_strings[] = {
    "Sony Interactive Entertainment",
    "Controller",
    "HID",
    "HID2",
};

// Buffer to be used for control requests.
static uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes hid_control_request(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
            void (**complete)(usbd_device *, struct usb_setup_data *))
{
    (void)complete;
    (void)dev;

    if((req->bmRequestType != 0x81) ||
       (req->bRequest != USB_REQ_GET_DESCRIPTOR) ||
       (req->wValue != (USB_HID_DT_REPORT << 8)))
        return USBD_REQ_NOTSUPP;

    // Handle the HID report descriptor.
    if (dual_report == 2)
    {
        *buf = (uint8_t *)hid_report_descriptor3;
        *len = sizeof(hid_report_descriptor3);
    }
    else
    {
        *buf = (uint8_t *)hid_report_descriptor12;
        *len = sizeof(hid_report_descriptor12);
    }

    return USBD_REQ_HANDLED;
}

static void hid_set_config(usbd_device *dev, uint16_t wValue)
{
    (void)wValue;
    (void)dev;

    if (dual_report == 2)
    {
        usbd_ep_setup(dev, USB_ENDPOINT_ADDR_IN(6), USB_ENDPOINT_ATTR_INTERRUPT, hid_endpoint3.wMaxPacketSize, NULL);
    }
    else
    {
        usbd_ep_setup(dev, USB_ENDPOINT_ADDR_IN(6), USB_ENDPOINT_ATTR_INTERRUPT, hid_endpoint1.wMaxPacketSize, NULL);
        if (dual_report)
        {
            usbd_ep_setup(dev, USB_ENDPOINT_ADDR_IN(7), USB_ENDPOINT_ATTR_INTERRUPT, hid_endpoint2.wMaxPacketSize, NULL);
        }
    }

    usbd_register_control_callback(
                dev,
                USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
                USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
                hid_control_request);

    // 72MHz / 8 => 9000000 counts per second
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);

    // 9000000/90000 = 100 overflows per second - every 10ms one interrupt
    // SysTick interrupt every N clock pulses: set reload to N-1
    systick_set_reload(89999);
    systick_interrupt_enable();
    systick_counter_enable();
}

int main(void)
{
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

    rcc_periph_clock_enable(RCC_GPIOC);
    // configure internal LED pin PC13 and turn LED off
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
    gpio_set(GPIOC, GPIO13);

    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_AFIO);
    // disable JTAG, keep SWD enabled
    gpio_primary_remap(AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON, 0);
    // configure Sense1 (PB4) and Sense2 (PB3) as inputs with internal pull-down and enable the internal pull-down
    gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, GPIO_SENSE1 | GPIO_SENSE2);
    gpio_clear(GPIOB, GPIO_SENSE1 | GPIO_SENSE2);  // pull-down when cleared
    // configure BOOT1 (PB2) as floating input
    gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO2);

    // configure I2C1 in disabled state
    rcc_periph_clock_enable(RCC_I2C1);
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO_I2C1_SCL | GPIO_I2C1_SDA);
    i2c_peripheral_disable(I2C1);

    // configure I2C2 in disabled state
    rcc_periph_clock_enable(RCC_I2C2);
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_OPENDRAIN, GPIO_I2C2_SCL | GPIO_I2C2_SDA);
    i2c_peripheral_disable(I2C2);

    // configure timer
    rcc_periph_clock_enable(RCC_TIM2);
    timer_set_prescaler(TIM2, 72 - 1); // 72MHz / 72 = 1MHz (1µs per tick)
    timer_set_period(TIM2, 0xFFFF);
    timer_enable_counter(TIM2);

    // detect whether to report two controllers or one
    if (gpio_get(GPIOB, GPIO_SENSE1 | GPIO_SENSE2) == (GPIO_SENSE1 | GPIO_SENSE2))
    {
        // detect whether to use alternative reporting for two controllers
        if (gpio_get(GPIOB, GPIO2))
        {
            dual_report = 2;
        }
        else
        {
            dual_report = 1;
        }
    }

    rcc_periph_clock_enable(RCC_GPIOA);
    /*
     * This is a somewhat common cheap hack to trigger device re-enumeration
     * on startup.  Assuming a fixed external pullup on D+, (For USB-FS)
     * setting the pin to output, and driving it explicitly low effectively
     * "removes" the pullup.  The subsequent USB init will "take over" the
     * pin, and it will appear as a proper pullup to the host.
     * The magic delay is somewhat arbitrary, no guarantees on USBIF
     * compliance here, but "it works" in most places.
     */
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
    gpio_clear(GPIOA, GPIO12);
    for (unsigned i = 0; i < 800000; i++) {
        __asm__("nop");
    }

    if (dual_report)
    {
        if (dual_report == 2)
        {
            usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config3, usb_strings, 3, usbd_control_buffer, sizeof(usbd_control_buffer));
        }
        else
        {
            usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config2, usb_strings, 4, usbd_control_buffer, sizeof(usbd_control_buffer));
        }
    }
    else
    {
        usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev_descr, &config1, usb_strings, 3, usbd_control_buffer, sizeof(usbd_control_buffer));
    }
    usbd_register_set_config_callback(usbd_dev, hid_set_config);

    while (1)
        usbd_poll(usbd_dev);
}


static bool check_controller_initialized(unsigned int index)
{
    if (controller_state[index] != 0)
    {
        if (!gpio_get(GPIOB, index ? GPIO_SENSE2 : GPIO_SENSE1))
        {
            controller_state[index] = 0;
            i2c_peripheral_disable(index ? I2C2 : I2C1);
        }
        else if (controller_state[index] > 0)
        {
            return true;
        }
    }
    return false;
}

static bool i2c_write7_timeout(uint32_t i2c, int addr, const uint8_t *data, size_t n, uint32_t timeout_us)
{
    timer_set_counter(TIM2, 0);

    while ((I2C_SR2(i2c) & I2C_SR2_BUSY))
    {
        if (timer_get_counter(TIM2) >= timeout_us) return false;
    }

    i2c_send_start(i2c);

    // Wait for the end of the start condition, master mode selected, and BUSY bit set
    while ( !( (I2C_SR1(i2c) & I2C_SR1_SB)
        && (I2C_SR2(i2c) & I2C_SR2_MSL)
        && (I2C_SR2(i2c) & I2C_SR2_BUSY) ))
    {
        if (timer_get_counter(TIM2) >= timeout_us) return false;
    }

    i2c_send_7bit_address(i2c, addr, I2C_WRITE);

    // Waiting for address is transferred.
    while (!(I2C_SR1(i2c) & I2C_SR1_ADDR))
    {
        if (timer_get_counter(TIM2) >= timeout_us) return false;
    }

    // Clearing ADDR condition sequence.
    (void)I2C_SR2(i2c);

    for (size_t i = 0; i < n; i++)
    {
        i2c_send_data(i2c, data[i]);
        while (!(I2C_SR1(i2c) & (I2C_SR1_BTF)))
        {
            if (timer_get_counter(TIM2) >= timeout_us) return false;
        }
    }

    i2c_send_stop(i2c);

    return true;
}

static bool i2c_read7_timeout(uint32_t i2c, int addr, uint8_t *res, size_t n, uint32_t timeout_us)
{
    timer_set_counter(TIM2, 0);

    i2c_send_start(i2c);
    i2c_enable_ack(i2c);

    // Wait for the end of the start condition, master mode selected, and BUSY bit set
    while ( !( (I2C_SR1(i2c) & I2C_SR1_SB)
        && (I2C_SR2(i2c) & I2C_SR2_MSL)
        && (I2C_SR2(i2c) & I2C_SR2_BUSY) ))
    {
        if (timer_get_counter(TIM2) >= timeout_us) return false;
    }

    i2c_send_7bit_address(i2c, addr, I2C_READ);

    // Waiting for address is transferred.
    while (!(I2C_SR1(i2c) & I2C_SR1_ADDR))
    {
        if (timer_get_counter(TIM2) >= timeout_us) return false;
    }
    // Clearing ADDR condition sequence.
    (void)I2C_SR2(i2c);

    for (size_t i = 0; i < n; ++i)
    {
        if (i == n - 1)
        {
            i2c_disable_ack(i2c);
        }
        while (!(I2C_SR1(i2c) & I2C_SR1_RxNE))
        {
            if (timer_get_counter(TIM2) >= timeout_us) return false;
        }
        res[i] = i2c_get_data(i2c);
    }

    i2c_send_stop(i2c);

    return true;
}

static bool setup_controller(uint32_t i2c, uint8_t *data_format)
{
    uint8_t buffer[6];

    if (!i2c_write7_timeout(i2c, 0x52, setup_data, 2, 2 * 150)) return false;
    if (!i2c_write7_timeout(i2c, 0x52, setup_data + 2, 2, 2 * 150)) return false;
    if (!i2c_write7_timeout(i2c, 0x52, setup_data + 4, 2, 2 * 150)) return false;

    buffer[0] = 0xFA;
    if (!i2c_write7_timeout(i2c, 0x52, buffer, 1, 2 * 100)) return false;
    if (!i2c_read7_timeout(i2c, 0x52, buffer, 6, 2 * 350)) return false;

    *data_format = buffer[4];

    return true;
}

void sys_tick_handler(void)
{
    unsigned int index;
    uint8_t data_format;
    uint8_t buffer[9];
    uint32_t buttons[2], value[2];
    uint16_t buf[2];
    static bool led_enabled = false;
    static int led_counter = 0;

    // write read address to I2C1/I2C2
    for (index = 0; index <= 1; index++)
    {
        if (check_controller_initialized(index))
        {
            buffer[0] = 0x00;
            if (!i2c_write7_timeout(index ? I2C2 : I2C1, 0x52, buffer, 1, 2 * 100))
            {
                controller_state[index] = 0;
                i2c_peripheral_disable(index ? I2C2 : I2C1);
            }
        }
    }

    // wait 150µs before reading
    timer_set_counter(TIM2, 0);
    while (timer_get_counter(TIM2) < 150);

    // read data from I2C1/I2C2
    for (index = 0; index <= 1; index++)
    {
        if (check_controller_initialized(index))
        {
            if (i2c_read7_timeout(index ? I2C2 : I2C1, 0x52, buffer, data_length[index], 2 * 500))
            {
                buttons[index] = ~(buffer[data_length[index] - 2] | (buffer[data_length[index] - 1] << 8));
            }
            else
            {
                buttons[index] = 0;
                controller_state[index] = 0;
                i2c_peripheral_disable(index ? I2C2 : I2C1);
            }
        }
        else buttons[index] = 0;
    }

    for (index = 0; index <= 1; index++)
    {
        value[index] = 0;

        if (buttons[index] & 0x0800) value[index] |= 1; // X
        if (buttons[index] & 0x1000) value[index] |= 2; // A
        if (buttons[index] & 0x4000) value[index] |= 4; // B
        if (buttons[index] & 0x2000) value[index] |= 8; // Y

        if (buttons[index] & 0x20) value[index] |= 0x40; // L
        if (buttons[index] & 0x02) value[index] |= 0x80; // R
        if (buttons[index] & 0x10) value[index] |= 0x0100; // Select
        if (buttons[index] & 0x04) value[index] |= 0x0200; // Start

        if (buttons[index] & 0x80) value[index] |= 0x0800; // DPAD X (right)
        else if (!(buttons[index] & 0x0200)) value[index] |= 0x0400; // DPAD X (center)

        if (buttons[index] & 0x40) value[index] |= 0x2000; // DPAD Y (down)
        else if (!(buttons[index] & 0x0100)) value[index] |= 0x1000; // DPAD Y (center)

        buf[index] = value[index];
    }

    if (dual_report)
    {
        if (dual_report == 2)
        {
            uint8_t buf2[3];

            last_report = 1 - last_report;

            buf2[0] = 1 + last_report;
            buf2[1] = buf[last_report];
            buf2[2] = buf[last_report] >> 8;

            usbd_ep_write_packet(usbd_dev, USB_ENDPOINT_ADDR_IN(6), buf2, 3);
        }
        else
        {
            usbd_ep_write_packet(usbd_dev, USB_ENDPOINT_ADDR_IN(6), &(buf[0]), 2);
            usbd_ep_write_packet(usbd_dev, USB_ENDPOINT_ADDR_IN(7), &(buf[1]), 2);
        }
    }
    else
    {
        if (buttons[last_report] & 0x80000000)
        {
            //last_report = last_report;
        }
        else if (buttons[1 - last_report] & 0x80000000)
        {
            last_report = 1 - last_report;
        }
        else last_report = 0;

        usbd_ep_write_packet(usbd_dev, USB_ENDPOINT_ADDR_IN(6), &(buf[last_report]), 2);
    }

    led_counter++;
    if (led_counter > ((controller_state[0] < 0 || controller_state[1] < 0 || (controller_state[0] == 0 && controller_state[1] == 0)) ? 50 : 100))
    {
        led_counter = 0;
        led_enabled = !led_enabled;
        if (led_enabled)
        {
            gpio_clear(GPIOC, GPIO13);
        }
        else
        {
            gpio_set(GPIOC, GPIO13);
        }
    }

    // setup controller 1/2
    for (index = 0; index <= 1; index++)
    {
        if (controller_state[index] == 0)
        {
            if (gpio_get(GPIOB, index ? GPIO_SENSE2 : GPIO_SENSE1))
            {
                rcc_periph_reset_pulse(index ? RST_I2C2 : RST_I2C1);
                i2c_set_speed(index ? I2C2 : I2C1, i2c_speed_fm_400k, 72);
                i2c_peripheral_enable(index ? I2C2 : I2C1);

                if (gpio_get(GPIOB, index ? GPIO_SENSE2 : GPIO_SENSE1))
                {
                    if (setup_controller(index ? I2C2 : I2C1, &data_format))
                    {
                        if (data_format == 1)
                        {
                            controller_state[index] = 1;
                            data_length[index] = 6;
                        }
                        else if (data_format == 2)
                        {
                            controller_state[index] = 1;
                            data_length[index] = 9;
                        }
                        else if (data_format == 3)
                        {
                            controller_state[index] = 1;
                            data_length[index] = 8;
                        }
                        else
                        {
                            controller_state[index] = -1;
                            i2c_peripheral_disable(index ? I2C2 : I2C1);
                        }
                    }
                    else
                    {
                        i2c_peripheral_disable(index ? I2C2 : I2C1);
                    }
                }
                else
                {
                    i2c_peripheral_disable(index ? I2C2 : I2C1);
                }
            }
        }
    }

}

