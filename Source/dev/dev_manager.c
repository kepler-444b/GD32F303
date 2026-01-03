#include "dev_manager.h"
#if defined PUBLIC_HOST
#include "../Source/app/app_public_protocol/app_public_protocol.h"
#endif

void dev_manmager_init(void)
{
#if defined PUBLIC_HOST
    app_public_protocol_init(); // 初始化panel相关的协议层
#endif
}