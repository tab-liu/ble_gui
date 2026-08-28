/************************************************************
Copyright (C), PowerOak Tech. Co., Ltd.
FileName: 
Author: weiyt
Date: 2022/10/10
Description: 分区数据加密保存、解密读取
Version: V1.00
Function List: none
***********************************************************/

//分区划分：信息区(16字节)+数据区(AES密文)
//其中，信息区包括标志(2字节)+明文长度(4字节)+密文长度(4字节)+类型(1字节)+完成标志(1字节)，信息区由api自己维护更新数据

#include "iot_partition.h"
#include "iot_ble_encrypt.h"
#include "iot_rsa.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iot_ota.h"
#include "filesystem.h"
#include "dev_data_record.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#define PARTITION_FLAG '*'
#define PARTITION_SIZE 4096
static const char *TAG = "[partition]";
#define WRITE_CERT_FACTORY 0xaa //工厂模式证书写入
#define WRITE_CERT_NORMAL  0xbb //正常模式证书写入

uint8_t *ca_cert_ptr = NULL;  //CA证书指针
uint16_t ca_cert_ptr_len = 0;
uint8_t *iot_cert_ptr = NULL; //设备证书指针
uint16_t iot_cert_ptr_len = 0;
uint8_t *private_key_ptr = NULL; //私钥指针
uint16_t private_key_ptr_len = 0;

uint8_t *server_ca_cert_ptr = NULL;  //CA证书指针
uint16_t server_ca_cert_ptr_len = 0;
uint8_t *server_cert_ptr = NULL; //设备证书指针
uint16_t server_cert_ptr_len = 0;
uint8_t *server_key_ptr = NULL; //私钥指针
uint16_t server_key_ptr_len = 0;

#ifdef     TCP2_ENCRYPT_ENABLE	
//第二个服务器的证书(对私服务器)
uint8_t *ca_cert_ptr_2 = NULL;  //CA证书指针
uint8_t *iot_cert_ptr_2 = NULL; //设备证书指针
uint8_t *private_key_ptr_2 = NULL; //私钥指针
#else

#endif	
//手动写入CA证书数组 //待还原   daedc8902868af36
char power_my_ca_cert_text[] = 
"-----BEGIN CERTIFICATE-----\n"
"MIIERTCCA0OgAwIBAgIJANrtyJAoaK82MA0GCSqGSIb3DQEBCwUAMIHOMQswCQYD\n"
"VQQGEwJDTjESMBAGA1UECAwJR3Vhbmdkb25nMREwDwYDVQQHDAhTaGVuemhlbjEq\n"
"MCgGA1UECgwhU2hlbnpoZW4gUG93ZXJPYWsgTmV3ZW5lciBDby4sbHRkMSowKAYD\n"
"VQQLDCFTaGVuemhlbiBQb3dlck9hayBOZXdlbmVyIENvLixsdGQxGzAZBgNVBAMM\n"
"ElBvd2VyT2FrICYgQkxVRVRUSTEjMCEGCSqGSIb3DQEJARYUeHVsaWFuZ0Bwb3dl\n"
"cm9hay5uZXQwIBcNMjIxMDIwMDI1NzQ5WhgPMjEyMjA5MjYwMjU3NDlaMIHOMQsw\n"
"CQYDVQQGEwJDTjESMBAGA1UECAwJR3Vhbmdkb25nMREwDwYDVQQHDAhTaGVuemhl\n"
"bjEqMCgGA1UECgwhU2hlbnpoZW4gUG93ZXJPYWsgTmV3ZW5lciBDby4sbHRkMSow\n"
"KAYDVQQLDCFTaGVuemhlbiBQb3dlck9hayBOZXdlbmVyIENvLixsdGQxGzAZBgNV\n"
"BAMMElBvd2VyT2FrICYgQkxVRVRUSTEjMCEGCSqGSIb3DQEJARYUeHVsaWFuZ0Bw\n"
"b3dlcm9hay5uZXQwggEKMA0GCSqGSIb3DQEBAQUAA4H4ADCB9AKB7ACuMN2LWTp2\n"
"a9PYQBdaVi9FusG3xH/bm22Xft+8VZNgSA/e6AxIr+3dfKwPNcrIrSyOzY0zJcU9\n"
"O2NhU9hUchIXAclTiDMcuztgCTeBPiY14DCVXZBwMk0GhuxAgBJ4m/77kQihygZk\n"
"8PL5k58RDxJ6DMbVY460sCpFyt0fZ6v/lkrrF//9CJPMfH5g+NLNLvalzuX7lDVN\n"
"/z2+vtvX69Tid7E70jA0tqZUIlUKV9Lca2aft9JndGjhsSKVRWJWkJMsvHldfaYL\n"
"Dy4u97v5PL+M5EeZf+Odant2PcTnEVxFPXE2PfAMBVlmO9QHAgMBAAGjUDBOMB0G\n"
"A1UdDgQWBBQZIdazj1204uVO+0ALgovWhDE5MTAfBgNVHSMEGDAWgBQZIdazj120\n"
"4uVO+0ALgovWhDE5MTAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBCwUAA4HsAFCh\n"
"N4+H6M1SEE0nJXvJr/GRV9iJPipQy5MEXWNENw575LWaom+MJDnKrpjfO/rBSVc5\n"
"mEbI7QkE+K2nfbICPmiURIkhwhhUP4FVhNPvlvC7qadIWfh3wmz47i+SQ3I1Ap62\n"
"0HYTFsi6e+4Hem5kjcib4FF8vF0Zv1k/rv4nWXeT9r+w8rUv+toKI2siuGW3FTMk\n"
"EOZkXE/nxo7EQEag0c0IvzspsEuEorN4ZgaPrCTVFQCuDtlHtJwwxsqTuxih0VEP\n"
"f08eOvNdErQV+3Yrrn2TBf1r642vV8fgw66F2rkB97spLyMzegGyOtk=\n"
"-----END CERTIFICATE-----";

//手动写入设备证书数组 testwx过期证书  2  35cf17fc6802000  IOT2236002361142
char power_mqtt_cert[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIFFTCCBBOgAwIBAgIIA1zxf8aAIAAwDQYJKoZIhvcNAQELBQAwgc4xCzAJBgNV\n"
"BAYTAkNOMRIwEAYDVQQIDAlHdWFuZ2RvbmcxETAPBgNVBAcMCFNoZW56aGVuMSow\n"
"KAYDVQQKDCFTaGVuemhlbiBQb3dlck9hayBOZXdlbmVyIENvLixsdGQxKjAoBgNV\n"
"BAsMIVNoZW56aGVuIFBvd2VyT2FrIE5ld2VuZXIgQ28uLGx0ZDEbMBkGA1UEAwwS\n"
"UG93ZXJPYWsgJiBCTFVFVFRJMSMwIQYJKoZIhvcNAQkBFhR4dWxpYW5nQHBvd2Vy\n"
"b2FrLm5ldDAeFw0yMjEwMzEwOTA4NTBaFw0yMzEwMzEwOTA4NTBaMIGnMRkwFwYD\n"
"VQQDExBJT1QyMjM2MDAyMzYxMTQyMSowKAYDVQQLEyFTaGVuemhlbiBQb3dlck9h\n"
"ayBOZXdlbmVyIENvLixsdGQxKjAoBgNVBAoTIVNoZW56aGVuIFBvd2VyT2FrIE5l\n"
"d2VuZXIgQ28uLGx0ZDERMA8GA1UEBxMIU2hlbnpoZW4xEjAQBgNVBAgTCUd1YW5n\n"
"ZG9uZzELMAkGA1UEBhMCQ04wggEKMA0GCSqGSIb3DQEBAQUAA4H4ADCB9AKB7ADn\n"
"/xD4Jh86od5yfuoGHqeTFtwm5uuiH7zkrD/jc7MZrNlCsY8p+9wBN/Ceo/FwRhGM\n"
"HRitwW/W9C8iUpy1zAwsu/NeMDBP85qNeCa1bL0dhHcQw2G9OcTrzdiNwzpXDDtG\n"
"MrKi67KvQYVgmhDombCfN3TATyCz0ThsVSoFr1/h2Hn7MdED7/b0GcTxlgnHFO1L\n"
"1Ax75TRfvISZQ1tq8VeU9L0XCxMPnOGIsxcHNNCmABDYqWHQxGhlPHvtEJ/KC18a\n"
"tHqOEchoXJ1v7wY324WPTAhPqGxsVbYUzvhcVbL2i8zb670y/470vh1PAgMBAAGj\n"
"ggFIMIIBRDAMBgNVHRMBAf8EAjAAMA4GA1UdDwEB/wQEAwIEkDCCAQMGA1UdIwSB\n"
"+zCB+IAUGSHWs49dtOLlTvtAC4KL1oQxOTGhgdSkgdEwgc4xCzAJBgNVBAYTAkNO\n"
"MRIwEAYDVQQIDAlHdWFuZ2RvbmcxETAPBgNVBAcMCFNoZW56aGVuMSowKAYDVQQK\n"
"DCFTaGVuemhlbiBQb3dlck9hayBOZXdlbmVyIENvLixsdGQxKjAoBgNVBAsMIVNo\n"
"ZW56aGVuIFBvd2VyT2FrIE5ld2VuZXIgQ28uLGx0ZDEbMBkGA1UEAwwSUG93ZXJP\n"
"YWsgJiBCTFVFVFRJMSMwIQYJKoZIhvcNAQkBFhR4dWxpYW5nQHBvd2Vyb2FrLm5l\n"
"dIIJANrtyJAoaK82MB0GA1UdDgQWBBSm2+R0thB8HTs2gQ6u7ltm0Az0qzANBgkq\n"
"hkiG9w0BAQsFAAOB7ABiQM+i0s7cEGoaoi4I6I+4yCuVE9NLyOl1lxJyQwCXRsld\n"
"jXe847s5aQEXkEnVDdDtZHkuFHGSCjPZoUZdKWz8HmRXI7iKj1NLbCczpORHNioV\n"
"o+scEuUJV03gx+0yJ6wprbOgJayaI40mQ6FCZ8DIKgkTrP9g/mwynHIqBRZRqKgN\n"
"Yqgt2/7xnyfalVvsjmM7uyXUCDkNIAFUwwRSiTUIb/cWnjmiKkebu1QoC7UPWt5o\n"
"8KZNHIEzPAcHLeV5+Kzt2urbKUpseVdUVRsxSymopMbD4WZYCGzmMVYdNpSjCQaB\n"
"Lotf5Dv7Z/pU\n"
"-----END CERTIFICATE-----";

//手动写入密钥数组 testwx过期证书  2   IOT2236002361142.key
//PKCS#8私钥
char power_private_key[] = 
"-----BEGIN PRIVATE KEY-----\n"
"MIIEWAIBADANBgkqhkiG9w0BAQEFAASCBEIwggQ+AgEAAoHsAOf/EPgmHzqh3nJ+\n"
"6gYep5MW3Cbm66IfvOSsP+Nzsxms2UKxjyn73AE38J6j8XBGEYwdGK3Bb9b0LyJS\n"
"nLXMDCy7814wME/zmo14JrVsvR2EdxDDYb05xOvN2I3DOlcMO0YysqLrsq9BhWCa\n"
"EOiZsJ83dMBPILPROGxVKgWvX+HYefsx0QPv9vQZxPGWCccU7UvUDHvlNF+8hJlD\n"
"W2rxV5T0vRcLEw+c4YizFwc00KYAENipYdDEaGU8e+0Qn8oLXxq0eo4RyGhcnW/v\n"
"BjfbhY9MCE+obGxVthTO+FxVsvaLzNvrvTL/jvS+HU8CAwEAAQKB7ACqLp55GiXw\n"
"cctnPBhZ8uTEbpGCbATQQe3j5UNci4QHQpeBaBT4HExQDIQTK3ox/QRkPvfahjqP\n"
"eUKWL/nLFmqb6ifloP3fuHhYbJognirAW2qoPlsVXypIPBEuNNc4Ab39ibUm9DV4\n"
"gFzh90KODifVM+4OID1rr2q4k0SeIFro45q9yg+wO4AwJFZcv/blRPmcGRuy+8/O\n"
"KuXj0M25p4dYuWKXXBYxkuGn01RraP+iww+A79ib9wimLKQi34g1V52jrAjWb018\n"
"UXXdwC2U1JdaYYeRMCjm1ThdytnKwXC7CV95OGfS3inXQOQRAnYPuMJFiCS5rzxZ\n"
"8o/UFh1iZAEbWTeSTBYLhic48Hub/4avC5dVtyX6Gboa/7O6yeFME51XWUASDHU4\n"
"LRuzTadpfzEkgEgN1LdEQebLzVmf8OEUnNJorZft+ZuPI3Ya3W3uAiXWPntyHsex\n"
"vEo6c0+o0EX7dEnXAnYOwaVLQCmUVJESBVhIV7kB+1CU3FtVJfewGVYm+1EJsMPT\n"
"jjkwfbjKWreKfiYNisMG0pnqmPogG2KaYqgwWOcpQHkqLqQNIyxU/VB7HOMi4JrB\n"
"ZvMXtaszjcRpWBYj50kR/8CD/EYUe7XUajyI0PYZKU/bV4lJAnYF2472nS0hyfqw\n"
"gMG6AatdF2maKPmdlp+4F3nRqzhC/UfPPIBcPWr27lL68D6k38cDs2MyQlyu3Nln\n"
"3tatTceMdQl4UhuGm7TUx8EYOiCkiPkz1uJGgjdTQWcGQ+4jQFFPFpnGY21XLcS1\n"
"ojP95yzM9xy7/a3tAnYC5gSa0GKacTYBrS0XIKwFeKDubI5AHnIVlBR5GbpkReVh\n"
"Q7l2DevjS4hr2rWMyWnfiDSVgTD4Z4ipvKE+xee6EaD7KSguMG9/zjYlyJnQ+v9K\n"
"5/bt/FBBkyI2cwf+S2rt7yeYhVAaq+YecDOtyCXe8V9vPTWJAnYLtmKEOqqMNfhb\n"
"QQUX+OyAUeZQezmrfZJkKdWF9Ais2So69A3Y44jgtrh8XOpUaoYoGnUgdYrEQ93f\n"
"y1haEGYoT1Vx37enOm5W9u5Kxf5XeRgJy3tqZsEN60fN+fWC6RTgV7kicH/PHZiM\n"
"Wxr4ObkoOuZMe7ga\n"
"-----END PRIVATE KEY-----";

#define IOT2446000004549 1

#if IOT2446000004549
char mqtts_iot_cert[] = 
"-----BEGIN CERTIFICATE-----\n"
"MIIFFTCCBBOgAwIBAgIICHEZzcLA0AAwDQYJKoZIhvcNAQELBQAwgc4xCzAJBgNV\n"
"BAYTAkNOMRIwEAYDVQQIDAlHdWFuZ2RvbmcxETAPBgNVBAcMCFNoZW56aGVuMSow\n"
"KAYDVQQKDCFTaGVuemhlbiBQb3dlck9hayBOZXdlbmVyIENvLixsdGQxKjAoBgNV\n"
"BAsMIVNoZW56aGVuIFBvd2VyT2FrIE5ld2VuZXIgQ28uLGx0ZDEbMBkGA1UEAwwS\n"
"UG93ZXJPYWsgJiBCTFVFVFRJMSMwIQYJKoZIhvcNAQkBFhR4dWxpYW5nQHBvd2Vy\n"
"b2FrLm5ldDAeFw0yNTA4MDYwNTUwMTlaFw0yNjA4MDYwNTUwMTlaMIGnMRkwFwYD\n"
"VQQDExBJT1QyNDQ2MDAwMDA0NTQ5MSowKAYDVQQLEyFTaGVuemhlbiBQb3dlck9h\n"
"ayBOZXdlbmVyIENvLixsdGQxKjAoBgNVBAoTIVNoZW56aGVuIFBvd2VyT2FrIE5l\n"
"d2VuZXIgQ28uLGx0ZDERMA8GA1UEBxMIU2hlbnpoZW4xEjAQBgNVBAgTCUd1YW5n\n"
"ZG9uZzELMAkGA1UEBhMCQ04wggEKMA0GCSqGSIb3DQEBAQUAA4H4ADCB9AKB7ACt\n"
"eBSo587slemcASxadUaSEVACexwUfA5JwUYraYK3oyMPPqgm+QmNJvkghGCQN5L0\n"
"vNgcaSsaYTp1BEVeWWFeXovufuSym2+VT5GzIGMZvDPJO/b3oR0g3Kr9qqb4XJUR\n"
"qUH3+yD9NgHlLs95HDYrKC2xSQECIbZcbyVOBErUizJekv5yOy9DI43exGUrcM0H\n"
"8fkISGp0mFdkbGeJYpiQQbsB3gmrehvEr+hQD7RKTSqCgKdeRFp0ZEY3m3dFjf02\n"
"miAgtff90aKRyHYU/sC6W8J6GUjILCfkhAwPuUsA2ZjBYrswbgEkdhkPAgMBAAGj\n"
"ggFIMIIBRDAMBgNVHRMBAf8EAjAAMA4GA1UdDwEB/wQEAwIEkDCCAQMGA1UdIwSB\n"
"+zCB+IAUGSHWs49dtOLlTvtAC4KL1oQxOTGhgdSkgdEwgc4xCzAJBgNVBAYTAkNO\n"
"MRIwEAYDVQQIDAlHdWFuZ2RvbmcxETAPBgNVBAcMCFNoZW56aGVuMSowKAYDVQQK\n"
"DCFTaGVuemhlbiBQb3dlck9hayBOZXdlbmVyIENvLixsdGQxKjAoBgNVBAsMIVNo\n"
"ZW56aGVuIFBvd2VyT2FrIE5ld2VuZXIgQ28uLGx0ZDEbMBkGA1UEAwwSUG93ZXJP\n"
"YWsgJiBCTFVFVFRJMSMwIQYJKoZIhvcNAQkBFhR4dWxpYW5nQHBvd2Vyb2FrLm5l\n"
"dIIJANrtyJAoaK82MB0GA1UdDgQWBBTpBG+l5QG3j2GM8Hp6pfbEjnlTxTANBgkq\n"
"hkiG9w0BAQsFAAOB7AB8X4MRjKjCqo6tHxd3zdAd/tMuw0ggStV1tUGuVvOGvm+Z\n"
"L28p81WXHWtS2AGzhbiDRA3OcOEisDH3yxS97FwL27g+l5OMypVB2KVnBR0RSKgt\n"
"W9H1m7Foq8SlyN4edt4e9RXiI+RCSsgJpwKjaRD8TDYdy5Kotk3OLra7quFcraXw\n"
"CzX2qGlqgLG1eKKzFJS4Q6BFjTDSL0wHyULA7cPRmkhUYybalV1xegI3OhZlkl5Q\n"
"Y1lrvEdr0hbzKcQNon5IfauBAxdn4UTDT0Q6catTWH/cWPqdlkEHj30P3IUrd0/X\n"
"kScjiNR+QoVi\n"
"-----END CERTIFICATE-----";

char mqtts_iot_key[] = 
"-----BEGIN PRIVATE KEY-----\n"
"MIIEVwIBADANBgkqhkiG9w0BAQEFAASCBEEwggQ9AgEAAoHsAK14FKjnzuyV6ZwB\n"
"LFp1RpIRUAJ7HBR8DknBRitpgrejIw8+qCb5CY0m+SCEYJA3kvS82BxpKxphOnUE\n"
"RV5ZYV5ei+5+5LKbb5VPkbMgYxm8M8k79vehHSDcqv2qpvhclRGpQff7IP02AeUu\n"
"z3kcNisoLbFJAQIhtlxvJU4EStSLMl6S/nI7L0Mjjd7EZStwzQfx+QhIanSYV2Rs\n"
"Z4limJBBuwHeCat6G8Sv6FAPtEpNKoKAp15EWnRkRjebd0WN/TaaICC19/3RopHI\n"
"dhT+wLpbwnoZSMgsJ+SEDA+5SwDZmMFiuzBuASR2GQ8CAwEAAQKB63wkRHzAMCjF\n"
"/EHl4Qz3gsKD20N7QRQz17HWvFXTE3ZCLJP7XjFN5hT7ACzSL5zl+KhnAS4L4Ynm\n"
"bQRTQyWLR5BWj0Pl0ds0O72aDQYpNmKzekgYPtzmk0byRPh2iTmSYCgcfhZuxSQQ\n"
"43hVcKsWknl+Ln0CZsBIfNtQz8neJTLx34Y+9UoCMwk2TGDjCPVVJZuJocE+T2L7\n"
"lye/Nh2D3EUiJL64B/90nzIAYhkR4SO9W1e+DkNM04BrxH+dCZfVDASDGEPIqXFG\n"
"/xJnDqo5ER0YwkSnYuwqBDeM5vBEe5CWFPIQ5JRRX7B1UckCdg83M3i71Qk77q3D\n"
"4hDnTXpFs8AzcLRMXoe3HY6bhrqXzM04zlJLCpjRKcyPkPS+VRNVDBnCIk4ypuqX\n"
"W3c9kAd/SccrQpu7E6XbzeByNBCM0jfGITTF61XoYDxL+1JZimwy9BnzhuGBqRR/\n"
"stsEbSvEvlyBO/MCdgtmlVFZRG5uDUt61bWOVAC+fsjCPpug0RM/tYHS9E37+w4C\n"
"5A8Wijv2vAsM1TVGwBCk97PLnsw8WmXp4AY0nDmUCitDskn6KBzUZJcWW8doSbpR\n"
"b2tw5GXHsvYJMBKT/2GNWuLSEmLPrhQZuMqoThDxT3pfQXUCdgC2w1h/37AOOtW2\n"
"Yi4O3EjHanazMy+SgcD2a5GcZIbuxnI+nxenY7jl34M0nUWOhKlijS3MORXYNJK/\n"
"RVBj+BUR+OE6e2aZMYPPu2ozkZPRN6/cQQSb4L3oKFPL8N/dSaxn+6IYFhUQo2FN\n"
"2JOEMqciFRh8C9cCdgS3kRhzgujeCAou7LWOChu0teiC1jy2MUtxlLv+eK67j4ig\n"
"kZPck51z3SIFUUIV3+oDF6oAmdePMSfxuItYwTrhkcOM4vvQ0T/8cRJCYf8Dm13H\n"
"y/38W8Pw+I7sBkPpkETgo7YtjQXtP+15JwhS0toXvfa36vECdgGsA9qi3la0BifB\n"
"R8wd4YhLuh1EhH/Pyyx/IoZvTzZn39vHoL/Rbjn9kjsYm+33fwzQqenGHZ7XCEHH\n"
"MFEznj4TVv06zv2/xr8otyUKBw+8AitjJzbZRjo9blileb+Hwx8T/KFsU/kL6tG2\n"
"smOCQrzHE0zTqb4=\n"
"-----END PRIVATE KEY-----";

#elif PBOX2529000002238
/* PBOX2529000002238设备使用证书 */
char mqtts_iot_cert[] = 
"-----BEGIN CERTIFICATE-----\n"
"MIIFFjCCBBSgAwIBAgIICFdroYJA0AAwDQYJKoZIhvcNAQELBQAwgc4xCzAJBgNV\n"
"BAYTAkNOMRIwEAYDVQQIDAlHdWFuZ2RvbmcxETAPBgNVBAcMCFNoZW56aGVuMSow\n"
"KAYDVQQKDCFTaGVuemhlbiBQb3dlck9hayBOZXdlbmVyIENvLixsdGQxKjAoBgNV\n"
"BAsMIVNoZW56aGVuIFBvd2VyT2FrIE5ld2VuZXIgQ28uLGx0ZDEbMBkGA1UEAwwS\n"
"UG93ZXJPYWsgJiBCTFVFVFRJMSMwIQYJKoZIhvcNAQkBFhR4dWxpYW5nQHBvd2Vy\n"
"b2FrLm5ldDAeFw0yNTA3MTcwNzA3MTlaFw0yNjA3MTcwNzA3MTlaMIGoMRowGAYD\n"
"VQQDExFQQk9YMjUyOTAwMDAwMjIzODEqMCgGA1UECxMhU2hlbnpoZW4gUG93ZXJP\n"
"YWsgTmV3ZW5lciBDby4sbHRkMSowKAYDVQQKEyFTaGVuemhlbiBQb3dlck9hayBO\n"
"ZXdlbmVyIENvLixsdGQxETAPBgNVBAcTCFNoZW56aGVuMRIwEAYDVQQIEwlHdWFu\n"
"Z2RvbmcxCzAJBgNVBAYTAkNOMIIBCjANBgkqhkiG9w0BAQEFAAOB+AAwgfQCgewA\n"
"0X6WS5oRot6zGhJLkssAoeT2p7hCpEFTOs+2nBmCNceoqtoEPeFhg6utn2Ibflx4\n"
"VUAYDCrBnq+vJ0EnlohV0evP7F0YHtPwax3ibU5WFAEYTIey2tLXQhA7v5O2JTJp\n"
"MlcILeYOB17vPehOri25Bvd5h2/0Makyc4x5A+OdX+VeJ24z5pbXTVRvVbKJcJ9L\n"
"yXc8KW6bne0PfrKf9YGQ5fl4BBGp+v9Om4Jtm17szbG0AbX2WeaIFo2R9+9J5SMC\n"
"f05DriQ+vFZw8ioKSg5O3ZmgxU2tDz8C8T65OnquwG1fY73+8Vb+kvHd+wIDAQAB\n"
"o4IBSDCCAUQwDAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8EBAMCBJAwggEDBgNVHSME\n"
"gfswgfiAFBkh1rOPXbTi5U77QAuCi9aEMTkxoYHUpIHRMIHOMQswCQYDVQQGEwJD\n"
"TjESMBAGA1UECAwJR3Vhbmdkb25nMREwDwYDVQQHDAhTaGVuemhlbjEqMCgGA1UE\n"
"CgwhU2hlbnpoZW4gUG93ZXJPYWsgTmV3ZW5lciBDby4sbHRkMSowKAYDVQQLDCFT\n"
"aGVuemhlbiBQb3dlck9hayBOZXdlbmVyIENvLixsdGQxGzAZBgNVBAMMElBvd2Vy\n"
"T2FrICYgQkxVRVRUSTEjMCEGCSqGSIb3DQEJARYUeHVsaWFuZ0Bwb3dlcm9hay5u\n"
"ZXSCCQDa7ciQKGivNjAdBgNVHQ4EFgQUeeubm2FtoU0V2ETq8KAMDhrF2R4wDQYJ\n"
"KoZIhvcNAQELBQADgewAocINfXWoBN+nI9b48fpOXbFAjkfZ6WU1mDtQQNA0Dhv+\n"
"W2ePc19W/FjsA33GoFto4eokmQtidNVz59yq/u4ZlMbUl5bVEqkpgrcu1cORqSPj\n"
"DlttCS+aqC9fbS9Ehs4wf/j/8ZUw1BEGdI/gwmmE0kGDZ5/GdSlZa3pTvv/4pGek\n"
"N98QZY193UrXDdkJCQJQS0pXO94etyxJ4t4eJzjZwJcXnM3+F3GT0LPKr0DaYx/K\n"
"vf8gxJle9W+oW14cjgHD8WdYIuby16hFmNF26tmAx7ju57npqe3eyJi7f2Ze21ZC\n"
"UIGWuiL4a4KbGA==\n"
"-----END CERTIFICATE-----";

char mqtts_iot_key[] = 
"-----BEGIN PRIVATE KEY-----\n"
"MIIEVwIBADANBgkqhkiG9w0BAQEFAASCBEEwggQ9AgEAAoHsANF+lkuaEaLesxoS\n"
"S5LLAKHk9qe4QqRBUzrPtpwZgjXHqKraBD3hYYOrrZ9iG35ceFVAGAwqwZ6vrydB\n"
"J5aIVdHrz+xdGB7T8Gsd4m1OVhQBGEyHstrS10IQO7+TtiUyaTJXCC3mDgde7z3o\n"
"Tq4tuQb3eYdv9DGpMnOMeQPjnV/lXiduM+aW101Ub1WyiXCfS8l3PClum53tD36y\n"
"n/WBkOX5eAQRqfr/TpuCbZte7M2xtAG19lnmiBaNkffvSeUjAn9OQ64kPrxWcPIq\n"
"CkoOTt2ZoMVNrQ8/AvE+uTp6rsBtX2O9/vFW/pLx3fsCAwEAAQKB6wC6lANHUfBj\n"
"HfRlQ+YK1sG5bjI/LdwsbgGyX0xDunQ9cZTGlWUxt4khu8TmLnpOEA0b3/mK3ImU\n"
"yuM0EtMvPj0wuNR1rnPNAPUecX832ozPRCpf/ntMaHtOWybbeLLAbiVCsECYoEve\n"
"RK7OdBefCRnBGqch+JNpzzzRsBcm7GjKLPzyUPC0yR1nYbIDYP1m1TsxRNvOjgAQ\n"
"gy+oavKAw0PcKZAqVav+opMLRNPjpyTevciKgOww0QGiAPTfWHTy/0OYekNnN61U\n"
"Y6xKGl85FHMncCCuVKQURFWtnJ8IPuFUL5TwDBaSByWL/qECdg+D3mieL483aQxs\n"
"xAJwrBwPpkwEiTz82IrQqVXSRnarJnel+IIq/DZ9wLB/Gl/6d3n5WKEsd6eFfI2O\n"
"5Aep9YLlm7XkCfiwjUw2Fcrn1AjZFysQeCW6gnxMjnrOmah9nxglrekh4Gz325xM\n"
"lyBmfzEYQ2qcjR0Cdg2Aqup0AKcu8UeWC2kj1T1cucZeYkjoNsITibiyYEeh/W/O\n"
"qwF3PajiY7hd0guA7IUrf1IXOkQm3/GrD6jh8FdL7h5nvydTs0iyTJm+LGk+tqwl\n"
"aFEtqj6nK+mn54pVTqC/Cvzx2BRluurKbS1NXV+NpS3J4/cCdgUby4CxDUHf4dP4\n"
"gDW5ecInmP8Qw0t0iaxzdj5O5WqvghQFbyUVGESs9WRoBXwy0WI1Kuyu4psjRdaq\n"
"hQTM/oziI3opouri3zOH33cXDGb5bDp+ysDJf6uD3aosGsYyzs65oWDRCb93gTUe\n"
"tugxN5InxhL1hzUCdgfmMhce7hLgP38lpF9J+0H+sR//r3f/galgTL2kfPbrKZzz\n"
"Rs+Atq3KY5RELtFUBwqRO9cGh6u/Ilv7OxoNs9EeNpvLDeUv7j6lMHLw8oJPkgZ7\n"
"i9+R23rzyZjUBXzEE0+u8/qucT7dlNAuxFgwXmO8NdnntlkCdg4q0WcuI523q/r4\n"
"+eTUI/y2TZbTxKNN643OM6zizinUx0LtH5jAlq6Udu2YcGg89ldBDvq/S4qLhYL5\n"
"nTxWHiQsZsMLExssk6HrynVsAvAM8tqcF56/WuFeu3LfJyz/cewnS5HFwySZhMk/\n"
"DWnoHSj6NjYLNBc=\n"
"-----END PRIVATE KEY-----";
#endif

#ifdef     TCP2_ENCRYPT_ENABLE	
//手动写入CA证书数组 
char power_my_ca_cert_text_2[] = 
"-----BEGIN CERTIFICATE-----\n"
"MIIEfTCCA2WgAwIBAgIUP7tASPA4H84+1UsvSTwBf/IU5uowDQYJKoZIhvcNAQEL\n"
"BQAwgcwxCzAJBgNVBAYTAkNOMRIwEAYDVQQIDAlHdWFuZ2RvbmcxETAPBgNVBAcM\n"
"CFNoZW56aGVuMSowKAYDVQQKDCFTaGVuemhlbiBQb3dlck9hayBOZXdlbmVyIENv\n"
"LixsdGQxKjAoBgNVBAsMIVNoZW56aGVuIFBvd2VyT2FrIE5ld2VuZXIgQ28uLGx0\n"
"ZDEbMBkGA1UEAwwSUG93ZXJPYWsgJiBCTFVFVFRJMSEwHwYJKoZIhvcNAQkBFhJ3\n"
"YW5neEBwb3dlcm9hay5uZXQwIBcNMjQwNDI1MDYzNTU0WhgPMjEyNDA0MDEwNjM1\n"
"NTRaMIHMMQswCQYDVQQGEwJDTjESMBAGA1UECAwJR3Vhbmdkb25nMREwDwYDVQQH\n"
"DAhTaGVuemhlbjEqMCgGA1UECgwhU2hlbnpoZW4gUG93ZXJPYWsgTmV3ZW5lciBD\n"
"by4sbHRkMSowKAYDVQQLDCFTaGVuemhlbiBQb3dlck9hayBOZXdlbmVyIENvLixs\n"
"dGQxGzAZBgNVBAMMElBvd2VyT2FrICYgQkxVRVRUSTEhMB8GCSqGSIb3DQEJARYS\n"
"d2FuZ3hAcG93ZXJvYWsubmV0MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC\n"
"AQEAyQUXwmQj63p6s3mpgoak85OOJLa48BlswqImW4KpUiK3uGTkh1KSf7qDANyc\n"
"R9N7j4QdmpxCffAN+QGuDdLl0DOtHEICD+prK0XQVBMAt3Q/Zk7wFQ8UKwknyF/D\n"
"t5rQfKcf++N4zDIBIS/xqwUw6t4tSuo6kgdBFypZXfDKO2emeIHey/rRfL8HeqkU\n"
"53NONjXnr6fRKG8MVzMANN5JD2MS9do1ijro8tJAqMWGdiFD6bEWX9Z8NdTAmTOp\n"
"CtPhSo/UeUjEEVnkVtmZrxA/ccnte7BnZ7F1ev693EeDAhfXv65HzDrkBdjWSKd4\n"
"uYylRJjmK9DXAJCPWjdyg2UbkQIDAQABo1MwUTAdBgNVHQ4EFgQUekX5e/ymWjlf\n"
"gbM6XsDH2M1opCYwHwYDVR0jBBgwFoAUekX5e/ymWjlfgbM6XsDH2M1opCYwDwYD\n"
"VR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAlHin+XGGibSIjCtnjjTH\n"
"+ETRq+RLrArUWT0PQ5f00h3mCwpNixdBzCpChDPUklQod/3gbjzZIVj05a4N8wuU\n"
"aczjYZx0b2K3U4yy5PbPKu/cyg9YGrLPB3rxbJfppPKt3DhX1VjfYdNK6Hdv94SV\n"
"JW5Wq+nakiytGLL3PFLLYZK8JLqjJU4Q9pQhm0RTgSqimW/Ll9wRA6M6AiTw4NVc\n"
"vXEWzgsVshvYRn+GoLjzSKWfzg4eXO/xpjPVDlEroFUEZtOMbpe4vOZ9WOplFfcT\n"
"BFxLkNcDwQUvcHYuEpfFXA3X1cEyKKNfvzl3hkDTAFq8pLmV1wH0QE8gkdhSul1c\n"
"OQ==\n"
"-----END CERTIFICATE-----";

//手动写入设备证书数组 
char power_mqtt_cert_2[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIEITCCAwkCFHlR4tpsxD1TKe1JAt7V78VxxC32MA0GCSqGSIb3DQEBCwUAMIHM\n"
"MQswCQYDVQQGEwJDTjESMBAGA1UECAwJR3Vhbmdkb25nMREwDwYDVQQHDAhTaGVu\n"
"emhlbjEqMCgGA1UECgwhU2hlbnpoZW4gUG93ZXJPYWsgTmV3ZW5lciBDby4sbHRk\n"
"MSowKAYDVQQLDCFTaGVuemhlbiBQb3dlck9hayBOZXdlbmVyIENvLixsdGQxGzAZ\n"
"BgNVBAMMElBvd2VyT2FrICYgQkxVRVRUSTEhMB8GCSqGSIb3DQEJARYSd2FuZ3hA\n"
"cG93ZXJvYWsubmV0MCAXDTI0MDQyNTA2NDQ0MloYDzIxMjQwNDAxMDY0NDQyWjCB\n"
"yjELMAkGA1UEBhMCQ04xEjAQBgNVBAgMCUd1YW5nZG9uZzERMA8GA1UEBwwIU2hl\n"
"bnpoZW4xKjAoBgNVBAoMIVNoZW56aGVuIFBvd2VyT2FrIE5ld2VuZXIgQ28uLGx0\n"
"ZDEqMCgGA1UECwwhU2hlbnpoZW4gUG93ZXJPYWsgTmV3ZW5lciBDby4sbHRkMRkw\n"
"FwYDVQQDDBBJT1QyMjM2MDAyMzYxMTQyMSEwHwYJKoZIhvcNAQkBFhJ3YW5neEBw\n"
"b3dlcm9hay5uZXQwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDPXXk5\n"
"RH+O5iBQALOb7NmGz0vQX4ON5siiFwOebZL1Y/+2X4YPdhbjiHoHUklxi/es7NG4\n"
"O7GpReQDaSm+n5H6o3CYZJQa02y+kJ4QqQs8EdadP4xIo7d3j570nDTeM9NkiWpZ\n"
"KjGCP40yh2DY+WYVU/3TiW5V/64ocum7YyhhsZBf4hXFRNRomC3/plG7+YRII4OT\n"
"njj5QydNx0y5jYcCD1pmGojA9kxZyogGTzgkEJk9WhMJxxTJCDc5+dTY8QJLxIIb\n"
"1waJ8aaX1nNCyhyk7WcpvMmFH076C7gZ1paUj3IAD8YFxAOl0i49aFkRERsr6NSV\n"
"fQZO8LyqIZOhLVqTAgMBAAEwDQYJKoZIhvcNAQELBQADggEBALIsUBWmmU6OsZXF\n"
"+EtQvUuklgUsY+zNQWZS7IxggILIRKFmHyjwQVkLPKU5JF2UEOjx07wJ3JQB4u6q\n"
"3X2iaVRgB00goOWVRdVMnfcQWgF+VX4pcUSHQHlHxMAyaaY2MGb5ijCx0NQyRFna\n"
"IyZ1SfhOvJ+8eMHi2u5hlvJSNZFg51+Xqoka04+FooIciD+zscnhFILolaYDhJIK\n"
"Xy7o354QMuJKIzA2d5M7tq+Jl2hJ5PnsApN1Zi5OGoFY2CNQrAVoSE0p5yJuvY8/\n"
"LlnLYGIwALL57RfOYmHh1rAKsghg4YuZM9IVQJ9YgUUC0H2uG/Gqvc970ILjeaHR\n"
"n3UgA9M=\n"
"-----END CERTIFICATE-----";

//手动写入密钥数组 
//PKCS#8私钥
char power_private_key_2[] = 
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDPXXk5RH+O5iBQ\n"
"ALOb7NmGz0vQX4ON5siiFwOebZL1Y/+2X4YPdhbjiHoHUklxi/es7NG4O7GpReQD\n"
"aSm+n5H6o3CYZJQa02y+kJ4QqQs8EdadP4xIo7d3j570nDTeM9NkiWpZKjGCP40y\n"
"h2DY+WYVU/3TiW5V/64ocum7YyhhsZBf4hXFRNRomC3/plG7+YRII4OTnjj5QydN\n"
"x0y5jYcCD1pmGojA9kxZyogGTzgkEJk9WhMJxxTJCDc5+dTY8QJLxIIb1waJ8aaX\n"
"1nNCyhyk7WcpvMmFH076C7gZ1paUj3IAD8YFxAOl0i49aFkRERsr6NSVfQZO8Lyq\n"
"IZOhLVqTAgMBAAECggEADolx/xwYiSRZguJEX8ZJ4D6Ar8SSYgyaEhWPm7f4w8Uq\n"
"iawb/NIoJp6hrLO4ZwJgmY8nNfAms0QtXjktblScv40HpAH2Xq8QV4enMmnMr32x\n"
"+udtO9u7oou/S/dLaYGz7GoJujh6rLPXKoH9ExjgxCwAMP0tw/pJnkJNX7qYJnOo\n"
"0o/rwly+gPMNjaMQMp95yCvVY0ADdj68CHvo34O7wgXg8iH8KF9zTTV1hX04rBpv\n"
"q+sz5uxmiKCLxoyR3BBUBUZ93J/A+NwR7tRdVygsYbAFkZbR7VoZCmyew7R7jy2D\n"
"VqUN/9HEEuyZqOEiEieHiJuZoxxjpjmuI9eEKkvtGQKBgQD4miQz1yp8TEVHSuTg\n"
"OUW8v1AOEERAiVt8r5WivCXqG1biw1om1OxV/E+wAM4MaKEI/teh98+HbSNcWULw\n"
"gsGf+SsrR2B44aV8uVApRWaFsBzLjk8tBn55RZ7lQR09MVxadcZKpP/tsvE92Z2Z\n"
"vfLDXhIopiP0yA/be/afwf591QKBgQDViS//2t6n9QJ30gMJN/rzEnWTUa6NWIwH\n"
"AmucwqjnCsYL3v1TLFgQXl28RoII6Dqyaxyc0pO2T+2Za0qahulpoCcC0KiJ8haN\n"
"mLVYVjKshyvFeaPDAbgo8PAvKGja4g2jKvIFJTtolNcP/nKGh3ZvuB5aImMACI8/\n"
"MH10yHZixwKBgQCSav6dhcGhGa7H8YVhxnmbVDZLQitgs0Wt5yBDi3rtKhL/Vb1Y\n"
"F5nmfsdUSvUQe+M0kBviajjRc2cbMftp+ikeFjvlrpFUQNod3msnE5fxbytWfEeH\n"
"pkaBGI8gSTx2WQcixDD4r7uIfRb2rZ/T9/ruNA6P4GJW/inQxG8Z4sEHzQKBgG8j\n"
"jgX+prCIRxaSCd58cnKdAvEALHeIxKjJS7U1Y6+M3fNDxlnJ1LQASY06rWdxZ0uB\n"
"kEzXFOZox2N4gkXXPhkpr+Q5Md9KTw63kns9sfY2DEQlwWQ3uuAkNv50a86wpSRt\n"
"r7WZ4UfXX/AFIRp/2tbe331ONHTJ/7SFymZEB8n1AoGAVKPsq7gIQ9St7XZJ1Uv9\n"
"0OCzSoadmP0NJyaTdQ2qMr2aFUrafHGKNrj/jmGUc36Gej6HL5q5/38f4Lg5qtRv\n"
"xICXbqigeJo1F4vMWYcC/Unnl7EPDQzXTIAH32W8Yx1Rz31LrtE2PfK+oHU7+2Ie\n"
"3Sn2O4ul4PeFUsULTAF1esY=\n"
"-----END PRIVATE KEY-----";

#else

#endif	

void partition_write_encrypt(uint8_t *data, uint32_t len, uint32_t offset, uint8_t area);
uint32_t partition_read_decrypt(uint8_t *data, uint32_t len, uint32_t offset, uint8_t area);
void partition_init(uint8_t area);

uint32_t get_partition_plaintext_len(uint8_t area);
uint32_t get_partition_ciphertext_len(uint8_t area);
void partiton_data_init(PARTITION_DATA_UNION *data, uint8_t type);
void test_partition(void);
void update_factory_cert_md5(void);


//16进制打印
static void dump_partition(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}

//ascii打印
static void dump_partition_ascii(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s%c%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}

//获取分区类型和名称
void get_partitiontype_string(uint8_t area, partition_type_t *data)
{
    memset(data->name, 0, sizeof(data->name));
    switch (area)
    {
    case CUSTOM_DATA_AREA://OTA
        data->type = 0x3A;
        memcpy(data->name, "custom_data", strlen("custom_data"));
        break;    	
    case CA_CERTIFICATE_AREA:
        data->type = 0x40;
        memcpy(data->name, "ca", strlen("ca"));
        break;

    case IOT_CERTIFICATE_AREA:
        data->type = 0x41;
        memcpy(data->name, "device", strlen("device"));
        break;

    case PRIVATE_AREA:
        data->type = 0x42;
        memcpy(data->name, "private", strlen("private"));
        break;

    case UPDATE_AREA:
        data->type = 0x43;
        memcpy(data->name, "update", strlen("update"));
        break;


#ifdef     TCP2_ENCRYPT_ENABLE	
    case CA_CERTIFICATE_AREA_2:
        data->type = 0x44;
        memcpy(data->name, "ca_2", strlen("ca_2"));
        break;

    case IOT_CERTIFICATE_AREA_2:
        data->type = 0x45;
        memcpy(data->name, "device_2", strlen("device_2"));
        break;

    case PRIVATE_AREA_2:
        data->type = 0x46;
        memcpy(data->name, "private_2", strlen("private_2"));
        break;

    case UPDATE_AREA_2:
        data->type = 0x47;
        memcpy(data->name, "update_2", strlen("update_2"));
        break;
#else

#endif	
    default:
        break;
    }
}

//分区信息体格式化
void partiton_data_init(PARTITION_DATA_UNION *data, uint8_t type)
{
    data->PartitionDataStruct.start_flag[0] = PARTITION_FLAG; //标志
    data->PartitionDataStruct.start_flag[1] = PARTITION_FLAG;
    data->PartitionDataStruct.ciphertext_len = 0; //密文长度
    data->PartitionDataStruct.plaintext_len = 0;  //明文长度
    data->PartitionDataStruct.type = type;  //类型
}

//获取分区密文长度
uint32_t get_partition_ciphertext_len(uint8_t area)
{
    partition_type_t partition_type;
    get_partitiontype_string(area, &partition_type);
    PARTITION_DATA_UNION partition_data;
    const esp_partition_t *partition = esp_partition_find_first(partition_type.type, 0, partition_type.name); //获取分区表
    assert(partition != NULL);
    esp_partition_read_raw(partition,0, partition_data.data, 16); //读取分区表数据
    return partition_data.PartitionDataStruct.ciphertext_len;
}

//获取分区明文长度
uint32_t get_partition_plaintext_len(uint8_t area)
{
    partition_type_t partition_type;
    get_partitiontype_string(area, &partition_type);
    PARTITION_DATA_UNION partition_data;
    const esp_partition_t *partition = esp_partition_find_first(partition_type.type, 0, partition_type.name); //获取分区表
    assert(partition != NULL);
    esp_partition_read_raw(partition,0, partition_data.data, 16); //读取分区表数据
    //检查分区标志
    if(partition_data.PartitionDataStruct.start_flag[0] != PARTITION_FLAG || partition_data.PartitionDataStruct.start_flag[1] != PARTITION_FLAG)
    {
        ESP_LOGI(TAG, "invalid flag");
        partition_data.PartitionDataStruct.plaintext_len = 0;
    }
    return partition_data.PartitionDataStruct.plaintext_len;
}

//初始化分区
void partition_init(uint8_t area)
{
    PARTITION_DATA_UNION partition_data;
    partition_type_t partition_type;
    get_partitiontype_string(area, &partition_type);
    const esp_partition_t *partition = esp_partition_find_first(partition_type.type, 0, partition_type.name); //获取分区表
    assert(partition != NULL);
    memset(partition_data.data, 0, sizeof(partition_data.data));
    //dump_partition("partition info", partition_data.data, sizeof(partition_data.data));
    esp_partition_read_raw(partition, 0, partition_data.data, sizeof(partition_data.data)); //读取分区表数据
    //dump_partition("partition info", partition_data.data, sizeof(partition_data.data));
    if(partition_data.PartitionDataStruct.start_flag[0] != PARTITION_FLAG || partition_data.PartitionDataStruct.start_flag[1] != PARTITION_FLAG)
    {
        //格式化分区
        ESP_LOGI (TAG, "init partition,type:0x%02x", partition_type.type);
        ESP_LOGI (TAG, "erase range:%d", PARTITION_SIZE);
        memset(partition_data.data, 0, sizeof(partition_data.data));
        partiton_data_init(&partition_data, partition_type.type);
        esp_partition_erase_range(partition, 0, PARTITION_SIZE); //擦除分区
        esp_partition_write_raw(partition, 0, partition_data.data, sizeof(partition_data.data)); //写入数据
        //dump_partition("write partition info", partition_data.data, sizeof(partition_data.data));
    }
    else
    {
        ESP_LOGI (TAG, "partition exist!");
 //       ESP_LOGI (TAG, "info:type:0x%02x,plaintext_len:%lu,ciphertext_len:%lu", partition_data.PartitionDataStruct.type, partition_data.PartitionDataStruct.plaintext_len,partition_data.PartitionDataStruct.ciphertext_len);
    }
}

//分区重初始化
void partition_reinit(uint8_t area, uint32_t erase_size)
{
    PARTITION_DATA_UNION partition_data;
    partition_type_t partition_type;
    get_partitiontype_string(area, &partition_type);
    const esp_partition_t *partition = esp_partition_find_first(partition_type.type, 0, partition_type.name); //获取分区表
    assert(partition != NULL);
    memset(partition_data.data, 0, sizeof(partition_data.data));
    partiton_data_init(&partition_data, partition_type.type);
    esp_partition_erase_range(partition, 0, erase_size); //擦除分区
    esp_partition_write_raw(partition, 0, partition_data.data, sizeof(partition_data.data)); //写入数据
    esp_partition_read_raw(partition, 0, partition_data.data, sizeof(partition_data.data)); //读取分区表数据
  //  ESP_LOGI (TAG, "reinit partition success!");
 //   ESP_LOGI (TAG, "info:type:0x%02x,plaintext_len:%lu,ciphertext_len:%lu", partition_data.PartitionDataStruct.type, partition_data.PartitionDataStruct.plaintext_len,partition_data.PartitionDataStruct.ciphertext_len);
}

//读取并解密分区明文数据数据
uint32_t partition_read_decrypt(uint8_t *data, uint32_t len, uint32_t offset, uint8_t area)
{
    uint8_t *raw_buffer = NULL;
    uint16_t raw_buffer_len = 0;
    uint8_t *aes_buffer = NULL;
    uint32_t start_offset = 0;  //AES密文单元起始地址
    uint32_t end_offset = 0;  //AES密文单元结束地址
    uint32_t differ_offset = 0;  
    uint32_t current_offset = offset; //偏移了16字节
    uint32_t current_len = len; //偏移了16字节
    if(len == 0)
    {
        return 0;
    }
    partition_type_t partition_type;
    get_partitiontype_string(area, &partition_type);
    PARTITION_DATA_UNION partition_data;
    const esp_partition_t *partition = esp_partition_find_first(partition_type.type, 0, partition_type.name); //获取分区表
    assert(partition != NULL);
    memset(partition_data.data, 0, sizeof(partition_data.data));
    esp_partition_read_raw(partition,0, partition_data.data, 16); //读取分区表信息头
    //ESP_LOGI(TAG, "read_decrypt, plaintext len:%d, ciphertext_len:%d", partition_data.PartitionDataStruct.plaintext_len, partition_data.PartitionDataStruct.ciphertext_len);
    //ESP_LOGI(TAG, "current_offset:%d",current_offset);
    //ESP_LOGI(TAG, "current_len:%d",current_len);
    if((current_offset+current_len) <= partition_data.PartitionDataStruct.ciphertext_len)
    {
        //注意：读取要按照AES密文(16字节)单元读取，首先要找到offset所在的密文单元，再偏移长度后的密文单元
        start_offset = (current_offset%16 != 0)?(current_offset-current_offset%16):current_offset;
        if(current_len >= start_offset)
        {
            end_offset = ((current_len-start_offset)%16 != 0)?(current_len-(current_len-start_offset)%16+16):(current_len-start_offset);
        }
        else
        {
            end_offset = ((start_offset+current_len)%16 !=0)?(start_offset+current_len-(start_offset+current_len)%16+16):(start_offset+current_len);
        }
        
        differ_offset = end_offset-start_offset;

        //ESP_LOGI(TAG, "start offset:%x",start_offset);
        //ESP_LOGI(TAG, "end offset:%d",end_offset);
        //ESP_LOGI(TAG, "differ_offset:%x",differ_offset);

        raw_buffer = (uint8_t*)heap_caps_malloc(differ_offset*(sizeof(uint8_t)), MALLOC_CAP_SPIRAM);
        if(raw_buffer == NULL)
        {
            ESP_LOGE(TAG, "malloc1 fail!,len:%lu", differ_offset);
            return 0;
        }
        aes_buffer = (uint8_t*)heap_caps_malloc(differ_offset*(sizeof(uint8_t)), MALLOC_CAP_SPIRAM);
        if(aes_buffer == NULL)
        {
            ESP_LOGE(TAG, "malloc2 fail!");
            if(raw_buffer != NULL)
            free(raw_buffer);
            return 0;
        }
        esp_partition_read_raw(partition, start_offset+16, aes_buffer, differ_offset); //读取AES密文
        //dump_partition("get encry data", aes_buffer, differ_offset); //打印分区数据
        iot_file_decrypt(aes_buffer, differ_offset, raw_buffer, &raw_buffer_len); //AES解密
        //dump_partition("out decrypt data", raw_buffer, raw_buffer_len); //打印分区数据

        memcpy(data, raw_buffer+offset%16, len);
        //dump_partition("copy data", data, len); //打印分区数据

        if(raw_buffer != NULL)
            free(raw_buffer);
        if(aes_buffer != NULL)
            free(aes_buffer);
        return len;
    }
    else
    {
        return 0;
    }
}

//写数据到分区，并且AES加密
void partition_write_encrypt(uint8_t *data, uint32_t len, uint32_t offset, uint8_t area)
{
    uint32_t erase_size = PARTITION_SIZE;
    uint32_t read_len = 0;
    uint8_t *aes_buffer = NULL;
    uint16_t aes_buffer_len = 0;
    uint8_t *buffer = NULL;
    uint32_t buffer_len = 0;
    partition_type_t partition_type;
    get_partitiontype_string(area, &partition_type);
    PARTITION_DATA_UNION partition_data;
    const esp_partition_t *partition = esp_partition_find_first(partition_type.type, 0, partition_type.name); //获取分区表
    ESP_LOGW(TAG,"partition_type.type:%d,partition_type.name:%s",partition_type.type,partition_type.name);
    assert(partition != NULL);
    memset(partition_data.data, 0, sizeof(partition_data.data));
    esp_partition_read_raw(partition,0, partition_data.data, 16); //读取分区表数据
    
    if(partition_data.PartitionDataStruct.plaintext_len > 0)
    {
        //确定申请空间大小，包括原先数据和待添加数据
        if(offset <= partition_data.PartitionDataStruct.plaintext_len) //小于明文长度
        {
            buffer_len = (offset+len > partition_data.PartitionDataStruct.plaintext_len)?(offset+len):partition_data.PartitionDataStruct.plaintext_len;         
        }
        else
        {
            buffer_len = offset+len-partition_data.PartitionDataStruct.plaintext_len;
        }
    }
    else //没有明文数据
    {
        ESP_LOGI(TAG, "no plain data");
        buffer_len = len;
    }
    
    //ESP_LOGE(TAG, "11buffer malloc buffer_len:%d", buffer_len);
    buffer = (uint8_t*)heap_caps_malloc(buffer_len*sizeof(uint8_t), MALLOC_CAP_SPIRAM); //申请空间
    if(buffer == NULL)
    {
        ESP_LOGE(TAG, "11buffer malloc fail");
        return;
    }
    memset(buffer, 0, buffer_len); //清零
    read_len = partition_read_decrypt(buffer, partition_data.PartitionDataStruct.plaintext_len, 0, area); //将分区的数据全部读出解密
    ESP_LOGI(TAG, "original data len:%lu", read_len);
    if(partition_data.PartitionDataStruct.plaintext_len > 0)
    {
        //dump_partition("original data", buffer, partition_data.PartitionDataStruct.plaintext_len); //打印分区数据
    }
    
    ESP_LOGI(TAG, "cpoy new data, offset:%lu, len:%lu", offset, len);
    memcpy(buffer+offset, data, len);
    ESP_LOGI(TAG, "sum len:%lu", buffer_len);
    // dump_partition("original data+add data", buffer, buffer_len); //打印分区数据
    // if(area == UPDATE_AREA)
    // {
    //     ESP_LOGI(TAG, "UPDATE_AREA");
    //     erase_size += PARTITION_SIZE;
    // }
    //ESP_LOGI(TAG, "erase type:0x%02x,range:%d", partition_data.PartitionDataStruct.type,erase_size);
    esp_partition_erase_range(partition, 0, erase_size); //擦除4096字节
    aes_buffer_len = (buffer_len%16 != 0)?(buffer_len-buffer_len%16+16):(buffer_len);
    //ESP_LOGI(TAG, "aes_buffer_len:%d",aes_buffer_len);
    aes_buffer = (uint8_t*)heap_caps_malloc(aes_buffer_len*sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    if(aes_buffer == NULL)
    {
        ESP_LOGE(TAG, "aes_buffer malloc fail");
        if(buffer != NULL)
        {
            free(buffer);
        }
        return;
    }
    //dump_partition("in data:", buffer, buffer_len); //打印分区数据
    iot_file_encrypt(buffer, buffer_len, aes_buffer, &aes_buffer_len); //AES加密
    //dump_partition("aes data:", aes_buffer, aes_buffer_len); //打印分区数据
    esp_partition_write_raw(partition, 16, aes_buffer, aes_buffer_len);
    partition_data.PartitionDataStruct.ciphertext_len = aes_buffer_len; //更新密文长度
    partition_data.PartitionDataStruct.plaintext_len = buffer_len; //更新明文长度
    esp_partition_write_raw(partition, 0, partition_data.data, sizeof(partition_data.data)); //最后写入分区信息
 //   ESP_LOGI (TAG, "save info:type:0x%02x,plaintext_len:%lu,ciphertext_len:%lu", partition_data.PartitionDataStruct.type, partition_data.PartitionDataStruct.plaintext_len, partition_data.PartitionDataStruct.ciphertext_len);
    
    if(buffer != NULL)
    {
        free(buffer);
    }
    if(aes_buffer != NULL)
    {
        free(aes_buffer);
    }
}

//打印分区信息(调试)
void print_partition_info(uint8_t area, uint8_t info_type)
{
    uint8_t *read_buf = NULL;
    partition_type_t partition_type;
    get_partitiontype_string(area, &partition_type);
    PARTITION_DATA_UNION partition_data;
    const esp_partition_t *partition = esp_partition_find_first(partition_type.type, 0, partition_type.name); //获取分区表
    assert(partition != NULL);
    memset(partition_data.data, 0, sizeof(partition_data.data));
    esp_partition_read_raw(partition,0, partition_data.data, 16); //读取分区表数据
    dump_partition("raw info:", partition_data.data, sizeof(partition_data.data)); //打印分区数据
    ESP_LOGI (TAG, "info:type:0x%02x,plaintext_len:%lu,ciphertext_len:%lu", partition_data.PartitionDataStruct.type, partition_data.PartitionDataStruct.plaintext_len, partition_data.PartitionDataStruct.ciphertext_len);
    if(info_type == RAW_INFO) //原始加密数据
    {
        read_buf = (uint8_t*)heap_caps_malloc((partition_data.PartitionDataStruct.ciphertext_len+16)*(sizeof(uint8_t)), MALLOC_CAP_SPIRAM);
        if(read_buf == NULL)
        {
            ESP_LOGE(TAG, "malloc fail!");
            return;
        }
        esp_partition_read_raw(partition, 0, read_buf, partition_data.PartitionDataStruct.ciphertext_len+16); //读取分区表数据
        dump_partition("raw data:", read_buf, partition_data.PartitionDataStruct.ciphertext_len+16); //打印分区数据
        
    }
    else if(info_type == NORMAL_INFO) //解密后数据
    {
        read_buf = (uint8_t*)heap_caps_malloc((partition_data.PartitionDataStruct.plaintext_len+16)*(sizeof(uint8_t)), MALLOC_CAP_SPIRAM);
        if(read_buf == NULL)
        {
            ESP_LOGE(TAG, "malloc fail!");
            return;
        }
        esp_partition_read_raw(partition, 0, read_buf, 16); //读取分区表数据
        partition_read_decrypt(read_buf+16, partition_data.PartitionDataStruct.plaintext_len, 0, area); //数据读取解密
        //dump_partition("normal data:", read_buf, partition_data.PartitionDataStruct.plaintext_len+16); //打印分区数据
        //dump_partition_ascii("ascii data:", read_buf+16, partition_data.PartitionDataStruct.plaintext_len);
        ESP_LOGI(TAG, "current_offset:%s",read_buf+16);
    }
    if(read_buf != NULL)
        free(read_buf);
}

//计算ota bin文件哈希值
void iot_sha256_bin(const mbedtls_md_info_t *md_info, uint32_t len, unsigned char *output) 
{
    int ret = -0x006E;
    uint8_t buffer[1024];
    uint32_t offset = 0;
    uint16_t count = 0;
    mbedtls_md_context_t ctx;
    esp_partition_t *partition = esp_ota_get_next_update_partition(NULL); // 获取当前OTA分区
    // if( md_info == NULL )
    // {
    //     return( MBEDTLS_ERR_MD_BAD_INPUT_DATA );
    // }
           
    mbedtls_md_init( &ctx );

    if( ( ret = mbedtls_md_setup( &ctx, md_info, 0 ) ) != 0 )
        goto cleanup;

    if( ( ret = mbedtls_md_starts( &ctx ) ) != 0 )
        goto cleanup;

    count = len/1024; //计算整数倍
    for(uint16_t i = 0; i < count; i++)
    {
        esp_partition_read_raw(partition,offset, buffer, 1024); //读取分区表数据
        //dump_partition("all", buffer, 1024);
        if(( ret = mbedtls_md_update( &ctx, buffer, 1024 ) ) != 0)
        {
            goto cleanup;
        }
        offset += 1024;
    }
    if(len%1024 != 0) //不足1024字节部分
    {
        esp_partition_read_raw(partition,offset, buffer, len%1024); //读取分区表数据
        //dump_partition("rest", buffer, len%1024);
        if(( ret = mbedtls_md_update( &ctx, buffer, len%1024 ) ) != 0)
        {
            goto cleanup;
        }
        offset += len%1024;
    }

    ret = mbedtls_md_finish( &ctx, output );
    

cleanup:
    mbedtls_platform_zeroize( buffer, sizeof( buffer ) );
    mbedtls_md_free( &ctx );
}

//计算ota bin文件哈希值
void other_device_sha256_bin(uint32_t address, const mbedtls_md_info_t *md_info, uint32_t len, unsigned char *output) 
{
    int ret = -0x006E;
    uint8_t buffer[1024];
    uint32_t offset = 0;
    uint16_t count = 0;
    mbedtls_md_context_t ctx;
           
    mbedtls_md_init( &ctx );

    if( ( ret = mbedtls_md_setup( &ctx, md_info, 0 ) ) != 0 )
        goto cleanup;

    if( ( ret = mbedtls_md_starts( &ctx ) ) != 0 )
        goto cleanup;

    count = len/1024; //计算整数倍
    offset = address;
    for(uint16_t i = 0; i < count; i++)
    {
        //ReadCode(offset, buffer, 1024); //读取数据
        //dump_partition("all", buffer, 1024);
        if(( ret = mbedtls_md_update( &ctx, buffer, 1024 ) ) != 0)
        {
            goto cleanup;
        }
        offset += 1024;
    }
    if(len%1024 != 0) //不足1024字节部分
    {
        //ReadCode(offset, buffer, len%1024);  //读取数据
        //dump_partition("rest", buffer, len%1024);
        if(( ret = mbedtls_md_update( &ctx, buffer, len%1024 ) ) != 0)
        {
            goto cleanup;
        }
        offset += len%1024;
    }

    ret = mbedtls_md_finish( &ctx, output );
    

cleanup:
    mbedtls_platform_zeroize( buffer, sizeof( buffer ) );
    mbedtls_md_free( &ctx );
}
//检查证书是否存在
uint8_t check_cer_empty(void)
{
    if(!get_partition_plaintext_len(CA_CERTIFICATE_AREA))
    {
        return CA_CERTIFICATE_AREA;
    }

    if(!get_partition_plaintext_len(IOT_CERTIFICATE_AREA))
    {
        return IOT_CERTIFICATE_AREA;
    }

    if(!get_partition_plaintext_len(PRIVATE_AREA))
    {
        return PRIVATE_AREA;
    }
    return 0;
}

//查找第一份证书的位置
uint8_t find_first_cert_position(uint8_t *data, uint32_t len, uint32_t *offset)
{
    uint8_t ret = 0;
    uint8_t flag = 0;
    //uint32_t offset = 0;
    char target_str[] = "-----BEGIN CERTIFICATE-----";
    *offset = 0;
    for(uint32_t i = 0; i < len; i++)
    {
         if(data[i] != target_str[0])
        {
            continue;
        }
        else
        {
            for(uint8_t k = 1; k < sizeof(target_str); k++)
            {
                if(data[i+k] != target_str[k])
                {
                    flag = 0;
                    break;
                }
                else
                {
                    if(flag++ == sizeof(target_str)-3)
                    {
                        //ESP_LOGI(TAG, "find target");
                        *offset = i;
                        ret = 1;
                        //ESP_LOGI(TAG, "offset:0x%x", i);
                        return ret;
                    }
                    //ESP_LOGI(TAG, "flag:%d", flag);
                    
                }
            }
        }
    }
    return ret;
}

//复制证书到区域
uint8_t copy_cert(uint8_t *data, uint32_t len,uint8_t area)
{
    uint8_t ret = 0;
    char *copy_str = NULL;
    copy_str = (char*)heap_caps_malloc((len+1)*sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    if(copy_str == NULL)
    {
        ESP_LOGE(TAG, "malloc fail");
        return ret;
    }
    memset(copy_str, 0, len+1);
    memcpy(copy_str, data, len);
    partition_reinit(area, PARTITION_SIZE); //重初始化分区
    partition_write_encrypt((uint8_t*)copy_str, len, 0, area); //写入设备证书
    free(copy_str);
    ret = 1;
    return ret;
}

//找到证书并且写入
void find_cert_and_write(char *start_format, char *end_format, char *data, uint32_t *offset, uint8_t area)
{
    uint32_t cur_offset = *offset;
    char *ptr_start = NULL;
    char *ptr_end = NULL;
    uint32_t str_len = 0;
    ptr_start = strstr(data, start_format);
    ptr_end = strstr(data, end_format);
    if(ptr_start != NULL && ptr_end != NULL)
    {
        str_len = ptr_end - ptr_start + strlen(end_format);
        ESP_LOGI(TAG, "find cert,start copy to area:0x%x,len:%lu....", area, str_len);
        if(!copy_cert((uint8_t*)ptr_start, str_len, area))//复制证书到区域
        {
            ESP_LOGE(TAG, "copy fail!");
        }    
        //dump_partition_ascii("ascii data:", (uint8_t*)ptr_start, str_len);
        *offset+=str_len;
    }
    else 
    {
        ESP_LOGI(TAG, "can not find cert!");
    }
}

//检查证书更新
void check_for_cert_update(void)
{
    char *cer_str = NULL;
    char *ptr_start = NULL;
    char *ptr_end = NULL;
    uint32_t str_len = 0;
    uint32_t offset;
    PARTITION_DATA_UNION data_save;
    uint32_t len = 0;

    ESP_LOGI(TAG, "check_for_cert_update");
    len = get_partition_plaintext_len(UPDATE_AREA); //获取长度
    ESP_LOGI(TAG, "UPDATE_AREA,len:%lu", len);
    if(len)
    {
        partition_type_t partition_type;
        get_partitiontype_string(UPDATE_AREA, &partition_type);
        //PARTITION_DATA_UNION partition_data;
        const esp_partition_t *partition = esp_partition_find_first(partition_type.type, 0, partition_type.name); //获取分区表
        esp_partition_read_raw(partition, 0x1ff0, data_save.data, sizeof(data_save.data)); //读取

        //检查标志
        if((data_save.PartitionDataStruct.complate_flag == WRITE_CERT_FACTORY || data_save.PartitionDataStruct.complate_flag == WRITE_CERT_NORMAL) && data_save.PartitionDataStruct.plaintext_len > 0)
        {
            ESP_LOGI(TAG, "need to update cert,len:%lu", data_save.PartitionDataStruct.plaintext_len);
            cer_str = (char*)heap_caps_malloc((data_save.PartitionDataStruct.plaintext_len+1)*sizeof(uint8_t), MALLOC_CAP_SPIRAM); //申请内存
            if(cer_str == NULL)
            {
                ESP_LOGI(TAG, "malloc fail");
                return;
            }
            memset(cer_str, 0, data_save.PartitionDataStruct.plaintext_len+1); //清空内容
            partition_read_decrypt((uint8_t*)cer_str, len, 0, UPDATE_AREA); //读取更新区数据

            find_first_cert_position((uint8_t*)cer_str, len, &offset); //查找第一份证书的位置
            find_cert_and_write("-----BEGIN CERTIFICATE-----", "-----END CERTIFICATE-----", cer_str+offset, &offset, IOT_CERTIFICATE_AREA);
            find_cert_and_write("-----BEGIN PRIVATE KEY-----", "-----END PRIVATE KEY-----", cer_str+offset, &offset, PRIVATE_AREA);
            
            if(data_save.PartitionDataStruct.complate_flag == WRITE_CERT_FACTORY) //如果是工厂写入证书，会多一份CA证书
            {
                find_cert_and_write("-----BEGIN CERTIFICATE-----", "-----END CERTIFICATE-----", cer_str+offset, &offset, CA_CERTIFICATE_AREA);
            }
            update_factory_cert_md5(); //更新MD5数据
            free(cer_str);
        }
        partition_reinit(UPDATE_AREA, PARTITION_SIZE+PARTITION_SIZE); //重新初始化更新区
    }
    else
    {
        //没有证书待拷贝
        ESP_LOGI(TAG, "update area is empty");
    }
}


//保存证书到更新区
void save_cer_to_update_area(uint8_t *data, uint16_t len, uint16_t offset)
{
    //ESP_LOGI(TAG, "write len:%d, offset:%x", len, offset);
    partition_write_encrypt(data, len, offset, UPDATE_AREA); //加密写入UPDATE_AREA区
    //dump_partition("write", data, len);
}

//证书全部保存完毕标志
void save_cer_complate(uint8_t flag)
{
    partition_type_t partition_type;
    get_partitiontype_string(UPDATE_AREA, &partition_type);
    //PARTITION_DATA_UNION partition_data;
    const esp_partition_t *partition = esp_partition_find_first(partition_type.type, 0, partition_type.name); //获取分区表
    
    PARTITION_DATA_UNION data_save;
    uint32_t len = 0;
    len = get_partition_plaintext_len(UPDATE_AREA);
    ESP_LOGE(TAG, "save_cer_complate");
    ESP_LOGE(TAG, "PRIVATE_AREA,len:%lu", len);
    memset(data_save.data, 0, sizeof(data_save.data));
    data_save.PartitionDataStruct.plaintext_len = len;
    data_save.PartitionDataStruct.complate_flag = flag;
    esp_partition_write_raw(partition, 0x1ff0, data_save.data, sizeof(data_save.data)); //最后写入分区信息
}

//计算分区的MD5值
static void calculate_area_md5(uint8_t area, uint8_t *out_md5)
{
    mbedtls_md5_context md5_ctx;
    uint16_t len = 0;
    uint8_t *md5_cert_ptr = NULL;
    len = get_partition_plaintext_len(area);
    if(md5_cert_ptr == NULL)
    {
        md5_cert_ptr = (uint8_t*)heap_caps_malloc((len+1)*sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if(md5_cert_ptr == NULL)
        {
            ESP_LOGE(TAG, "md5_cert_ptr malloc faild");
            return;
        }
    }
    memset(md5_cert_ptr, 0, len+1);
    partition_read_decrypt(md5_cert_ptr, len, 0, area);
    mbedtls_md5_init(&md5_ctx);

	//	  mbedtls_md5_starts_ret(&md5_ctx);
		mbedtls_md5_starts(&md5_ctx);
    ESP_LOGE(TAG, "cert_size:%d", strlen((char*)md5_cert_ptr));
    mbedtls_md5_update(&md5_ctx, md5_cert_ptr, strlen((char*)md5_cert_ptr));
    mbedtls_md5_finish(&md5_ctx, out_md5);
    mbedtls_md5_free(&md5_ctx);
    if(md5_cert_ptr != NULL)
    {
        free(md5_cert_ptr);
    }
}

//更新工厂的证书MD5数据
void update_factory_cert_md5(void)
{
    uint8_t md5_data[48];
    calculate_area_md5(IOT_CERTIFICATE_AREA, md5_data); //计算IOT设备证书MD5值
    calculate_area_md5(PRIVATE_AREA, md5_data+16); //计算私钥证书MD5值
    calculate_area_md5(CA_CERTIFICATE_AREA, md5_data+32); //计算CA证书MD5值
//    memcpy(g_device_data.iot_dev_node.factory.cert_md5, md5_data, sizeof(g_device_data.iot_dev_node.factory.cert_md5));
    
//    dump_partition("iot cert md5", g_device_data.iot_dev_node.factory.cert_md5, 16);
//    dump_partition("private md5", g_device_data.iot_dev_node.factory.cert_md5+16, 16);
//    dump_partition("ca cert md5", g_device_data.iot_dev_node.factory.cert_md5+32, 16);
}



//厂测写入证书完成
void write_cert_factory_complate(void)
{
    save_cer_complate(WRITE_CERT_FACTORY); //工厂模式写入证书完成
    check_for_cert_update(); //检查更新证书到对应区
    //update_factory_cert_md5(); //更新MD5数据 
}

void format_cert_factory_area(void)
{
    partition_reinit(UPDATE_AREA, PARTITION_SIZE+PARTITION_SIZE); //重新初始化更新区
}

#if ENCRYPT_CERT_USE_FILE_SYSTEM
/**
 * @brief 读取网络资产
 * @param file_path 资产文件路径
 * @param buf 读取到的资产数据缓冲区指针
 * @return 读取到的资产数据长度，失败返回0
 */
static uint16_t load_asset(char *file_path, uint8_t **buf)
{
    uint16_t len = 0;
    int fd = -1;

    len = get_file_size(file_path);
    if(len <= 0)
    {
        ESP_LOGE(TAG, "file:%s empty", file_path);
        return len;
    }

    ESP_LOGI(TAG, "Get file:%s, size:%d", file_path, len);

    if(*buf == NULL)
    {
        *buf = (uint8_t*)heap_caps_malloc((len + 1) * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if(*buf == NULL)
        {
            ESP_LOGE(TAG, "load cert malloc faild");
            return len;
        }
    }
    memset(*buf, 0, len + 1);

    /* 只读格式打开文件 */
    fd = open(file_path, O_RDONLY);
    if(fd < 0)
    {
       ESP_LOGE(TAG, "open:%s error", file_path);
        return 0;
    }

    ESP_LOGI(TAG, "open file:%s success, len:%d", file_path, len);
    /* 移动文件指针到设置的数据位置 */
    if(lseek(fd, 0, SEEK_SET) != 0)
    {
        ESP_LOGE(TAG, "[iot file para SetData] start_pos set error");
        goto end;
    }

    /* 读数据到缓存 */
    if(read(fd, *buf, len) != len)
    {
        ESP_LOGE(TAG, "read file:%s error", file_path);
        goto end;
    }

    close(fd);

    (*buf)[len] = 0x0;
    ESP_LOGI(TAG, "after load cert:%s len:%d data:\n[%s]", file_path, len, *buf);

    return len;

end:
    close(fd);
    return 0;
}

static uint16_t load_asset_decrypt(char *file_path, uint8_t **buf)
{
    uint16_t len = 0;
    int fd = -1;

    len = get_file_size(file_path);
    if(len <= 0)
    {
        ESP_LOGE(TAG, "file:%s empty", file_path);
        return 0;
    }

    ESP_LOGI(TAG, "Get file:%s, size:%d", file_path, len);

    uint8_t *ciphertext = (uint8_t *)heap_caps_malloc((len + 1) * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    if(ciphertext == NULL)
    {
        ESP_LOGE(TAG, "load cert decrypt malloc faild");
        return 0;
    }
    memset(ciphertext, 0, len + 1);

    /* 只读格式打开文件 */
    fd = open(file_path, O_RDONLY);
    if(fd < 0)
    {
       ESP_LOGE(TAG, "open:%s error", file_path);
        return 0;
    }

    ESP_LOGI(TAG, "open file:%s success, len:%d", file_path, len);
    /* 移动文件指针到设置的数据位置 */
    if(lseek(fd, 0, SEEK_SET) != 0)
    {
        ESP_LOGE(TAG, "[iot file para SetData] start_pos set error");
        goto end;
    }

    /* 读文件到缓存 */
    if(read(fd, ciphertext, len) != len)
    {
        ESP_LOGE(TAG, "read file:%s error", file_path);
        goto end;
    }
    close(fd);

    // ESP_LOGI(TAG, "Load asset decrypt read len:%d, data:", len);
    // ESP_LOG_BUFFER_HEX_LEVEL(TAG, ciphertext, len, ESP_LOG_INFO);

    PARTITION_DATA_UNION encrypt_info;
    memcpy(&encrypt_info, ciphertext, sizeof(PARTITION_DATA_UNION));

    ESP_LOGI(TAG, "Load asset decrypt read encrypt info,  ciphertext_len:%lu, plaintext_len:%lu", 
            encrypt_info.PartitionDataStruct.ciphertext_len,
            encrypt_info.PartitionDataStruct.plaintext_len);

    if(*buf == NULL)
    {
        //使用填充模式, 密文长度>=明文长度
        *buf = (uint8_t*)heap_caps_malloc((encrypt_info.PartitionDataStruct.ciphertext_len + 1) * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if(*buf == NULL)
        {
            ESP_LOGE(TAG, "load cert decrypt buf malloc faild");
            return len;
        }
    }
    memset(*buf, 0, encrypt_info.PartitionDataStruct.ciphertext_len + 1);

    uint16_t plaintext_len = 0;
    iot_file_decrypt(ciphertext + 16, len - 16, *buf, &plaintext_len);

    ESP_LOGI(TAG, "Load asset decrypt plaintext_len:%d, encrypt_info.PartitionDataStruct.plaintext_len:%lu", plaintext_len, encrypt_info.PartitionDataStruct.plaintext_len);

    //截取明文，删掉后面的填充内容
    (*buf)[encrypt_info.PartitionDataStruct.plaintext_len] = 0x00;
    ESP_LOGI(TAG, "after load cert:%s len:%lu data:\n[%s]", file_path, encrypt_info.PartitionDataStruct.plaintext_len, *buf);

    return len;

end:
    close(fd);
    return 0;
}

/**
 * @brief 写网络资产到文件系统
 * @param file_path 资产文件路径
 * @param buf 写入的资产数据缓冲区指针
 * @param buf_len 写入的资产数据长度
 * @return 写入的资产数据长度，失败返回0
 */
static uint32_t write_asset(const char *fname, const uint8_t *buf, uint32_t buf_len)
{
    if(fname == NULL || buf == NULL || buf_len == 0)
    {
        ESP_LOGE(TAG, "write_asset param error");
        return 0;
    }

    char path[64] = {0};
    int fd = -1;

    snprintf(path, sizeof(path), "%s/%s", RECORD_ROOT_PATH, fname);
    /* 检查目录是否存在 */
    historic_dir_check(fname);

    //Notice: 写入一个大文件，再写入小文件，只会覆盖小文件的部分，后面的内容不会被删除. 所以这里先把文件删除
    remove(path);

    /* 只写格式打开文件,不存在时创建 */
    fd = open(path, O_WRONLY|O_CREAT);
    if(fd < 0) 
    {
        ESP_LOGE(TAG, "Writing file %s failed, open error", fname);
        return 0;
    }

    /* 移动文件指针到文件开始 */
    if(lseek(fd, 0, SEEK_SET) != 0)
    {
        ESP_LOGE(TAG, "Writing file %s failed, lseek error", fname);
        goto end;
    }

    /* 写数据 */
    if(write(fd, buf, buf_len) != buf_len)
    {
        ESP_LOGE(TAG, "Writing file %s failed, write error", fname);
        goto end;
    }

    ESP_LOGI(TAG, "Write asset:%s, len:%lu[%s] to file", path, buf_len, buf);

    close(fd);
    return buf_len;
end:
    close(fd);
    return 0;
}

/**
 * @brief 加密并写网络资产到文件系统
 * @param file_path 资产文件路径
 * @param buf 写入的资产数据缓冲区指针
 * @param buf_len 写入的资产数据长度
 * @return 写入的资产数据长度，失败返回0
 */
static uint32_t write_asset_encrypt(const char *fname, const uint8_t *buf, uint32_t buf_len)
{
    if(fname == NULL || buf == NULL || buf_len == 0)
    {
        ESP_LOGE(TAG, "write_asset param error");
        return 0;
    }

    char path[64] = {0};
    int fd = -1;
    uint32_t ret = 0;
    uint8_t *ciphertext = NULL;
    uint16_t ciphertext_len = 0;
    PARTITION_DATA_UNION partition_data;

    snprintf(path, sizeof(path), "%s/%s", RECORD_ROOT_PATH, fname);
    /* 检查目录是否存在 */
    historic_dir_check(fname);

    remove(path);   //写入一个大文件，再写入小文件，只会覆盖小文件的部分，后面的内容不会被删除.
    /* 只写格式打开文件,不存在时创建 */
    fd = open(path, O_WRONLY|O_CREAT);
    if(fd < 0) 
    {
        ESP_LOGE(TAG, "Writing file %s failed, open error", fname);
        return 0;
    }

    /* 移动文件指针到文件开始 */
    if(lseek(fd, 0, SEEK_SET) != 0)
    {
        ESP_LOGE(TAG, "Writing file %s failed, lseek error", fname);
        goto end;
    }

    /* 密文长度为16的倍数, 并且前16字节为加密信息 */
    ciphertext_len = (buf_len%16 != 0)?(buf_len - (buf_len%16) + 16):(buf_len);
    ciphertext = (uint8_t *)heap_caps_malloc((ciphertext_len + 16 + 1) * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    if (ciphertext == NULL)
    {
        ESP_LOGE(TAG, "Write asset encrypt malloc ciphertext buffer error");
        goto end;
    }

    iot_file_encrypt(buf, buf_len, ciphertext + 16, &ciphertext_len); //AES加密

    memset(partition_data.data, 0, sizeof(partition_data.data));
    partition_data.PartitionDataStruct.ciphertext_len = ciphertext_len; //更新密文长度
    partition_data.PartitionDataStruct.plaintext_len = buf_len; //更新明文长度
    ESP_LOGI(TAG, "Write asset encrypt, plaintext_len:%lu, ciphertext_len:%d", buf_len, ciphertext_len);

    memcpy(ciphertext, partition_data.data, 16);
    /* 写数据 */
    if(write(fd, ciphertext, ciphertext_len + 16) != ciphertext_len + 16)
    {
        ESP_LOGE(TAG, "Writing file %s failed, write error", fname);
        goto end;
    }

    ESP_LOGI(TAG, "Write asset:%s, len:%d[%s] to file", path, ciphertext_len + 16, buf);

    ret = buf_len;
end:
    if (ciphertext)
    {
        free(ciphertext);
        ciphertext = NULL;
    }

    if (fd >= 0)
    {
        close(fd);
    }
    return 0;
}

uint8_t load_all_cer(void)
{
    char path[64] = {0};
    char fname[32] = {0};

    // CA证书
    PARAMETER_FILE_PATH_CA_CERT(fname, 0, 0);
    snprintf(path, sizeof(path), "%s/%s", RECORD_ROOT_PATH, fname);
    historic_dir_check(fname);
#if ENCRYPT_CERT_ENCRYPT_SAVE
    ca_cert_ptr_len = load_asset_decrypt(path, &ca_cert_ptr);
#else
    ca_cert_ptr_len = load_asset(path, &ca_cert_ptr);
#endif
    if (ca_cert_ptr_len == 0)
    {
        ESP_LOGE(TAG, "CA certificate load failed");
        return 0;
    }

    // MQTT加密证书
    memset(fname, 0, sizeof(fname));
    memset(path, 0, sizeof(path));
    PARAMETER_FILE_PATH_IOT_CERT(fname, 0, 0);
    snprintf(path, sizeof(path), "%s/%s", RECORD_ROOT_PATH, fname);
    historic_dir_check(fname);
#if ENCRYPT_CERT_ENCRYPT_SAVE
    iot_cert_ptr_len = load_asset_decrypt(path, &iot_cert_ptr);
#else
    iot_cert_ptr_len = load_asset(path, &iot_cert_ptr);
#endif
    if (iot_cert_ptr_len == 0)
    {
        ESP_LOGE(TAG, "iot certificate load failed");
        return 0;
    }

    // MQTT加密私钥
    memset(fname, 0, sizeof(fname));
    memset(path, 0, sizeof(path));
    PARAMETER_FILE_PATH_IOT_KEY(fname, 0, 0);
    snprintf(path, sizeof(path), "%s/%s", RECORD_ROOT_PATH, fname);
    historic_dir_check(fname);
#if ENCRYPT_CERT_ENCRYPT_SAVE
    private_key_ptr_len = load_asset_decrypt(path, &private_key_ptr);
#else
    private_key_ptr_len = load_asset(path, &private_key_ptr);
#endif
    if (private_key_ptr_len == 0)
    {
        ESP_LOGE(TAG, "private key load failed");
        return 0;
    }

#ifdef TCP2_ENCRYPT_ENABLE
    // CA证书2
    memset(fname, 0, sizeof(fname));
    memset(path, 0, sizeof(path));
    PARAMETER_FILE_PATH_CA_CERT(fname, 1, 0);
    snprintf(path, sizeof(path), "%s/%s", RECORD_ROOT_PATH, fname);
    historic_dir_check(fname);
    load_asset(path, &ca_cert_ptr_2);

    // IOT证书2
    memset(fname, 0, sizeof(fname));
    memset(path, 0, sizeof(path));
    PARAMETER_FILE_PATH_IOT_CERT(fname, 1, 0);
    snprintf(path, sizeof(path), "%s/%s", RECORD_ROOT_PATH, fname);
    historic_dir_check(fname);
    load_asset(path, &iot_cert_ptr_2);

    // 私钥2
    memset(fname, 0, sizeof(fname));
    memset(path, 0, sizeof(path));
    PARAMETER_FILE_PATH_IOT_KEY(fname, 1, 0);
    snprintf(path, sizeof(path), "%s/%s", RECORD_ROOT_PATH, fname);
    historic_dir_check(fname);
    load_asset(path, &private_key_ptr_2);
#endif
    return 1;
}

/**
 * @brief 加载Modbus TCP证书
 * @return 1-成功，0-失败
 */
uint8_t load_modbus_tcp_cert(void)
{
    char path[64] = {0};
    char fname[32] = {0};

    /* Modbus TCP Slave */
    // CA证书
    PARAMETER_FILE_PATH_CA_CERT(fname, 2, 0);
    snprintf(path, sizeof(path), "%s/%s", RECORD_ROOT_PATH, fname);
    historic_dir_check(fname);
    server_ca_cert_ptr_len = load_asset(path, &server_ca_cert_ptr);
    if (server_ca_cert_ptr_len == 0)
    {
        ESP_LOGE(TAG, "%s ca certificate load failed", path);
        return 0;
    }

    // IOT证书
    memset(fname, 0, sizeof(fname));
    memset(path, 0, sizeof(path));
    PARAMETER_FILE_PATH_IOT_CERT(fname, 2, 0);
    snprintf(path, sizeof(path), "%s/%s", RECORD_ROOT_PATH, fname);
    historic_dir_check(fname);
    server_cert_ptr_len = load_asset(path, &server_cert_ptr);
    if (server_cert_ptr_len == 0)
    {
        ESP_LOGE(TAG, "server certificate:%s load failed", path);
        return 0;
    }

    // 私钥
    memset(fname, 0, sizeof(fname));
    memset(path, 0, sizeof(path));
    PARAMETER_FILE_PATH_IOT_KEY(fname, 2, 0);
    snprintf(path, sizeof(path), "%s/%s", RECORD_ROOT_PATH, fname);
    historic_dir_check(fname);
    server_key_ptr_len = load_asset(path, &server_key_ptr);
    if (server_key_ptr_len == 0)
    {
        ESP_LOGE(TAG, "private key:%s load failed", path);
        return 0;
    }

    return 1;
}


/**
 * @brief 写证书到文件系统
 * @param data 证书数据指针
 * @param len 证书数据长度
 * @param type 证书类型
 */
int16_t write_cert_to_file(uint8_t type, uint8_t *data, uint16_t len)
{
    char path[50] = {0};
    char fname[30] = {0};
    int fd = -1;
    int ret = 0;

    if(type == CA_CERTIFICATE_AREA)
    {
        PARAMETER_FILE_PATH_CA_CERT(fname, 0, 0);
    }
    else if(type == IOT_CERTIFICATE_AREA)
    {
        PARAMETER_FILE_PATH_IOT_CERT(fname, 0, 0);
    }
    else if(type == PRIVATE_AREA)
    {
        PARAMETER_FILE_PATH_IOT_KEY(fname, 0, 0);
    }
#ifdef TCP2_ENCRYPT_ENABLE
    else if(type == CA_CERTIFICATE_AREA_2)
    {
        PARAMETER_FILE_PATH_CA_CERT(fname, 1, 0);
    }
    else if(type == IOT_CERTIFICATE_AREA_2)
    {
        PARAMETER_FILE_PATH_IOT_CERT(fname, 1, 0);
    }
    else if(type == PRIVATE_AREA_2)
    {
        PARAMETER_FILE_PATH_IOT_KEY(fname, 1, 0);
    }
#endif
    else if(type == MD_TCP_SERVER_CA_AREA)
    {
        PARAMETER_FILE_PATH_CA_CERT(fname, 2, 0);
    }
    else if(type == MD_TCP_SERVER_CERT_AREA)
    {
        PARAMETER_FILE_PATH_IOT_CERT(fname, 2, 0);
    }
    else if(type == MD_TCP_SERVER_PRIVATE_AREA)
    {
        PARAMETER_FILE_PATH_IOT_KEY(fname, 2, 0);
    }
    else
    {
        ESP_LOGE(TAG, "write cert to file type error");
        return -1;
    }
#if ENCRYPT_CERT_ENCRYPT_SAVE
    write_asset_encrypt(fname, data, len);
#else
    write_asset(fname, data, len); //写入到文件系统
#endif
    return len;
}
#else
//加载全部证书和秘钥到动态内存中
/*

return:
0-fail
not 0-ok
*/
uint8_t load_all_cer(void)
{
    uint8_t ret = 0;
    uint16_t len = 0;
    uint16_t len_2 = 0;
	
    len = get_partition_plaintext_len(CA_CERTIFICATE_AREA);
    if(len == 0)
    {
        ESP_LOGE(TAG, "CA_CERTIFICATE_AREA empty");
        return ret;
    }
    if(ca_cert_ptr == NULL)
    {
        ca_cert_ptr = (uint8_t*)heap_caps_malloc((len+1)*sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if(ca_cert_ptr == NULL)
        {
            ESP_LOGE(TAG, "ca_cert_ptr malloc faild");
            return ret;
        }
    }
    memset(ca_cert_ptr, 0, len+1);
    partition_read_decrypt(ca_cert_ptr, len, 0, CA_CERTIFICATE_AREA);
    ca_cert_ptr[len] = 0x0;
    ca_cert_ptr_len = len;
    ESP_LOGI(TAG, "after load_all_cer ca_cert_ptr len:%d data:\n[%s]", ca_cert_ptr_len, ca_cert_ptr);

#ifdef     TCP2_ENCRYPT_ENABLE	
    len_2 = get_partition_plaintext_len(CA_CERTIFICATE_AREA_2);
    if(len_2 == 0)
    {
        ESP_LOGE(TAG, " CA_CERTIFICATE_AREA_2 empty");
        return ret;
    }	
    if(ca_cert_ptr_2 == NULL)
    {
        ca_cert_ptr_2 = (uint8_t*)heap_caps_malloc((len_2+1)*sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if(ca_cert_ptr_2 == NULL)
        {
            return ret;
        }
    }
    memset(ca_cert_ptr_2, 0, len_2+1);
    partition_read_decrypt(ca_cert_ptr_2, len_2, 0, CA_CERTIFICATE_AREA_2);
#endif
    len = get_partition_plaintext_len(IOT_CERTIFICATE_AREA);
    if(len == 0)
    {
        ESP_LOGE(TAG, "IOT_CERTIFICATE_AREA empty");
        return ret;
    }
    if(iot_cert_ptr == NULL)
    {
        iot_cert_ptr = (uint8_t*)heap_caps_malloc((len+1)*sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if(iot_cert_ptr == NULL)
        {
            ESP_LOGE(TAG, "iot_cert_ptr malloc faild");
            return ret;
        }
    }
    memset(iot_cert_ptr, 0, len+1);
    partition_read_decrypt(iot_cert_ptr, len, 0, IOT_CERTIFICATE_AREA);
    iot_cert_ptr[len] = 0x0;
    iot_cert_ptr_len = len;
    ESP_LOGI(TAG, "after load_all_cer iot_cert_ptr data:\n[%s]", iot_cert_ptr);//testwx

#ifdef     TCP2_ENCRYPT_ENABLE	
    if(iot_cert_ptr_2 == NULL)
    {
        iot_cert_ptr_2 = (uint8_t*)heap_caps_malloc((len_2+1)*sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if(iot_cert_ptr_2 == NULL)
        {
            ESP_LOGE(TAG, "iot_cert_ptr malloc faild");
            return ret;
        }
    }
    memset(iot_cert_ptr_2, 0, len_2+1);
    partition_read_decrypt(iot_cert_ptr_2, len_2, 0, IOT_CERTIFICATE_AREA_2);
#endif
    //两套Key证书
    len = get_partition_plaintext_len(PRIVATE_AREA);
    if(len == 0)
    {
        ESP_LOGE(TAG, "PRIVATE_AREA empty");
        return ret;
    }
    if(private_key_ptr == NULL)
    {
        private_key_ptr = (uint8_t*)heap_caps_malloc((len+1)*sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if(private_key_ptr == NULL)
        {
            ESP_LOGE(TAG, "private_key_ptr malloc faild");
            return ret;
        }
    }
    memset(private_key_ptr, 0, len+1);
    partition_read_decrypt(private_key_ptr, len, 0, PRIVATE_AREA);
    private_key_ptr_len = len;
#ifdef     TCP2_ENCRYPT_ENABLE	
    len_2 = get_partition_plaintext_len(PRIVATE_AREA_2);
    if(len_2 == 0)
    {
        ESP_LOGE(TAG, "PRIVATE_AREA_2 empty");
        return ret;
    }	
    if(private_key_ptr_2 == NULL)
    {
        private_key_ptr_2 = (uint8_t*)heap_caps_malloc((len_2+1)*sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if(private_key_ptr_2 == NULL)
        {
            ESP_LOGE(TAG, "private_key_ptr malloc faild");
            return ret;
        }
    }
    memset(private_key_ptr_2, 0, len_2+1);
    partition_read_decrypt(private_key_ptr_2, len_2, 0, PRIVATE_AREA_2);
#endif
    ret = 1;
    return ret;
}

//重新加载证书
void reload_cert(uint8_t *cert_ptr, uint8_t area)
{
    uint16_t len = 0;
    len = get_partition_plaintext_len(area);
    if(cert_ptr != NULL)
    {
        free(cert_ptr);
    }

    if(cert_ptr == NULL)
    {
        cert_ptr = (uint8_t*)heap_caps_malloc((len+1)*sizeof(uint8_t), MALLOC_CAP_SPIRAM);
         if(cert_ptr == NULL)
        {
            ESP_LOGE(TAG, "cert_ptr malloc faild");
            return;
        }
    }
    memset(cert_ptr, 0, len+1);
    partition_read_decrypt(cert_ptr, len, 0, area);
}
#endif

//证书写入(头文件写入，调试使用)
void write_cert(void)
{
#if ENCRYPT_CERT_USE_FILE_SYSTEM
    write_cert_to_file(CA_CERTIFICATE_AREA, (uint8_t*)power_my_ca_cert_text, strlen(power_my_ca_cert_text));
    // write_cert_to_file(IOT_CERTIFICATE_AREA, (uint8_t*)power_mqtt_cert, strlen(power_mqtt_cert));
    // write_cert_to_file(PRIVATE_AREA, (uint8_t*)power_private_key, strlen(power_private_key));

    write_cert_to_file(IOT_CERTIFICATE_AREA, (uint8_t*)mqtts_iot_cert, strlen(mqtts_iot_cert));
    write_cert_to_file(PRIVATE_AREA, (uint8_t*)mqtts_iot_key, strlen(mqtts_iot_key));
#else
    partition_reinit(CA_CERTIFICATE_AREA, PARTITION_SIZE);
    partition_reinit(IOT_CERTIFICATE_AREA, PARTITION_SIZE);
    partition_reinit(PRIVATE_AREA, PARTITION_SIZE);
    partition_reinit(UPDATE_AREA, PARTITION_SIZE);//D +PARTITION_SIZE
#ifdef     TCP2_ENCRYPT_ENABLE	
    partition_reinit(CA_CERTIFICATE_AREA_2, PARTITION_SIZE);
    partition_reinit(IOT_CERTIFICATE_AREA_2, PARTITION_SIZE);
    partition_reinit(PRIVATE_AREA_2, PARTITION_SIZE);
    partition_reinit(UPDATE_AREA_2, PARTITION_SIZE);
#endif
    /***************************证书写入****************************/
    partition_write_encrypt((uint8_t*)power_my_ca_cert_text, strlen(power_my_ca_cert_text), 0, CA_CERTIFICATE_AREA);
    partition_write_encrypt((uint8_t*)power_mqtt_cert, strlen(power_mqtt_cert), 0, IOT_CERTIFICATE_AREA);
    partition_write_encrypt((uint8_t*)power_private_key, strlen(power_private_key), 0, PRIVATE_AREA);
#ifdef     TCP2_ENCRYPT_ENABLE	
    partition_write_encrypt((uint8_t*)power_my_ca_cert_text_2, strlen(power_my_ca_cert_text_2), 0, CA_CERTIFICATE_AREA_2);
    partition_write_encrypt((uint8_t*)power_mqtt_cert_2, strlen(power_mqtt_cert_2), 0, IOT_CERTIFICATE_AREA_2);
    partition_write_encrypt((uint8_t*)power_private_key_2, strlen(power_private_key_2), 0, PRIVATE_AREA_2);
#endif

#endif	
}

//证书读取(分区读取，调试使用)
void read_cert(void)
{
    uint16_t len_max = 0;
    uint16_t len = 0;
    uint8_t *buf = NULL;
#ifdef     TCP2_ENCRYPT_ENABLE	
    len = get_partition_plaintext_len(CA_CERTIFICATE_AREA);
    len_max = get_partition_plaintext_len(CA_CERTIFICATE_AREA_2);
    if(len == len_max)
    {
        ESP_LOGW(TAG,"000 len:%d, len_max:%d",len,len_max);
    }
    else
    {
        ESP_LOGW(TAG,"111 len:%d, len_max:%d",len,len_max);
    }
#else

#endif	

    len_max = get_partition_plaintext_len(CA_CERTIFICATE_AREA);
    len = get_partition_plaintext_len(IOT_CERTIFICATE_AREA);
    len_max = (len_max < len)?len:len_max;
    len = get_partition_plaintext_len(PRIVATE_AREA);
    len_max = (len_max < len)?len:len_max;
    len_max+=1;
    buf = (uint8_t*)heap_caps_malloc(len_max*sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    if(buf == NULL)
    {
        ESP_LOGE(TAG, "malloc fail!");
        return;
    }
    memset(buf, 0 , len_max);

    /************************证书读取***********************/
    len = get_partition_plaintext_len(CA_CERTIFICATE_AREA);
    partition_read_decrypt(buf, len, 0, CA_CERTIFICATE_AREA);
    ESP_LOGE(TAG, "ca data:len:%d\n%s", len, buf);
    memset(buf, 0 , len_max);
    len = get_partition_plaintext_len(IOT_CERTIFICATE_AREA);
    partition_read_decrypt(buf, len, 0, IOT_CERTIFICATE_AREA);
    ESP_LOGE(TAG, "iot data:len:%d\n%s", len, buf);
    memset(buf, 0 , len_max);
    len = get_partition_plaintext_len(PRIVATE_AREA);
    partition_read_decrypt(buf, len, 0, PRIVATE_AREA);
    ESP_LOGE(TAG, "private data:len:%d\n%s", len, buf);
    //testwx
    memset(buf, 0 , len_max);
    len = get_partition_plaintext_len(UPDATE_AREA);
    partition_read_decrypt(buf, len, 0, UPDATE_AREA);
#ifdef     TCP2_ENCRYPT_ENABLE	
    len = get_partition_plaintext_len(CA_CERTIFICATE_AREA_2);
    partition_read_decrypt(buf, len, 0, CA_CERTIFICATE_AREA_2);
    ESP_LOGE(TAG, "2 ca data:len:%d\n%s", len, buf);
    memset(buf, 0 , len_max);
    len = get_partition_plaintext_len(IOT_CERTIFICATE_AREA_2);
    partition_read_decrypt(buf, len, 0, IOT_CERTIFICATE_AREA_2);
    ESP_LOGE(TAG, "2 iot data:len:%d\n%s", len, buf);
    memset(buf, 0 , len_max);
    len = get_partition_plaintext_len(PRIVATE_AREA_2);
    partition_read_decrypt(buf, len, 0, PRIVATE_AREA_2);
    ESP_LOGE(TAG, "2 private data:len:%d\n%s", len, buf);
    memset(buf, 0 , len_max);
    len = get_partition_plaintext_len(UPDATE_AREA_2);
    partition_read_decrypt(buf, len, 0, UPDATE_AREA_2);	
#else

#endif	
    // ESP_LOGE(TAG, "UPDATE_AREA data:len:%d\n%s", len, buf);
    free(buf);
}



void test_partition(void)
{
    write_cert();
    read_cert();
    load_all_cer();
    if (ca_cert_ptr_len > 0)
    {
        ESP_LOGE(TAG, "ca_cert_ptr data:\n%s", ca_cert_ptr);
    }
    if (iot_cert_ptr_len)
    {
        ESP_LOGE(TAG, "iot_cert_ptr data:\n%s", iot_cert_ptr);
    }
    if (private_key_ptr_len > 0)
    {
        ESP_LOGE(TAG, "private_key_ptr data:\n%s", private_key_ptr);
    }
#ifdef     TCP2_ENCRYPT_ENABLE	
    ESP_LOGW(TAG, "ca_cert_ptr data_2:\n%s", ca_cert_ptr_2);
    ESP_LOGW(TAG, "iot_cert_ptr data_2:\n%s", iot_cert_ptr_2);
    ESP_LOGW(TAG, "private_key_ptr data_2:\n%s", private_key_ptr_2);
#else

#endif	
}

void test_bin(void)
{
    uint16_t refresh_count = 0;
    uint16_t rsa_decrypt_len = 0;
    uint16_t count = 0;
    uint32_t offset = 0;
    uint32_t write_offset = 0;
    uint8_t my_data[235];
    uint8_t out_data[235];
    partition_type_t partition_type;
    count = 1450420/235;
    ESP_LOGI(TAG, "count:%d", count);
    priv_rsa_refresh_seed();
    const esp_partition_t *partition = esp_partition_find_first(0x44, 0, "storage"); //获取分区表
    assert(partition != NULL);
    for(uint16_t i = 0; i < count; i++)
    {
        //priv_rsa_refresh_seed();
        ESP_LOGI(TAG, "offset:%lu,write_offset:%lu\n", offset, write_offset);
        esp_partition_read_raw(partition,offset, my_data, sizeof(my_data)); //读取分区表信息头
        priv_rsa_decrypt(my_data, out_data, &rsa_decrypt_len); //其中11个字节为填充
        offset+=235;
        write_offset+=224;
        // if((refresh_count++) % 100 == 0)
        // {
        //     priv_rsa_refresh_seed();
        //     mbedtls_printf( "priv_rsa_refresh_seed\n");
        // }
        //dump_partition("encrypt", my_data, sizeof(my_data));
        //dump_partition("plaintext", out_data, 224);
        //vTaskDelay(pdMS_TO_TICKS(5));  
    }
     //RSA解密失败
    {
        ESP_LOGI(TAG, "RSA decrypt fail!");
    }
}

void test_sha256(void)
{
    uint8_t sign[235];
    uint16_t file_size = 0;
    int ret = -0x006E;
    uint8_t *buffer1 = NULL;
    uint8_t *buffer2 = NULL;
    uint32_t offset = 0;
    uint16_t count = 0;
    buffer1 = (uint8_t*)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
    if(buffer1 == NULL)
    {
        ESP_LOGI(TAG, "buffer1 malloc faild");
    }
    memset(buffer1, 0, 1024);
    buffer2 = (uint8_t*)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
    if(buffer2 == NULL)
    {
        ESP_LOGI(TAG, "buffer2 malloc faild");
    }
    memset(buffer2, 0, 1024);
    mbedtls_md_context_t ctx;
    const esp_partition_t *partition_data = esp_partition_find_first(0x44, 0, "storage"); //获取分区表
    assert(partition_data != NULL);
    const esp_partition_t *partition_ota = esp_ota_get_next_update_partition(NULL); // 获取当前OTA分区
    assert(partition_data != NULL);
    iot_ota_begin();
    file_size = 3099;
    count = file_size/1024;
    for(uint16_t i = 0; i < count; i++)
    {
        ESP_LOGI(TAG, "offset:%lu", offset);
        esp_partition_read_raw(partition_data,offset, buffer1, 1024); //读取分区表信息头
        dump_partition("raw", buffer1, 1024);
        iot_ota_write(buffer1, 1024);
        offset += 1024;
    }
    if(file_size%1024 != 0) //不足224字节部分
    {
        ESP_LOGI(TAG, "offset:%lu", offset);
        esp_partition_read_raw(partition_data,offset, buffer1, file_size%1024); //读取分区表信息头
        dump_partition("raw", buffer1, file_size%1024);
        iot_ota_write(buffer1, file_size%1024);
        offset += file_size%1024;
    }
    ca_rsa_verify_ota_bin(file_size, sign);
}

void test_update(void)
{
    uint16_t offset = 0;
    ESP_LOGI(TAG, "offset:%d", offset);
    save_cer_to_update_area(iot_cert_ptr, strlen((char*)iot_cert_ptr), offset);
    offset+=strlen((char*)iot_cert_ptr);
    ESP_LOGI(TAG, "offset:%d", offset);
    save_cer_to_update_area(private_key_ptr, strlen((char*)private_key_ptr), offset);
    save_cer_complate(WRITE_CERT_NORMAL);
    check_for_cert_update();
}
