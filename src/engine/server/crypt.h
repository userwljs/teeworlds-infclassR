#pragma once

#include <stdint.h>

void Crypt(const char *pass, const char *salt, int32_t iterations, uint32_t outputBytes, char *hexResult);
