
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

// 头部类型：小内存用uint16（4字节头），内存池大于32K用uint32（8字节头）
typedef uint16_t Offset;

typedef struct BlockHeader
{
    Offset next;  /* 下一个块的偏移量（最低位为1表示已使用） */
    Offset prev;  /* 上一个块的偏移量 */
} BlockHeader ;

// 内存池配置参数
#define HEAP_TOTAL_SIZE     (2048)                      // 内存池总大小

#define HEADER_SIZE         (sizeof(BlockHeader))       // 内存块头部大小

#define ALIGNMENT           (4u)                        // 内存对齐字节数

#define ALIGN_UP(value)     (((value) + ALIGNMENT - 1u) & ~(ALIGNMENT - 1u)) // 向上对齐

// 块状态判断宏（减少重复条件判断）
#define IS_BLOCK_FREE(block)    ((block)->next & 1u) == 0u

// 内存池全局变量
static uint8_t heap_buffer[HEAP_TOTAL_SIZE] __attribute__((aligned(ALIGNMENT))); // 内存池缓冲区

static uint8_t *heap_start_addr=NULL;
static uint8_t *heap_end_addr=NULL;
static BlockHeader *g_freeList=NULL;   //当前空闲表头

/**
 * 偏移量转指针
 * @param offset 相对于内存池起始地址的偏移量
 * @return 偏移量对应的指针
 */
static inline uint8_t *offsetToPtr(uint32_t offset)
{
    return (uint8_t *)heap_buffer + offset;
}
/**
 * 指针转偏移量
 * @param ptr 内存池中的指针
 * @return 相对于内存池起始地址的偏移量
 */
static inline uint32_t ptrToOffset(void *ptr)
{
    return (uint8_t *)ptr - heap_buffer;
}

/**
 * 获取块的总大小（包含头部）
 * @param block 内存块指针
 * @return 块的总大小（字节）
 */
static inline uint32_t getBlockSize(BlockHeader  *block)
{
    return (block->next & ~1u) - ptrToOffset(block);
}

/**
 * 分割内存块（提取公共分割逻辑）
 * @param block 原始块
 * @param alloc_size 需要保留的大小（包含头部）
 * @return 分割出的新块（NULL表示无需分割）
 */
static BlockHeader  *splitBlock(BlockHeader  *block, uint32_t alloc_size)
{
    uint32_t orig_size = getBlockSize(block);
    // 剩余空间足够分割出新块（需包含头部和对齐）
    if (orig_size < alloc_size + HEADER_SIZE + ALIGNMENT)   return NULL;
    BlockHeader  *rem = (BlockHeader  *)((uint8_t *)block + alloc_size);
    rem->prev = ptrToOffset(block);    // 更新新块的链接关系
    rem->next = block->next & ~1u;     //
    ((BlockHeader  *)offsetToPtr(rem->next))->prev = ptrToOffset(rem);
    block->next = ptrToOffset(rem);    // 更新原始块的next
    return   rem;
}

/**
 * 合并相邻的空闲块
 * @param block 要合并的内存块
 */
static BlockHeader *merge_adjacent_blocks(BlockHeader  *block)
{
    if (!block) return NULL;
    BlockHeader *merged = block;
    // 合并下一个空闲块
    BlockHeader  *next_block = (BlockHeader  *)offsetToPtr(block->next & ~1u);
    if (IS_BLOCK_FREE(next_block) && (uint8_t *)next_block < heap_end_addr)
    {
        merged->next = next_block->next;
        ((BlockHeader  *)offsetToPtr(merged->next))->prev = ptrToOffset(merged);
    }
    // 合并上一个空闲块
    BlockHeader  *prev_block = (BlockHeader  *)offsetToPtr(block->prev);
    if (IS_BLOCK_FREE(prev_block) && prev_block != block)
    {
        prev_block->next = merged->next;
        ((BlockHeader  *)offsetToPtr(prev_block->next))->prev = ptrToOffset(prev_block);
        merged= prev_block;                      //返回合并后的主块
    }
    if ((uint8_t *)merged < (uint8_t *)g_freeList)
    {
        g_freeList = merged;                    //空闲指针向前移
    }
    return merged;
}

/**
 * 初始化内存池
 */
void heap_init(void)
{
    uint32_t heap_size;
    heap_start_addr = heap_buffer;
    heap_end_addr = heap_buffer + ALIGN_UP(sizeof(heap_buffer));
    heap_size = heap_end_addr - heap_start_addr;
    // 初始化头部块
    BlockHeader  *head_block = (BlockHeader  *)heap_start_addr;
    head_block->next = heap_size - HEADER_SIZE;
    head_block->prev = 0;
    // 初始化尾部块
    BlockHeader  *tail_block = (BlockHeader  *)offsetToPtr(head_block->next);
    tail_block->next = heap_size | 1u; // 尾部块标记为已使用
    tail_block->prev = head_block->next;

    g_freeList = head_block;
    printf("Heap init success! head->next: %u, tail->next: %u\n",
           head_block->next, tail_block->next&~1u);
}

/**
 * 分配内存
 * @param size 要分配的内存大小（字节）
 * @return 分配成功返回内存指针，失败返回NULL
 */
void *heap_malloc(uint32_t size)
{
    if (size == 0u) return NULL;
    if(heap_start_addr==NULL)
    {
        heap_init();
    }
    uint32_t alloc_size = ALIGN_UP(size + HEADER_SIZE); // 计算实际需要分配的块大小（用户数据 + 块头 + 对齐）
    uint32_t smallest_diff = 0xFFFFFFFF;
    BlockHeader  *best_block = NULL;
    // 遍历寻找最佳适配块
    for (BlockHeader  *block = (BlockHeader  *)g_freeList;
            (uint8_t *)block < heap_end_addr;
            block = (BlockHeader  *)offsetToPtr(block->next & ~1u))
    {
        if (IS_BLOCK_FREE(block))
        {
            uint32_t available_size = getBlockSize(block);
            if (available_size >= alloc_size)
            {
                if (available_size < smallest_diff)
                {
                    smallest_diff = available_size;
                    best_block = block;  //取最小内存块
                    break;               //快速分配
                }
            }
        }
    }
    if (best_block)
    {
        // 分割块（复用公共逻辑）
        BlockHeader  *new_block = splitBlock(best_block, alloc_size);
        if (new_block && g_freeList == best_block)
        {
            g_freeList = new_block;                  //指针向后移
        }
        best_block->next |= 1u;  // 标记为已用
        return (uint8_t *)best_block + HEADER_SIZE; // 返回用户区域
    }
    return NULL;
}

/**
 * 释放内存
 * @param ptr 要释放的内存指针
 */
void heap_free(void *ptr)
{
    if (ptr == NULL) return;                        // 处理NULL指针情况
    if (ptr < heap_start_addr || ptr > heap_end_addr) return;
    BlockHeader  *block = (BlockHeader  *)((uint8_t *)ptr - HEADER_SIZE);
    block->next &= ~1u;                      // 标记为空闲
    merge_adjacent_blocks(block);            // 释放后合并内存
}

/**
 * 重新分配内存
 * @param ptr 原内存指针
 * @param new_size 新的内存大小
 * @return 重新分配后的内存指针，失败返回NULL
 */
void *heap_realloc(void *ptr, uint32_t new_size)
{
    if (ptr == NULL) return heap_malloc(new_size);
    if (new_size == 0)
    {
        heap_free(ptr);
        return NULL;
    }
    uint32_t new_alloc_size = ALIGN_UP(new_size + HEADER_SIZE);
    BlockHeader  *block = (BlockHeader  *)((uint8_t *)ptr - HEADER_SIZE);
    uint32_t old_total_size = getBlockSize(block);
    // 1. 缩小：切尾（保持指针不动）
    if (new_alloc_size <= old_total_size)
    {
        BlockHeader  *new_block = splitBlock(block, new_alloc_size);  //
        if (new_block)
        {
            block->next |= 1u;                  // 标记为已使用
            merge_adjacent_blocks(new_block);   // 合并内存指针向前移
        }
        return ptr;
    }
    // 2. 尝试原地扩展
    BlockHeader  *next_block = (BlockHeader  *)offsetToPtr(block->next & ~1u);
    if (IS_BLOCK_FREE(next_block) && (uint8_t *)next_block < heap_end_addr)
    {
        //计算合并后的空间
        uint32_t merged_size = old_total_size + getBlockSize(next_block);
        if (merged_size >= new_alloc_size)
        {
            block->next = next_block->next;           // 合并当前块与下一块
            ((BlockHeader  *)offsetToPtr(block->next))->prev = ptrToOffset(block);
            // 分割合并后的块（复用公共逻辑）
            BlockHeader  *new_block = splitBlock(block, new_alloc_size);
            if (new_block&&g_freeList == next_block)
            {
                g_freeList = new_block;  //指针向后移
            }
            block->next |= 1u;    //标记为已用
            return ptr;
        }
    }
    // 3. 无法原地扩展：重新分配+复制+释放
    void *new_ptr = heap_malloc(new_size);
    if (new_ptr)
    {
        memcpy(new_ptr, ptr, old_total_size - HEADER_SIZE);
        heap_free(ptr);
    }
    return new_ptr;
}

/**
 * 获取内存池使用情况
 * @param total 总可用内存（除去头部）
 * @param used  已使用内存（除去头部）
 */
void heap_usage(uint32_t *total, uint32_t *used)
{
    uint32_t used_size = 0u;
    uint32_t header_total = 0u;
    uint32_t heap_size=heap_end_addr-heap_start_addr;
    printf("======usage======== \n");
    for (BlockHeader  *block = (BlockHeader  *)heap_start_addr;
            (uint8_t *)block < heap_end_addr;
            block = (BlockHeader  *)offsetToPtr(block->next & ~1u))
    {
        if(block->next & 1u)
        {
            used_size += getBlockSize(block);
            header_total += HEADER_SIZE;
//            printf("0x%04x  size=%d \n",
//                    ptrToOffset(block)+ HEADER_SIZE,
//                    getBlockSize(block)-HEADER_SIZE);
        }
    }
    if (total)  *total  = heap_size - header_total;
    if (used)   *used   = used_size - header_total;
}


void heap_usage_printf(void)
{
    uint32_t total, used;
    heap_usage(&total, &used);
    printf("Heap: %u/%u bytes (%u%%)\n", used, total, used * 100U / total);
}
