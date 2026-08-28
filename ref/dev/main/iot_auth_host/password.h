#ifndef PASSWORD_H_
#define PASSWORD_H_

#include "stdint.h"

uint32_t CreateEncryptPassword(char *name, uint64_t code, uint32_t nowTime);
#endif
