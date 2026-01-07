#ifndef _APP_HTTP_OTA_H_
#define _APP_HTTP_OTA_H_

#include <stdint.h>

uint8_t do_http_request(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *destip, uint16_t destport);

uint32_t ota_post_version(uint8_t *pkt, const char *pro_id, const char *dev_name, const char *auth, const char *s_version, const char *f_version);
uint32_t ota_get_update_task(uint8_t *pkt, uint32_t pkt_size, const char *pro_id, const char *dev_name, const char *type, const char *version, const char *auth);
uint32_t ota_get_download(uint8_t *pkt, uint32_t pkt_size, const char *pro_id, const char *dev_name, uint32_t tid, const char *auth, const char *range);

#endif