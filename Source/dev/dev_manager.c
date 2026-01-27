#include "dev_manager.h"
#if defined PUBLIC_HOST
#include "../Source/dev/dev_public_host.h"
#endif

void dev_manmager_init(void)
{
#if defined PUBLIC_HOST
    dev_public_host(); // 初始化工区主机
#endif
}