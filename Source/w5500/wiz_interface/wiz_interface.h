#ifndef __WIZ_INTERFACE_H__
#define __WIZ_INTERFACE_H__

#include "../ioLibrary_Driver/Ethernet/wizchip_conf.h"

void wizchip_initialize(void);
void network_init(uint8_t *ethernet_buff, wiz_NetInfo *conf_info);

#endif
