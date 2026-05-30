#ifndef APP_TOKEN_H_
#define APP_TOKEN_H_

#include <stdint.h>
#include <stdbool.h>
#include "../Source/app/app_network_model/app_network_info.h"
#include "../Source/dev/dev_info.h"
 
bool app_token_generate(dev_save_info_t *dev);

#endif