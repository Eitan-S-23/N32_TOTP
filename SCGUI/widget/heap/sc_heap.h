/**
 * @file   small_heap.h
 * @brief  Tiny, single-file heap manager
 * @note   Public Domain
 */
#ifndef SC_HEAP_H
#define SC_HEAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/**
 * @brief  Initialise the heap
 */
void heap_init(void);

/**
 * @brief  Allocate memory
 * @param  size  Requested bytes
 * @retval NULL  Failure
 */
void *heap_malloc(uint32_t size);

/**
 * @brief  Free memory
 * @param  ptr  Previous sh_malloc/sh_realloc result
 */
void heap_free(void *ptr);

/**
 * @brief  Resize allocation
 * @param  ptr   Previous result
 * @param  size  New bytes
 * @retval NULL  Failure (old block unchanged)
 */
void *heap_realloc(void *ptr, uint32_t size);

/**
 * @brief  Query heap usage
 * @param[out] total    Total bytes
 * @param[out] used     Used bytes
 * @param[out] percent  Used % (0..100)
 */
void heap_usage(uint32_t *total, uint32_t *used);

void heap_usage_printf(void);

#endif /* SMALL_HEAP_H */
