#ifndef __LINUX_H__
#define __LINUX_H__

#define HTTPD_MAX_CONNECTIONS CONFIG_ESPHTTPD_MAX_CONNECTIONS

#include "stdint.h"
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <stdint.h>

#include "platform.h"

#endif // __LINUX_H__