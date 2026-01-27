#ifndef SRC_BMP280_H
#define SRC_BMP280_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

#include "bmp280_defs.h"

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

/**
 * @brief Callback type to execute when the BMP280 driver finishes an operation.
 *
 * @param rc Result code that indicates success or the reason for failure. One of @ref BMP280ResultCode.
 * @param user_data User data.
 */
typedef void (*BMP280CompleteCb)(uint8_t rc, void *user_data);

typedef enum {
    BMP280_RESULT_CODE_OK = 0,
    BMP280_RESULT_CODE_INVAL_ARG,
    BMP280_RESULT_CODE_NO_MEM,
    BMP280_RESULT_CODE_IO_ERR,
} BMP280ResultCode;

typedef struct {
    BMP280GetInstBuf get_inst_buf;
    void *get_inst_buf_user_data;
    BMP280ReadRegs read_regs;
    void *read_regs_user_data;
} BMP280InitCfg;

uint8_t bmp280_create(BMP280 *const inst, const BMP280InitCfg *const cfg);

/**
 * @brief Read BMP280 chip id from the device.
 *
 * Once the chip id is read out or an error occurs, @p cb is executed. "rc" parameter of @p cb indicates success or
 * reason for failure:
 * - @ref BMP280_RESULT_CODE_OK Successfully read chip id.
 * - @ref BMP280_RESULT_CODE_IO_ERR IO transaction to read the chip id failed.
 *
 * @param[in] self BMP280 instance created by @ref bmp280_create.
 * @param[out] chip_id Chip id is written to this parameter in case of success.
 * @param[in] cb Callback to execute once chip id has been read out.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BMP280_RESULT_CODE_OK Successfully initiated chip id readout.
 * @retval BMP280_RESULT_CODE_INVAL_ARG @p self is NULL or @p chip_id is NULL.
 */
uint8_t bmp280_get_chip_id(BMP280 self, uint8_t *const chip_id, BMP280CompleteCb cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* SRC_BMP280_H */
