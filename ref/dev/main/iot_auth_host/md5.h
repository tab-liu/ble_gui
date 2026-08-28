#ifndef __MD5_H
#define __MD5_H	 

#include "stdint.h"
#include "stdio.h"	

// typedef unsigned long int UINT32;
// typedef unsigned short int UINT16;

/* MD5 context. */
typedef struct {
	uint32_t state[4];                                   /* state (ABCD) */
	uint32_t count[2];        /* number of bits, modulo 2^64 (lsb first) */
	unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

void MD5Init (MD5_CTX *context);
void MD5Update (MD5_CTX *context, unsigned char *input, unsigned int inputLen);
void MD5Final (  unsigned char * digest,MD5_CTX * context);

//void MD5Final ( MD5_CTX * context, unsigned char digest[16])//参数顺序和IDF5 库顺序颠倒

void md5calc( unsigned char *pInput, unsigned int length, unsigned char *pOutput );


/*
windy:
md5定义 ESP32 idf库有代码

\Espressif\frameworks\esp-idf-v5.1.2\components\esp_rom\include\esp32s3\rom\md5_hash.h

*/
#endif
