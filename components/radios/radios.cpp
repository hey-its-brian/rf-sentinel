// RF Sentinel radio front end. RECEIVE ONLY.
//
// The RFS_RX_ONLY guard below is deliberate: if someone later adds a call to
// transmit()/startTransmit() in this file the build breaks instead of the
// node quietly becoming a transmitter. Keep it.
#include "radios.h"
#include <RadioLib.h>
#include <hal/ESP-IDF/EspHal.h>
#include "esp_log.h"
#include "sdkconfig.h"

// Receive-only guard. Any transmit()/startTransmit() call written below this
// line expands to a static_assert and fails the build. Keep it below the
// includes so it never touches RadioLib's own declarations.
#define RFS_RX_ONLY 1
#if RFS_RX_ONLY
#define transmit(...)       static_assert(false, "RF Sentinel is receive only")
#define startTransmit(...)  static_assert(false, "RF Sentinel is receive only")
#endif

static const char *TAG = "radios";

// Thin subclass so we can read the live RSSI status register while in RX.
// RadioLib's getRSSI() only returns the last packet's RSSI unless direct mode
// is on, which is not what a band scanner wants.
class CC1101Scanner : public CC1101 {
public:
    using CC1101::CC1101;
    int8_t liveRssiDbm() {
        uint8_t raw = getMod()->SPIreadRegister(RADIOLIB_CC1101_REG_RSSI | RADIOLIB_CC1101_CMD_BURST);
        int v = (raw >= 128) ? ((int)raw - 256) : (int)raw;
        return (int8_t)(v / 2 - 74);
    }
};

static EspHal *s_hal;
static CC1101Scanner *s_cc;
static nRF24 *s_nrf;
#if CONFIG_RFS_LORA
static SX1262 *s_lora;
static volatile int s_lora_pkts;
#endif
static radios_status_t s_status;

radios_status_t radios_init(void) {
    s_hal = new EspHal(CONFIG_RFS_PIN_SCK, CONFIG_RFS_PIN_MISO, CONFIG_RFS_PIN_MOSI, SPI2_HOST, 4000000);

    // CC1101: Module(hal, cs, gdo0, rst=NC, gdo2=NC)
    s_cc = new CC1101Scanner(new Module(s_hal, CONFIG_RFS_PIN_CC_CS,
        CONFIG_RFS_PIN_CC_GDO0 < 0 ? RADIOLIB_NC : CONFIG_RFS_PIN_CC_GDO0, RADIOLIB_NC, RADIOLIB_NC));
    // 433.92 MHz, 4.8 kbps, 5 kHz dev, 101 kHz RX bandwidth (wide-ish for scanning), power ignored.
    int st = s_cc->begin(433.92f, 4.8f, 5.0f, 101.6f, 0, 16);
    if (st == RADIOLIB_ERR_NONE) {
        s_cc->setPromiscuousMode(true);   // no sync-word filtering
        s_cc->setCrcFiltering(false);
        s_status.cc1101_ok = true;
        ESP_LOGI(TAG, "CC1101 ok");
    } else {
        ESP_LOGW(TAG, "CC1101 missing (code %d)", st);
    }

    // nRF24: Module(hal, cs, irq=NC, rst slot = CE)
    s_nrf = new nRF24(new Module(s_hal, CONFIG_RFS_PIN_NRF_CS, RADIOLIB_NC, CONFIG_RFS_PIN_NRF_CE));
    st = s_nrf->begin(2400, 1000, -12, 5);
    if (st == RADIOLIB_ERR_NONE) {
        // Auto-ack MUST stay off: with it on the chip would transmit ACKs by itself.
        s_nrf->setAutoAck(false);
        s_nrf->setCrcFiltering(false);
        s_status.nrf24_ok = true;
        ESP_LOGI(TAG, "nRF24 ok");
    } else {
        ESP_LOGW(TAG, "nRF24 missing (code %d)", st);
    }

#if CONFIG_RFS_LORA
    // SX1262: Module(hal, nss, dio1, reset, busy)
    s_lora = new SX1262(new Module(s_hal, CONFIG_RFS_PIN_LORA_NSS, CONFIG_RFS_PIN_LORA_DIO1,
                                   CONFIG_RFS_PIN_LORA_RST, CONFIG_RFS_PIN_LORA_BUSY));
    st = s_lora->begin((float)CONFIG_RFS_LORA_FREQ_MHZ, 125.0f, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 0, 8, 1.6f, false);
    if (st == RADIOLIB_ERR_NONE) {
        s_lora->setPacketReceivedAction([]() { s_lora_pkts++; });
        s_lora->startReceive();
        s_status.lora_ok = true;
        ESP_LOGI(TAG, "SX1262 ok, listening on %d MHz", CONFIG_RFS_LORA_FREQ_MHZ);
    } else {
        ESP_LOGW(TAG, "SX1262 missing (code %d)", st);
    }
#endif
    return s_status;
}

radios_status_t radios_status(void) { return s_status; }

void radios_sweep_sub(int8_t out[RFS_SUB_CHANNELS]) {
    for (int i = 0; i < RFS_SUB_CHANNELS; i++) out[i] = -120;
    if (!s_status.cc1101_ok) return;
    for (int i = 0; i < RFS_SUB_CHANNELS; i++) {
        float f = RFS_SUB_FIRST_MHZ + i * RFS_SUB_STEP_MHZ;
        s_cc->setFrequency(f);
        s_cc->startReceive();
        s_hal->delayMicroseconds(1500);   // let the RSSI estimator settle
        int8_t best = s_cc->liveRssiDbm();
        s_hal->delayMicroseconds(500);
        int8_t r2 = s_cc->liveRssiDbm();
        if (r2 > best) best = r2;
        out[i] = best;
    }
    s_cc->standby();
}

void radios_sweep_nrf(uint8_t out[RFS_NRF_CHANNELS]) {
    for (int i = 0; i < RFS_NRF_CHANNELS; i++) out[i] = 0;
    if (!s_status.nrf24_ok) return;
    for (int ch = 0; ch < RFS_NRF_CHANNELS; ch++) {
        s_nrf->setFrequency(2400 + ch);
        s_nrf->startReceive();            // CE high, PRX
        s_hal->delayMicroseconds(200);    // RPD needs ~170 us of RX
        out[ch] = s_nrf->isCarrierDetected() ? 1 : 0;
        s_nrf->standby();                 // CE low
    }
}

int radios_lora_poll(float *rssi, float *snr) {
    *rssi = 0; *snr = 0;
#if CONFIG_RFS_LORA
    if (!s_status.lora_ok) return 0;
    int n = s_lora_pkts;
    if (n) {
        s_lora_pkts = 0;
        uint8_t buf[64];
        size_t len = s_lora->getPacketLength();
        if (len > sizeof buf) len = sizeof buf;
        s_lora->readData(buf, len);
        *rssi = s_lora->getRSSI();
        *snr = s_lora->getSNR();
        s_lora->startReceive();
    }
    return n;
#else
    return 0;
#endif
}
