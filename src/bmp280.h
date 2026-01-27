#ifndef SRC_BMP280_H
#define SRC_BMP280_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

typedef struct BMP280Struct *BMP280;

/**
 * @brief Gets called in @ref bmp280_create to get memory for a BMP280 instance.
 *
 * The implementation of this function should return a pointer to memory of size sizeof(struct BMP280Struct). All
 * private data for the created BMP280 instance will reside in that memory.
 *
 * The implementation of this function should be defined in a separate source file. That source file should include
 * bmp280_private.h, which contains the definition of struct BMP280Struct. The implementation of this function then
 * knows at compile time the size of memory that it needs to provide.
 *
 * This function will be called as many times as @ref bmp280_create is called (given that all parameters passed to @ref
 * bmp280_create are valid). The implementation should be capable of returning memory for that many distinct instances.
 *
 * Implementation example - two statically allocated instances:
 * ```
 * void *get_inst_buf(void *user_data) {
 *     static struct BMP280Struct instances[2];
 *     static size_t idx = 0;
 *     return (idx < 2) ? (&(instances[idx++])) : NULL;
 * }
 * ```
 *
 * If the application uses dynamic memory allocation, another implementation option is to allocate sizeof(struct
 * BMP280Struct) bytes dynamically.
 *
 * @param user_data When this function is called, this parameter will be equal to the get_inst_buf_user_data field in
 * the BMP280InitCfg passed to @ref bmp280_create.
 *
 * @return void * Pointer to instance buffer of size sizeof(struct BMP280Struct). If failed to get memory, should return
 * NULL.
 */
typedef void *(*BMP280GetInstBuf)(void *user_data);

typedef enum {
    BMP280_RESULT_CODE_OK = 0,
    BMP280_RESULT_CODE_NO_MEM,
    BMP280_RESULT_CODE_INVAL_ARG,
} BMP280ResultCode;

typedef struct {
    BMP280GetInstBuf get_inst_buf;
    void *get_inst_buf_user_data;
} BMP280InitCfg;

uint8_t bmp280_create(BMP280 *const inst, const BMP280InitCfg *const cfg);

#ifdef __cplusplus
}
#endif

#endif /* SRC_BMP280_H */
