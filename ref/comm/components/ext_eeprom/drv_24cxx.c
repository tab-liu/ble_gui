#include "drv_24cxx.h"

/* calculate iic device communication address */
#define _CALC_DEVADD(dev_addr, mem_addr)						\
do {															\
	if(_MEMADD_SIZE == 8) {										\
		dev_addr = _CALC_DEVADD_8BIT(_DEV_ADDRESS, mem_addr);	\
	}															\
	else {														\
		dev_addr = _CALC_DEVADD_16BIT(_DEV_ADDRESS, mem_addr);	\
	}															\
} while(0);

static eeprom_24cxx_t eeprom_handle = {0};

/**
* @brief :eeprom_24cxx_program
支持页间写延时的EEPROM写操作，页间延时5~10ms
* @param[in] :	   
* @param[out] : 
* @return :	 
*/
static int eeprom_24cxx_program(uint32_t mem_addr, uint8_t *buf, uint32_t len)
{
	/* the data size not exceed the memory address boundary of the array */
	if((mem_addr+len) > _MEM_MAX_SIZE)
	{
		eeprom_handle.print("%s %d: eeprom data out of range, addr:%u, len:%u\n", __FILE__, __LINE__, mem_addr, len);
		return -1;
	}

	uint16_t dev_addr;
	uint32_t page_remain = _MEM_PAGE_SIZE - mem_addr % _MEM_PAGE_SIZE;

#if (EEPROM_WP_ENABLE > 0)
	eeprom_handle.wp_enable(0); /* Disable Write Protect */
#endif

	if (page_remain >= len) /**< the data written is smaller than the remaining size of the current page */
	{
		_CALC_DEVADD(dev_addr, mem_addr);
		if(eeprom_handle.i2c_write(dev_addr, _MEMADD_SIZE, mem_addr, buf, len) < 0)
		{
			eeprom_handle.print("%s %d: eeprom write failed\n", __FILE__, __LINE__);
			goto __err;
		}
		eeprom_handle.delay_ms(_MEM_STWC);		/**< Delay for Self Timed Write Cycle */
	}
	else
	{
		_CALC_DEVADD(dev_addr, mem_addr);
		if(eeprom_handle.i2c_write(dev_addr, _MEMADD_SIZE, mem_addr, buf, page_remain) < 0)
		{
			eeprom_handle.print("%s %d: eeprom write failed\n", __FILE__, __LINE__);
			goto __err;
		}
		eeprom_handle.delay_ms(_MEM_STWC);		/**< Delay for Self Timed Write Cycle */

		mem_addr += page_remain;				/**< Set new address */
		buf += page_remain; 					/**< Set new data address */
		len -= page_remain;						/**< Set new data length */

		/* Loop for write data */
		for (; ; )
		{
			if (len <= _MEM_PAGE_SIZE) 			/**< Check data length */
			{
				_CALC_DEVADD(dev_addr, mem_addr);
				if(eeprom_handle.i2c_write(dev_addr, _MEMADD_SIZE, mem_addr, buf, len) < 0)
				{
					eeprom_handle.print("%s %d: eeprom write failed\n", __FILE__, __LINE__);
					goto __err;
				}

				eeprom_handle.delay_ms(_MEM_STWC);	/* Delay for Self Timed Write Cycle */
				break;
			}
			else
			{
				_CALC_DEVADD(dev_addr, mem_addr);
				if(eeprom_handle.i2c_write(dev_addr, _MEMADD_SIZE, mem_addr, buf, _MEM_PAGE_SIZE) < 0)
				{
					eeprom_handle.print("%s %d: eeprom write failed\n", __FILE__, __LINE__);
					goto __err;
				}

				mem_addr += _MEM_PAGE_SIZE;		/**< Set new address */
				buf += _MEM_PAGE_SIZE; 			/**< Set new data address */
				len -= _MEM_PAGE_SIZE;			/**< Set new data length */

				eeprom_handle.delay_ms(_MEM_STWC);	/* Delay for Self Timed Write Cycle */
			}
		}
	}

#if (EEPROM_WP_ENABLE > 0)
	eeprom_handle.wp_enable(1); /* Enable Write Protect */
#endif
	eeprom_handle.delay_ms(_MEM_STWC);
	return 0;

__err:
	eeprom_handle.delay_ms(_MEM_STWC);
	return -1;
}

void eeprom_port_init(eeprom_24cxx_t *handle)
{
	eeprom_handle.i2c_write = handle->i2c_write;
	eeprom_handle.i2c_read = handle->i2c_read;
	eeprom_handle.delay_ms = handle->delay_ms;
	eeprom_handle.print = handle->print;
	eeprom_handle.mutex_take = handle->mutex_take;
	eeprom_handle.mutex_release = handle->mutex_release;
	eeprom_handle.inited = 1;

#if (EEPROM_WP_ENABLE > 0)
	eeprom_handle.wp_enable = handle->wp_enable;
#endif
}

/**
* @brief :eeprom_24cxx_write
支持页间写延时的EEPROM写操作，页间延时5~10ms
* @param[in] :	   
* @param[out] : 
* @return :	 

0-ok
非0-fail
*/
int eeprom_24cxx_write(uint32_t mem_addr, uint8_t *buf, uint32_t len)
{
	if(!eeprom_handle.inited)
	{
		eeprom_handle.print("%s %d: eeprom not initialization\n", __FILE__, __LINE__);
		return -1;
	}

	if(len == 0 || len > _MEM_MAX_SIZE)
	{
		eeprom_handle.print("%s %d: eeprom data len out of memory boundary\n", __FILE__, __LINE__);
		return -1;
	}

	eeprom_handle.mutex_take();

	/* When the data size exceeds the memory address boundary of the array, first
	   write the data to the memory area from the current address to the boundary,
	   and then write the remaining data from address 0 */
	if((mem_addr+len) > _MEM_MAX_SIZE)
	{
		if(eeprom_24cxx_program(mem_addr, buf, (_MEM_MAX_SIZE-mem_addr)) < 0) return -1;
		len -= (_MEM_MAX_SIZE - mem_addr);
		buf += (_MEM_MAX_SIZE - mem_addr);
		mem_addr = 0;
	}

	if(eeprom_24cxx_program(mem_addr, buf, len) < 0)
	{
		eeprom_handle.mutex_release();
		return -1;
	}

	eeprom_handle.mutex_release();
	return 0;
}

/**
* @brief :eeprom_24cxx_read
EEPROM多字节读操作 
* @param[in] :	   
* @param[out] : 
* @return :	 

0-ok
非0-fail
*/
int eeprom_24cxx_read(uint32_t mem_addr, uint8_t *buf, uint32_t len)
{
	if(!eeprom_handle.inited)
	{
		eeprom_handle.print("%s %d: eeprom not initialization\n", __FILE__, __LINE__);
		return -1;
	}

	if(len == 0 || len > _MEM_MAX_SIZE || mem_addr >= _MEM_MAX_SIZE)
	{
		eeprom_handle.print("%s %d: eeprom data len or address out of memory boundary\n", __FILE__, __LINE__);
		return -1;
	}

	eeprom_handle.mutex_take();

	uint16_t dev_addr;
	if((_MEM_MAX_SIZE - mem_addr) < len)
	{
		_CALC_DEVADD(dev_addr, mem_addr);
		if(eeprom_handle.i2c_read(dev_addr, _MEMADD_SIZE, mem_addr, buf, (_MEM_MAX_SIZE-mem_addr)) < 0)
		{
			eeprom_handle.print("%s %d: eeprom read failed\n", __FILE__, __LINE__);
			goto __err;
		}

		len -= (_MEM_MAX_SIZE - mem_addr);
		buf += (_MEM_MAX_SIZE - mem_addr);
		mem_addr = 0;
	}

	_CALC_DEVADD(dev_addr, mem_addr);
	if(eeprom_handle.i2c_read(dev_addr, _MEMADD_SIZE, mem_addr, buf, len) < 0)
	{
		eeprom_handle.print("%s %d: eeprom read failed\n", __FILE__, __LINE__);
		goto __err;
	}

	eeprom_handle.mutex_release();
	return 0;

__err:
	eeprom_handle.mutex_release();
	return -1;
}


