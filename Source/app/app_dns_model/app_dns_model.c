#include "app_dns_model.h"
#include "../../Source/w5500/ioLibrary_Driver/Ethernet/wizchip_conf.h"
#include "../../Source/w5500/ioLibrary_Driver/Internet/DNS/dns.h"
#include "../../Source/w5500/wiz_interface/wiz_interface.h"
#include "../../Source/bsp/bsp_usart/bsp_usart.h"
#include <stdint.h>

int app_dns_init(uint8_t *buf, uint8_t *domain_name, uint8_t *domain_ip)
{
    int dns_ok_flag  = 0;
    int dns_run_flag = 1;
    wiz_NetInfo net_info;
    uint8_t dns_retry_cnt = 0;
    DNS_init(0, buf); // DNS client init
    wizchip_getnetinfo(&net_info);
    while (1) {

        switch (DNS_run(net_info.dns, domain_name, domain_ip)) // 读取 DNS_run 的返回值
        {
            case DNS_RET_FAIL: { // DNS 解析失败
                if (dns_retry_cnt < DNS_RETRY) {
                    dns_retry_cnt++;
                } else {
                    printf("> DNS Failed\r\n");
                    dns_ok_flag  = -1;
                    dns_run_flag = 0;
                }
                break;
            }
            case DNS_RET_SUCCESS: {
                printf("> Translated %s to %d.%d.%d.%d\r\n", domain_name, domain_ip[0], domain_ip[1], domain_ip[2], domain_ip[3]);
                dns_ok_flag  = 0;
                dns_run_flag = 0;
                break;
            }
        }
        if (dns_run_flag != 1) {
            return dns_ok_flag;
        }
    }
}