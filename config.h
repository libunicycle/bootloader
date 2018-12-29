#include <stdint.h>
#include "stdlib.h"

size_t config_to_string(char *ptr, size_t max);
void config_init(const char *ptr, size_t len);
const char *config_get(const char *key, const char *_default);
uint32_t config_get_uint32(const char *key, uint32_t _default);