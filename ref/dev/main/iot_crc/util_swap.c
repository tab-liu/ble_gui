#include "util_swap.h"


/* store cpu byte order, 0 for little endian, 1 for big endian */
static uint8_t is_big_order = 0xff;
							   
/**
  * @brief cpu little or big endian judgment
  * 
  * @return big endian return 1, little endian return 0
  */
int swap_is_cpu_big_endian(void)
{
    union {
        uint32_t i;
        uint8_t s[4];
    } c;

    c.i = 0x12345678;
    return (0x12 == c.s[0]);
}

/**
  * @brief 32-bit host to net byte order conversion
  * 
  * @param host hsot byte order data
  * @return net hsot byte order data
  */
uint32_t swap_htonl(uint32_t host)
{
	if(is_big_order == 0xff) {
		is_big_order = swap_is_cpu_big_endian();
	}
    return is_big_order ? host : UTIL_SWAP_32(host);
}

/**
  * @brief 32-bit net to host byte order conversion
  * 
  * @param net net byte order data
  * @return host hsot byte order data
  */
uint32_t swap_ntohl(uint32_t net)
{
	if(is_big_order == 0xff) {
		is_big_order = swap_is_cpu_big_endian();
	}
    return is_big_order ? net : UTIL_SWAP_32(net);
}

/**
  * @brief 16-bit host to net byte order conversion
  * 
  * @param host hsot byte order data
  * @return net hsot byte order data
  */
uint16_t swap_htons(uint16_t host)
{
	if(is_big_order == 0xff) {
		is_big_order = swap_is_cpu_big_endian();
	}
    return is_big_order ? host : UTIL_SWAP_16(host);
}

/**
  * @brief 16-bit net to host byte order conversion
  * 
  * @param net net byte order data
  * @return host hsot byte order data
  */
uint16_t swap_ntohs(uint16_t net)
{
	if(is_big_order == 0xff) {
		is_big_order = swap_is_cpu_big_endian();
	}
    return is_big_order ? net : UTIL_SWAP_16(net);
}

/**
  * @brief swap memory data
  * 
  * @param buf buffer
  * @param len buffer length
  * @return none
  */
void swap_buffer(uint8_t *buf, uint32_t len)
{
    uint32_t i;
    uint8_t tmp;
    uint32_t half_size = len / 2;

    for(i = 0 ; i < half_size; i++)
    {
        tmp = buf[i];
        buf[i] = buf[len-1-i];
        buf[len-1-i] = tmp;
    }
}


