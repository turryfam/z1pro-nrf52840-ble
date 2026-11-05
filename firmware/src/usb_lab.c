/* Z1PRO ZMK diagnostic/control interface, firmware tag Z1LAB004.
 * Secondary application Flash programming is disabled in this build.
 * Explicit recovery uses the inspected original vendor bootloader marker.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/reboot.h>
#include <string.h>
#include <stdint.h>
#include <zephyr/init.h>
#ifdef Z1PRO_WITH_ZMK
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/usb.h>
#include <zephyr/bluetooth/conn.h>
#include <z1pro/low_latency_queue.h>
#endif

#define REPORT_SIZE 64
#define GPIO_IN0 (*(volatile uint32_t *)0x50000510u)
#define GPIO_IN1 (*(volatile uint32_t *)0x50000810u)
#define FICR_PART (*(volatile uint32_t *)0x10000100u)
#define NVMC_READY (*(volatile uint32_t *)0x4001e400u)
#define NVMC_CONFIG (*(volatile uint32_t *)0x4001e504u)
#define NVMC_ERASEPAGE (*(volatile uint32_t *)0x4001e508u)

static uint8_t response[REPORT_SIZE];
static struct k_work_delayable reset_work;
static uint8_t reset_action;

static void feed_inherited_watchdog(void) {
    if ((*(volatile uint32_t *)0x40010400u) & 1u) {
        uint32_t enabled = *(volatile uint32_t *)0x40010508u;
        for (unsigned int i = 0; i < 8; i++) {
            if (enabled & (1u << i)) {
                *(volatile uint32_t *)(0x40010600u + 4u * i) = 0x6e524635u;
            }
        }
    }
}

static const uint8_t report_descriptor[] = {
    0x06, 0x00, 0xff, 0x09, 0x01, 0xa1, 0x01,
    0x85, 0x01, 0x15, 0x00, 0x26, 0xff, 0x00,
    0x75, 0x08, 0x95, 0x3f, 0x09, 0x01, 0xb1, 0x02, 0xc0
};

static void wait_nvmc(void) {
    while (NVMC_READY == 0u) { }
}

static void factory_marker_and_reset(void) {
    unsigned int key = irq_lock();
    feed_inherited_watchdog();
    wait_nvmc(); NVMC_CONFIG = 2; NVMC_ERASEPAGE = 0x000e5000u; wait_nvmc();
    NVMC_CONFIG = 1;
    *(volatile uint32_t *)0x000e5000u = 0x78563412u; wait_nvmc();
    *(volatile uint32_t *)0x000e5004u = 0x19d2bc9au; wait_nvmc();
    NVMC_CONFIG = 0; irq_unlock(key);
    sys_reboot(SYS_REBOOT_COLD);
}

static int lab_rescue_early(void) {
    volatile uint32_t *middle_cfg = (volatile uint32_t *)0x50000770u;
    volatile uint32_t *dpi_cfg = (volatile uint32_t *)0x50000a3cu;
    uint32_t middle_saved = *middle_cfg, dpi_saved = *dpi_cfg;
    *middle_cfg = 12u; *dpi_cfg = 12u;
    for (volatile unsigned int i = 0; i < 8192; i++) { }
    bool held = !(GPIO_IN0 & (1u << 28)) && !(GPIO_IN1 & (1u << 15));
    *middle_cfg = middle_saved; *dpi_cfg = dpi_saved;
    if (held && FICR_PART == 0x52840u) factory_marker_and_reset();
    return 0;
}
SYS_INIT(lab_rescue_early, PRE_KERNEL_1, 0);

static void reset_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (reset_action == 0x31u) {
        /* Mirrors the inspected factory V2.17 entry marker. Host must verify
         * that the physical bootloader uses this same layout before invoking.
         */
        factory_marker_and_reset();
    }
    sys_reboot(SYS_REBOOT_COLD);
}

static bool readable(uint32_t address, uint32_t size) {
    if (size > 48 || size == 0 || address + size < address) return false;
    uint32_t flash_end = (*(volatile uint32_t *)0x10000010u) *
                         (*(volatile uint32_t *)0x10000014u);
    if (address < flash_end && address + size <= flash_end) return true;
    if (address >= 0x10000100u && address + size <= 0x10000114u) return true;
    if (address >= 0x10001000u && address + size <= 0x10002000u) return true;
    return false;
}

static void finish_response(void) {
    sys_put_le32(crc32_ieee(response, 60), response + 60);
}

static int set_report(const struct device *dev, struct usb_setup_packet *setup,
                      int32_t *length, uint8_t **data) {
    ARG_UNUSED(dev);
    if (*length != REPORT_SIZE || !data || !*data || (*data)[0] != 1) return -EINVAL;
    const uint8_t *request = *data;
    uint32_t want_crc = sys_get_le32(request + 60);
    if (crc32_ieee(request, 60) != want_crc) return -EINVAL;
    memset(response, 0, sizeof(response));
    response[0] = 1; response[1] = request[1]; response[3] = request[3];
    uint32_t address = sys_get_le32(request + 4);
    uint8_t size = request[8];
    sys_put_le32(address, response + 4);
    switch (request[1]) {
    case 0x01:
        response[8] = 32;
        memcpy(response + 12, "Z1LAB004", 8);
        memcpy(response + 20, (const void *)0x10000100u, 20);
        sys_put_le32(CONFIG_FLASH_LOAD_OFFSET, response + 40);
        break;
    case 0x10:
        if (!readable(address, size)) { response[2] = 1; break; }
        response[8] = size;
        memcpy(response + 12, (const void *)(uintptr_t)address, size);
        break;
    case 0x11:
        response[8] = 8;
        sys_put_le32(GPIO_IN0, response + 12);
        sys_put_le32(GPIO_IN1, response + 16);
        break;
    case 0x12:
        response[8] = 16;
        sys_put_le32(*(volatile uint32_t *)0x40000400u, response + 12);
        sys_put_le32(*(volatile uint32_t *)0x40010400u, response + 16);
        sys_put_le32(*(volatile uint32_t *)0x40010504u, response + 20);
        sys_put_le32(*(volatile uint32_t *)0x40010508u, response + 24);
        break;
    case 0x32:
        if (address > 1 || memcmp(request + 12, "MODE", 4)) { response[2] = 2; break; }
        *(volatile uint32_t *)0x40000520u = address ? 0x52u : 0x4du;
        if (size == 1) { reset_action = 0x30; k_work_schedule(&reset_work, K_MSEC(250)); }
        break;
#ifdef Z1PRO_WITH_ZMK
    case 0x13: {
        struct zmk_endpoint_instance endpoint = zmk_endpoint_get_selected();
        response[8] = 32;
        sys_put_le32(endpoint.transport, response + 12);
        sys_put_le32(zmk_endpoint_get_preferred_transport(), response + 16);
        sys_put_le32(zmk_ble_active_profile_index(), response + 20);
        sys_put_le32(zmk_ble_active_profile_is_connected(), response + 24);
        sys_put_le32(zmk_ble_active_profile_is_open(), response + 28);
        sys_put_le32(zmk_usb_get_conn_state(), response + 32);
        sys_put_le32(*(volatile uint32_t *)0x40000520u, response + 36);
        sys_put_le32(*(volatile uint32_t *)0x40000400u, response + 40);
        break;
    }
    case 0x14: {
        response[8]=32;
        uint32_t values[8]={0,0,0,0,(uint32_t)k_uptime_get(),
            CONFIG_PAW3395_REPORT_INTERVAL_MIN,CONFIG_BT_PERIPHERAL_PREF_LATENCY,0};
        struct bt_conn *conn=zmk_ble_active_profile_conn();
        if (conn) {
            struct bt_conn_info info;
            if (!bt_conn_get_info(conn,&info)) {
                values[0]=1;values[1]=info.le.interval;values[2]=info.le.latency;values[3]=info.le.timeout;
            }
            bt_conn_unref(conn);
        }
        for (unsigned int i=0;i<8;i++) sys_put_le32(values[i],response+12+4*i);
        break;
    }
    case 0x15: {
        uint32_t values[8];z1pro_perf_stats(values);response[8]=32;
        for (unsigned int i=0;i<8;i++) sys_put_le32(values[i],response+12+4*i);
        break;
    }
    case 0x33:
        if (address > 1 || memcmp(request + 12, "OUT!", 4)) { response[2] = 2; break; }
        zmk_endpoint_set_preferred_transport(address ? ZMK_TRANSPORT_USB : ZMK_TRANSPORT_BLE);
        break;
#endif
    case 0x20:
    case 0x21:
#ifdef Z1PRO_WITH_ZMK
        response[2] = 3;
#else
        if (request[9] != 0xa5 || request[10] != 0x5a || request[11] != 0xc3 ||
            address < 0x34000u || address >= 0x80000u) {
            response[2] = 2; break;
        }
        if (request[1] == 0x20) {
            if ((address & 4095u) || memcmp(request + 12, "PAGE", 4) || address + 4096u > 0x80000u) {
                response[2] = 1; break;
            }
            unsigned int key = irq_lock(); feed_inherited_watchdog();
            wait_nvmc(); NVMC_CONFIG = 2; NVMC_ERASEPAGE = address; wait_nvmc();
            NVMC_CONFIG = 0; irq_unlock(key);
        } else {
            if ((address & 3u) || !size || size > 48 || (size & 3u) || address + size > 0x80000u) {
                response[2] = 1; break;
            }
            unsigned int key = irq_lock(); feed_inherited_watchdog();
            wait_nvmc(); NVMC_CONFIG = 1;
            for (unsigned int i = 0; i < size; i += 4) {
                *(volatile uint32_t *)(uintptr_t)(address + i) = sys_get_le32(request + 12 + i);
                wait_nvmc();
            }
            NVMC_CONFIG = 0; irq_unlock(key);
        }
#endif
        break;
    case 0x30:
    case 0x31:
        if (FICR_PART != 0x52840u || memcmp(request + 12, "BOOT", 4) != 0) {
            response[2] = 2; break;
        }
        reset_action = request[1];
        k_work_schedule(&reset_work, K_MSEC(250));
        break;
    default:
        response[2] = 0x7f;
        break;
    }
    finish_response();
    return 0;
}

static int get_report(const struct device *dev, struct usb_setup_packet *setup,
                      int32_t *length, uint8_t **data) {
    ARG_UNUSED(dev); ARG_UNUSED(setup);
    *length = sizeof(response); *data = response;
    return 0;
}

static const struct hid_ops hid_callbacks = {
    .get_report = get_report, .set_report = set_report,
};

static int lab_usb_init(void) {
#ifdef Z1PRO_WITH_ZMK
    /* Survive a CPU reset in the selected application. A deliberate RF switch
     * writes 0x52 before resetting. A cold boot uses the selector's default ZMK mode.
     */
    *(volatile uint32_t *)0x40000520u = 0x4du;
    const struct device *hid = device_get_binding("HID_1");
#else
    const struct device *hid = device_get_binding("HID_0");
#endif
    if (!hid) return 0;
    k_work_init_delayable(&reset_work, reset_handler);
    response[0] = 1;
    finish_response();
    usb_hid_register_device(hid, report_descriptor, sizeof(report_descriptor), &hid_callbacks);
    if (usb_hid_init(hid)) return 0;
#ifndef Z1PRO_WITH_ZMK
    if (usb_enable(NULL)) return 0;
#endif
    return 0;
}

#ifdef Z1PRO_WITH_ZMK
static void lab_watchdog_thread(void *a, void *b, void *c) {
    while (true) { feed_inherited_watchdog(); k_sleep(K_MSEC(100)); }
}
K_THREAD_DEFINE(lab_watchdog, 512, lab_watchdog_thread, NULL, NULL, NULL, -1, 0, 0);
SYS_INIT(lab_usb_init, APPLICATION, CONFIG_ZMK_USB_HID_INIT_PRIORITY);
#else
int main(void) {
    feed_inherited_watchdog();
    lab_usb_init();
    while (true) { feed_inherited_watchdog(); k_sleep(K_MSEC(100)); }
    return 0;
}
#endif
