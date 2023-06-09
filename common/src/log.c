/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "log.h"

#include <stdint.h>
#include "config.h"
#include "securec.h"

static void HcfOutPrint(const char *buf, HcfLogLevel level)
{
    switch (level) {
        case HCF_LOG_LEVEL_DEBUG:
            HCF_LOG_DEBUG(buf);
            break;
        case HCF_LOG_LEVEL_INFO:
            HCF_LOG_INFO(buf);
            break;
        case HCF_LOG_LEVEL_WARN:
            HCF_LOG_WARN(buf);
            break;
        case HCF_LOG_LEVEL_ERROR:
            HCF_LOG_ERROR(buf);
            break;
        default:
            break;
    }
}

void HcfLogPrint(HcfLogLevel level, const char *funName, const char *fmt, ...)
{
    int32_t ulPos = 0;
    char outStr[LOG_PRINT_MAX_LEN] = {0};
    int32_t ret = sprintf_s(outStr, sizeof(outStr), "%s: ", funName);
    if (ret < 0) {
        return;
    }
    ulPos = strlen(outStr);
    va_list arg;
    va_start(arg, fmt);
    ret = vsprintf_s(&outStr[ulPos], sizeof(outStr) - ulPos, fmt, arg);
    va_end(arg);
    if (ret < 0) {
        return;
    }
    HcfOutPrint(outStr, level);
}
