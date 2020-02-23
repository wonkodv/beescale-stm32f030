void lora_reset(void);
void lora_up(void);
void lora_down(void);
void lora_explicit_header_mode(void);
void lora_implicit_header_mode(uint8_t size);
void lora_idle(void);
void lora_sleep(void);
void lora_receive(void);
void lora_set_tx_power(uint8_t level);
void lora_set_frequency(uint32_t frequency);
void lora_set_spreading_factor(uint8_t sf);
void lora_set_bandwidth(uint8_t bw);
void lora_set_coding_rate(uint8_t denominator);
void lora_set_preamble_length(uint32_t length);
void lora_set_sync_word(uint8_t sw);
void lora_enable_crc(void);
void lora_disable_crc(void);
void lora_init(void);
void lora_send_packet(void *buf, uint8_t size);
int16_t lora_receive_packet(void *buf, uint8_t size);
uint8_t lora_available(void);
uint8_t lora_packet_rssi(void);
float lora_packet_snr(void);

#if DEBUG==1
void lora_selftest();
#endif


#define LORA_FREQ_868M 868*1000*1000
#define LORA_FREQ_915M 915*1000*1000


#define LORA_BW_7800Hz   0
#define LORA_BW_10400Hz  1
#define LORA_BW_15600Hz  2
#define LORA_BW_20800Hz  3
#define LORA_BW_31250Hz  4
#define LORA_BW_41700Hz  5
#define LORA_BW_62500Hz  6
#define LORA_BW_125kHz   7
#define LORA_BW_250kHz   8
#define LORA_BW_500kHz   9
#define LORA_BW_MAX LORA_BW_500kHz

#define LORA_ERR_NO_PACKET  -1
#define LORA_ERR_CRC_ERR    -2
