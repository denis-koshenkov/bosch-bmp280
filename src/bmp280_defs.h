#ifndef SRC_BMP280_DEFS_H
#define SRC_BMP280_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * @brief BMP280 definitions.
 *
 * These definitions are placed in a separate header, so that they can be included both by bmp280_private.h and
 * bmp280.h. They need to be a part of the public interface, but they also need to be included in bmp280_private.h,
 * because they contain data types that are present in the struct BMP280Struct definition.
 */

/** Result codes describing outcomes of a IO transaction. */
typedef enum {
    /** Successful IO transaction. */
    BMP280_IO_RESULT_CODE_OK = 0,
    /** IO transaction failed. */
    BMP280_IO_RESULT_CODE_ERR,
} BMP280_IOResultCode;

/**
 * @brief Callback type to execute when a BMP280 IO transaction is complete.
 *
 * @param[in] rc Pass one of the values from @ref BMP280_IOResultCode to describe the transaction result.
 * @param[in] user_data The caller must pass user_data parameter that the BMP280 driver passed to @ref BMP280ReadReg or
 * @ref BMP280WriteReg.
 */
typedef void (*BMP280_IOCompleteCb)(uint8_t io_rc, void *user_data);

/**
 * @brief Read BMP280 registers.
 *
 * For SPI, bit 7 (MSb) of @p start_addr must be set to 1 to indicate that a read must be performed. The implementation
 * of this function must take care of that if SPI is being used.
 *
 * This is not done in the driver logic, because the implementation of this function can also use I2C, and in that case,
 * the full register address is necessary.
 *
 * @param[in] start_addr Register address of the first register to be read. If @p num_regs is greater than 1, the
 * implementation must read consecutive registers. For example, if @p start_addr is 0x42, and @p num_regs is 3, the
 * implementation must read registers 0x42, 0x43, and 0x44.
 * @param[in] num_regs Number of registers to read.
 * @param[out] data Register values are written to this parameter. Must be a buffer of size @p num_regs bytes (each
 * register value is 1 byte).
 * @param[in] user_data This parameter will be equal to read_regs_user_data field of init cfg passed to @ref
 * bmp280_create.
 * @param[in] cb Callback that must be executed once the register read is complete. Pass @ref BMP280_IO_RESULT_CODE_OK
 * to io_rc parameter if registers were read successfully. Otherwise, pass @ref BMP280_IO_RESULT_CODE_ERR.
 * @param[in] cb_user_data This argument must be passed to the user_data parameter of @p cb.
 */
typedef void (*BMP280ReadRegs)(uint8_t start_addr, size_t num_regs, uint8_t *data, void *user_data,
                               BMP280_IOCompleteCb cb, void *cb_user_data);

#ifdef __cplusplus
}
#endif

#endif /* SRC_BMP280_DEFS_H */
