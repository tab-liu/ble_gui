#include "modbus_master.h"

#define DEFAULT_ADDRESS     0x01

#define TAG "[MODBUS_MASTER]"

// const reg_array* Master_vLookupDataTab(uint16_t iReadAddr, uint16_t iReadNum, bool is_write)


/*------------------------------------------------------------------------
*@Function： Modbus_MasterWriteRegs

-------------------------------------------------------------------------*/
/**
*@brief
*@param[in]     None
*@param[out]    None
*@return
modbus报文字节长度
*/
uint16_t Modbus_MasterWriteRegs (uint8_t addr, uint16_t start, uint16_t reg_num, uint8_t *out_buf)
{
    uint16_t crc;
    uint16_t i = 0;
//    const reg_array* pdata = NULL;
	//ESP_LOGI(TAG,"Modbus_MasterWriteRegs ADDR:0X%x cnt:%d",addr,reg_num);
	const uint16_t *p_tab2 = Master_vLookupDataTab2(addr ,start, reg_num, false);			// 查询table2中的数据
	if (p_tab2 == NULL)
	{
		return 0;

	}

    out_buf[i++] = addr;
    if (reg_num == 1) {
        out_buf[i++] = 0x06; // write single
    } else  {
        out_buf[i++] = 0x10; // write muitl
    }

    out_buf[i++] = (unsigned char)(start >> 8);
    out_buf[i++] = (unsigned char) start;

    if (out_buf[1] == 0x10) {
        out_buf[i++] = (unsigned char)(reg_num >> 8);
        out_buf[i++] = (unsigned char) reg_num;
        out_buf[i++] = reg_num << 1; //  字节数
    }

    uint16_t *dst = (uint16_t *)&out_buf[i];
    for (uint16_t j = 0; j < reg_num; j++, i += 2)
	{

		{
			/* table2中的数据 */
			dst[j] = LSB2MSB(p_tab2[j]);
		}
    }

    crc = ModbusCrc16(out_buf,i);
    out_buf[i++] = (unsigned char) crc;
    out_buf[i++] = (unsigned char)(crc>>8);
    return i;
}

uint16_t Modbus_WriteCmd_06H_10H_Build(uint8_t slave_address, uint16_t regAddress, uint8_t regNum, uint8_t *inbuf, uint8_t *outbuf)
{
    uint16_t crc;
    uint16_t i = 0, j = 0;

    outbuf[i++] = slave_address;

    if (regNum == 1){
        outbuf[i++] = 0x06; // write single
    }
    else {
        outbuf[i++] = 0x10; // write muitl
    }

    outbuf[i++] = (unsigned char)(regAddress >> 8);
    outbuf[i++] = (unsigned char) regAddress;

    if (outbuf[1] == 0x10)
    {
        outbuf[i++] = (unsigned char)(regNum >> 8);
        outbuf[i++] = (unsigned char) regNum;
        outbuf[i++] = regNum << 1; //
    }

    while(regNum--)
    {
        outbuf[i++] =	inbuf[j+1];//H
        outbuf[i++] =	inbuf[j];//L
        j += 2;
    }

    crc = ModbusCrc16(outbuf,i);

    outbuf[i++] = (unsigned char) crc;
    outbuf[i++] = (unsigned char)(crc>>8);

    return i;
}
