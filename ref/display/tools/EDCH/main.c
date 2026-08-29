#include <stdio.h>
#include <stdlib.h>
#include "micro-ecc/uECC.h"
#include "sha256.h"

uint8_t public_key[] = {0x50, 0x1A, 0x66, 0xAB, 0x54, 0x2B, 0xA2, 0xA3, 0x88, 0x61, 0x40, 0x4F, 0xD0, 0xF4, 0xC2, 0x7C,
     0xFA, 0xBD, 0xF1, 0x28, 0x5E, 0x5E, 0x75, 0x51, 0x6A, 0xBB, 0xDF, 0xBD, 0xE1, 0xED, 0x9B, 0x8A,
     0x40, 0x68, 0x3E, 0x36, 0xE8, 0x3C, 0xA4, 0xBC, 0x4A, 0xD3, 0x55, 0xD1, 0x9B, 0x94, 0x85, 0x6F,
     0xDE, 0xE9, 0xCC, 0xBD, 0xF5, 0x16, 0x21, 0x14, 0x51, 0xE3, 0x8E, 0x16, 0x8A, 0x5B, 0x8E, 0x24};

uint8_t private_key[] = {0x0D, 0xB5, 0x94, 0x54, 0xC0, 0xC0, 0x21, 0xE5, 0xDC, 0x85, 0x43, 0xD1, 0x99, 0xA3, 0x8A, 0x26,
     0x3F, 0xF3, 0xA0, 0x95, 0xCB, 0x46, 0x1D, 0x28, 0x38, 0x71, 0xAD, 0xEF, 0x1B, 0xC1, 0x92, 0x40};

static void dump_partition(char *info, uint8_t *buf, uint32_t len)
{
    printf("%s", info);
    for (int i = 0; i < len; i++) {
        printf("%s0x%02X%s,", i % 16 == 0 ? "\n     ":" ",
                        buf[i], i == len - 1 ? "\n":"");
    }
}


void vli_print(char *str, uint8_t *vli, unsigned int size) {
    printf("%s ", str);
    for(unsigned i=0; i<size; ++i) {
        printf("%02X ", (unsigned)vli[i]);
    }
    printf("\n");
}

void test_sha256(void)
{
    const char *m = "abc";
    uint8_t digest[32];
    struct tc_sha256_state_struct s;
    tc_sha256_init(&s);
    tc_sha256_update(&s, (const uint8_t *) m, strlen(m));
    tc_sha256_final(digest, &s);
    dump_partition("sha256", digest, sizeof(digest));
}

void gen_key(void)
{
    uint8_t private[32];
    uint8_t public[64];

    if (!uECC_make_key(public, private, uECC_secp256r1()))
    {
        printf("uECC_make_key() failed\n");
    }
    dump_partition("public key", public, sizeof(public));
    dump_partition("private key", private, sizeof(private));
}

long get_file_size(FILE *stream)
{
	long file_size = -1;
	long cur_offset = ftell(stream);	// 获取当前偏移位置
	if (cur_offset == -1) {
		printf("ftell failed\n");
		return -1;
	}
	if (fseek(stream, 0, SEEK_END) != 0) {	// 移动文件指针到文件末尾
		printf("fseek failed:\n");
		return -1;
	}
	file_size = ftell(stream);	// 获取此时偏移值，即文件大小
	if (file_size == -1) {
		printf("ftell failed\n");
	}
	if (fseek(stream, cur_offset, SEEK_SET) != 0) {	// 将文件指针恢复初始位置
		printf("fseek failed:\n");
		return -1;
	}
	return file_size;
}

//填充数据为256字节整数倍
void fill_bin_256_integer_multiple(char *file_path)
{
    uint32_t offset = 0;
    uint8_t data = 0xff;
    //将文件填充为256字节的整数倍
    FILE *in_f;
    long file_size = 0;
    //打开文件
    if( ( in_f = fopen( file_path, "r+b" ) ) == NULL )
    {
        printf( "open file failed\n");
    }
    file_size = get_file_size(in_f);
    offset = file_size;
    printf("Before filled bin size::%d\n", file_size);
    if(file_size%256 != 0)
    {
        for(uint16_t i = 0; i < 256-file_size%256; i++)
        {
            if (fseek(in_f,offset,SEEK_SET)!= 0) //文件偏移
            {
                printf("fseek failed:\n");
                goto cleanup;
            }
            fwrite(&data, 1, 1, in_f);
            offset+=1;
        }
        file_size = get_file_size(in_f);
        printf("After filled bin size::%d\n", file_size);

    }
cleanup:
    fclose(in_f);
}
void calculate_bin_sha256(char *file_path, uint8_t *hash, uint8_t fill_enable, uint8_t size_enable)
{
    FILE *in_f;
    long file_size = 0;
    uint8_t in_buf[1024];
    uint32_t offset = 0;
    uint16_t count = 0;
    struct tc_sha256_state_struct s;
    tc_sha256_init(&s);
    if(fill_enable) //是否需要填充
    {
        fill_bin_256_integer_multiple(file_path); //填充为256字节整数倍
    }
    //打开文件
    if( ( in_f = fopen( file_path, "rb" ) ) == NULL )
    {
        printf( "open file failed\n");
    }
    file_size = get_file_size(in_f);

    if(size_enable)
    {
        file_size-=file_size%256;
    }
    printf( "file size:%d\n", file_size);
    count = file_size/1024; //计算整数倍
    //printf( "count:%d, rest:%d\n", count, file_size%1024);
    for(uint16_t i = 0; i < count; i++)
    {
        fseek(in_f,offset,SEEK_SET); //文件偏移
        //printf( "offset:%ld\n", offset);
        fread(in_buf, 1, 1024, in_f); //读取文件
        if(!tc_sha256_update(&s, (const uint8_t *) in_buf, 1024))
        {
            printf( "ha256 update fail\n");
            goto cleanup;
        }
        offset += 1024;
    }
    if(file_size%1024 != 0) //不足1024字节部分
    {
        fseek(in_f,offset,SEEK_SET); //文件偏移
        //printf( "offset:%ld\n", offset);
        fread(in_buf, 1, file_size%1024, in_f); //读取文件

        if(!tc_sha256_update(&s, (const uint8_t *) in_buf, file_size%1024))
        {
            printf( "ha256 update fail\n");
            goto cleanup;
        }

        offset += file_size%1024;
    }
    tc_sha256_final(hash, &s);

cleanup:
    fclose(in_f);
}

//检查签名标志
uint8_t check_sign_flag(char *file_path)
{
    uint8_t ret = 0;
    uint8_t flag_num = 0;
    uint8_t last_data[256];
    FILE *in_f;
    long file_size = 0;
    uint32_t offset = 0;
    if( ( in_f = fopen( file_path, "rb" ) ) == NULL )
    {
        printf( "open file failed\n");
    }
    file_size = get_file_size(in_f);
    if(file_size%256 < 96)
    {
        printf("less than 96\n");
        goto cleanup;
    }
    if (fseek(in_f, file_size-(file_size%256),SEEK_SET)!= 0) //文件偏移
    {
        printf("fseek failed:\n");
        goto cleanup;
    }
    memset(last_data, 0, sizeof(last_data));
    fread(last_data, 1, file_size%256, in_f); //读取文件
    for(uint8_t count = file_size%256; count > 0; count--)
    {
        if(last_data[count] == 0x1b)
        {
            flag_num++;
        }
        else
        {
            flag_num = 0;
            continue;
        }
        if(flag_num == 32) //累计找到32个0x1b
        {
            printf("find 32 bytes 0x1b\n");
            ret = 1;
            break;
        }
    }
    printf("bin need to add sign flag\n");

cleanup:
    fclose(in_f);
    return ret;
}

//文件ECDSA签名
void file_sign(char *file_path)
{
    uint8_t sign_data[64];
    FILE *in_f;
    long file_size = 0;
    uint8_t hash[32];
    uint8_t fill_data[96];
    if(check_sign_flag(file_path)) //检查签标志
    {
        printf( "bin has signed,break!\n");
        return;
    }
    calculate_bin_sha256(file_path, hash, 1, 0); //计算文件sha256值
    //dump_partition("hash", hash, sizeof(hash));
    if( ( in_f = fopen( file_path, "r+b" ) ) == NULL )
    {
        printf( "open file failed\n");
    }
    file_size = get_file_size(in_f);
    memset(fill_data, 0, sizeof(fill_data));
    memset(sign_data, 0, sizeof(sign_data));
    uECC_sign(private_key, hash, sizeof(hash), sign_data, uECC_secp256r1()); //文件签名
    //dump_partition("sign", sign_data, sizeof(sign_data));
    for(uint8_t j = 0; j < 32; j++)
    {
        fill_data[j] = 0x1b;
    }
    memcpy(fill_data+32, sign_data, sizeof(sign_data));
    dump_partition("fill data", fill_data, sizeof(fill_data));
    if (fseek(in_f,file_size,SEEK_SET)!= 0) //文件偏移
    {
        printf("fseek failed:\n");
        goto cleanup;
    }
    fwrite(fill_data, 1, sizeof(fill_data), in_f);
cleanup:
    file_size = get_file_size(in_f);
    printf( "final size:%d\n", file_size);
    fclose(in_f);
}

//找到签名值
void find_sign_data(char *file_path, uint8_t *sign_data)
{
    uint8_t flag_num = 0;
    uint8_t last_data[256];
    FILE *in_f;
    long file_size = 0;
    uint32_t offset = 0;
    if( ( in_f = fopen( file_path, "r+b" ) ) == NULL )
    {
        printf( "open file failed\n");
    }
    file_size = get_file_size(in_f);
    offset = file_size-(file_size%256);
    if (fseek(in_f, file_size-(file_size%256),SEEK_SET)!= 0) //文件偏移
    {
        printf("fseek failed:\n");
        goto cleanup;
    }
    memset(last_data, 0, sizeof(last_data));
    fread(last_data, 1, file_size%256, in_f); //读取文件
    for(uint8_t count = file_size%256; count >= 0; count--)
    {
        if(last_data[count] == 0x1b)
        {
            flag_num++;
        }
        else
        {
            flag_num = 0;
            continue;
        }

        if(flag_num == 32) //累计找到32个0x1b
        {
            printf("find 32 bytes 0x1b\n");
            memcpy(sign_data, &last_data[count+32], 64);
            break;
        }
    }

cleanup:
    fclose(in_f);
}

void file_verify(char *file_path)
{
    uint8_t sign_data[64];
    uint8_t hash[32];
    calculate_bin_sha256(file_path, hash, 0, 1);
    find_sign_data(file_path, sign_data);
    if(!uECC_verify(public_key, hash, sizeof(hash), sign_data, uECC_secp256r1()))
    {
        printf("check verify fail\n");
        //system("pause");
    }
    else
    {
        printf("check verify ok\n");

    }
    for(unsigned i=0; i<32; ++i) {
        printf("%02X ", (unsigned)hash[i]);
    }
}
int main(int argc, char *argv[])
{
    //gen_key();
    if(argc >= 2)
    {
        file_sign(argv[1]);
        file_verify(argv[1]);
    }
    return 0;
}
