// 环形队列头文件
#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

// 队列容量需为2的幂以便高效取模
#define CIRCULAR_QUEUE_MAX_SIZE 512

#pragma pack(1)

typedef struct {
    uint8_t *buffer;
    uint16_t total; // 当前元素数量
    uint16_t head; // 写入指针
    uint16_t tail; // 读取指针
} CircularQueue;

#pragma pack()

bool CircularQueue_Init(CircularQueue *q, uint16_t size);
bool CircularQueue_Uninit(CircularQueue *q);
// 写入len字节，空间不足则丢弃本次写入，返回实际写入数量
uint16_t CircularQueue_Push(CircularQueue *q, const uint8_t *data, uint16_t len);
// 读取len字节，len=0xFFFF时读取所有可读内容，返回实际读取数量
uint16_t CircularQueue_Pop(CircularQueue *q, uint8_t *data, uint16_t len);
uint16_t CircularQueue_Available(const CircularQueue *q); // 队列中可读数据量
uint16_t CircularQueue_Space(const CircularQueue *q);     // 队列剩余空间
bool CircularQueue_IsEmpty(const CircularQueue *q);
bool CircularQueue_IsFull(const CircularQueue *q);

#endif // CIRCULAR_QUEUE_H
