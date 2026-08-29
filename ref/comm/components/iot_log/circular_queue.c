#include "circular_queue.h"
#include "utils.h"
#include <stdlib.h>

#include "esp_log.h"

#define TAG "[CircularQueue]"

bool CircularQueue_Init(CircularQueue *q, uint16_t size)
{
    if (!q || size == 0 || size > CIRCULAR_QUEUE_MAX_SIZE)
    {
        return false;
    }

    q->buffer = (uint8_t *)iot_malloc(size * sizeof(uint8_t));
    if (!q->buffer)
    {
        ESP_LOGE(TAG, "CircularQueue_Init: failed to allocate memory");
        return false;
    }
    q->total = size;

    q->head = 0;
    q->tail = 0;

    ESP_LOGI(TAG, "CircularQueue_Init: initialized with size %u", size);

    return true;
}

bool CircularQueue_Uninit(CircularQueue *q)
{
    if (q && q->buffer)
    {
        iot_free(q->buffer);
        q->buffer = NULL;
        q->total = 0;
        q->head = 0;
        q->tail = 0;
    }

    return true;
}

bool CircularQueue_IsEmpty(const CircularQueue *q)
{
    if (!q)
    {
        return true;
    }

    return q->head == q->tail;
}

bool CircularQueue_IsFull(const CircularQueue *q)
{
    if (!q)
    {
        return false;
    }

    return ((q->head + 1) % q->total) == q->tail;
}

// 批量写入，空间不足则全部丢弃
uint16_t CircularQueue_Push(CircularQueue *q, const uint8_t *data, uint16_t len)
{
    if (!q || !data || len == 0)
    {
        ESP_LOGE(TAG, "CircularQueue_Push: invalid parameters");
        return 0;
    }

    uint16_t space = CircularQueue_Space(q);
    if (len > space)
    {
        // ESP_LOGE(TAG, "CircularQueue_Push: not enough space, required=%u, available=%u", len, space);
        return 0; // 空间不足，全部丢弃
    }

    for (uint16_t i = 0; i < len; ++i)
    {
        q->buffer[q->head] = data[i];
        q->head = (q->head + 1) % q->total;
    }

    // ESP_LOGI(TAG, "CircularQueue_Push: pushed %u bytes, space left=%u", len, CircularQueue_Space(q));
    return len;
}

// 批量读取，len=0xFFFF时读取所有
uint16_t CircularQueue_Pop(CircularQueue *q, uint8_t *data, uint16_t len)
{
    if (!q || !data)
    {
        ESP_LOGE(TAG, "CircularQueue_Pop: invalid parameters");
        return 0;
    }

    uint16_t available = CircularQueue_Available(q);
    if (len == 0xFFFF)
    {
        len = available;
    }

    if (len > available)
    {
        len = available;
    }

    for (uint16_t i = 0; i < len; ++i)
    {
        data[i] = q->buffer[q->tail];
        q->tail = (q->tail + 1) % q->total;
    }

    return len;
}

// 队列中可读数据量
uint16_t CircularQueue_Available(const CircularQueue *q)
{
    if (!q)
    {
        ESP_LOGE(TAG, "CircularQueue_Available: invalid parameters");
        return 0;
    }
    if (q->head >= q->tail)
    {
        return q->head - q->tail;
    }
    else
    {
        return q->total - q->tail + q->head;
    }
}

// 队列剩余空间
uint16_t CircularQueue_Space(const CircularQueue *q)
{
    if (!q)
    {
        ESP_LOGE(TAG, "CircularQueue_Space: invalid parameters");
        return 0;
    }
    return q->total - 1 - CircularQueue_Available(q);
}

#ifdef CIRCULAR_QUEUE_DEMO
#include <stdio.h>
int main(void)
{
    CircularQueue q;
    CircularQueue_Init(&q, 128);

    // 生产者批量写入
    uint8_t src[10];
    for (uint8_t i = 0; i < 10; ++i)
        src[i] = i;
    uint16_t pushed = CircularQueue_Push(&q, src, 10);
    printf("Push %u bytes\n", pushed);

    // 消费者批量读取
    uint8_t dst[16] = {0};
    uint16_t popped = CircularQueue_Pop(&q, dst, 0xFFFF); // 读取所有
    printf("Pop %u bytes: ", popped);
    for (uint16_t i = 0; i < popped; ++i)
        printf("%d ", dst[i]);
    printf("\n");
    return 0;
}
#endif
