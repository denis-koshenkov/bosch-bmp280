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
    /** Something went wrong in the code of this driver. */
    BMP280_RESULT_CODE_DRIVER_ERR,
    BMP280_RESULT_CODE_INVAL_USAGE,
} BMP280ResultCode;

/* There is no option to read out just pressure, because temperature value is needed to convert raw pressure values
 * to hPa, so temperature has to be read out either way. If only pressure value is needed, use
 * BMP280_MEAS_TYPE_TEMP_AND_PRES and ignore the temperature value. */
typedef enum {
    /** Read out only temperature. */
    BMP280_MEAS_TYPE_ONLY_TEMP,
    /** Read out both temperature and pressure. */
    BMP280_MEAS_TYPE_TEMP_AND_PRES,
} BMP280MeasType;

typedef enum {
    BMP280_OVERSAMPLING_SKIPPED = 0,
    BMP280_OVERSAMPLING_1 = 1,
    BMP280_OVERSAMPLING_2 = 2,
    BMP280_OVERSAMPLING_4 = 3,
    BMP280_OVERSAMPLING_8 = 4,
    BMP280_OVERSAMPLING_16 = 5,
} BMP280Oversampling;

typedef enum {
    BMP280_FILTER_COEFF_FILTER_OFF = 0,
    BMP280_FILTER_COEFF_2 = 1,
    BMP280_FILTER_COEFF_4 = 2,
    BMP280_FILTER_COEFF_8 = 3,
    BMP280_FILTER_COEFF_16 = 4,
} BMP280FilterCoeff;

typedef enum {
    /** Disable SPI 3 wire mode - sets SPI 4 wire mode. */
    BMP280_SPI_3_WIRE_DIS = 0,
    /** Sets SPI 3 wire mode. */
    BMP280_SPI_3_WIRE_EN = 1,
} BMP280Spi3Wire;

typedef struct {
    /** Temperature in degrees Celsius, resolution is 0.01. Output value "5123" equals 51.23 degrees Celsius. */
    int32_t temperature;
    /** Pressure in Pa in Q24.8 format (24 integer bits and 8 fractional bits). Output value "24674867" represents
     * 24674867/256 = 96386.2 Pa = 963.862 hPa. */
    uint32_t pressure;
} BMP280Meas;

typedef struct {
    /** User-defined function to get memory buffer for BMP280 instance. Cannot be NULL. Called once during @ref
     * bmp280_create. */
    BMP280GetInstBuf get_inst_buf;
    /** User data to pass to get_inst_buf function. */
    void *get_inst_buf_user_data;
    /** User-defined function to perform BMP280 register read over I2C or SPI. Cannot be NULL. */
    BMP280ReadRegs read_regs;
    /** User data to pass to read_regs function. */
    void *read_regs_user_data;
    /** User-defined function to perform BMP280 register write over I2C or SPI. Cannot be NULL. */
    BMP280WriteReg write_reg;
    /** User data to pass to write_reg function. */
    void *write_reg_user_data;
    /** User-defined function to start a timer that schedules a callback execution. Cannot be NULL. */
    BMP280StartTimer start_timer;
    /** User data to pass to start_timer function. */
    void *start_timer_user_data;
} BMP280InitCfg;

/**
 * @brief Create a BMP280 instance.
 *
 * @param[out] inst Created instance is written to this parameter in case of success.
 * @param[in] cfg Initialization config.
 *
 * @retval BMP280_RESULT_CODE_OK Successfully created BMP280 instance.
 * @retval BMP280_RESULT_CODE_INVAL_ARG @p inst is NULL, or @p cfg is not a valid init cfg.
 */
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

/**
 * @brief Reset BMP280 and wait for the duration of power up sequence.
 *
 * Resets BMP280 by writing to the reset register and waits for 2 ms to give the device time to perform the reset. 2 ms
 * is specified in the datasheet as the time of startup procedure which includes the power on reset sequence. This means
 * the power on reset sequence itself cannot take longer than 2 ms, so it is guaranteed that the device has finished
 * resetting after 2 ms.
 *
 * Once the reset is complete or an error occurs, @p cb is executed. "rc" parameter of @p cb indicates success or
 * reason for failure:
 * - @ref BMP280_RESULT_CODE_OK Successfully reset the device.
 * - @ref BMP280_RESULT_CODE_IO_ERR IO transaction to write the reset register failed.
 *
 * @param[in] self BMP280 instance created by @ref bmp280_create.
 * @param[in] cb Callback to execute once reset with delay is complete.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BMP280_RESULT_CODE_OK Successfully initiated reset with delay sequence.
 * @retval BMP280_RESULT_CODE_INVAL_ARG @p self is NULL.
 */
uint8_t bmp280_reset_with_delay(BMP280 self, BMP280CompleteCb cb, void *user_data);

/**
 * @brief Read out temperature and pressure calibration values from the device.
 *
 * This function must be called once per instance before any measurement readout functions are called, such as @ref
 * bmp280_read_meas_forced_mode.
 *
 * This function reads out temperature and pressure calibration trimmings from BMP280 registers and saves them to RAM.
 * These calibration values are then used to convert raw measurement register values into measurements in applicable
 * units - DegC or Pa.
 *
 * Once calibration values are read out or an error occurrs, @p cb is executed. "rc" parameter of @p cb indicates
 * success or reason for failure:
 * - @ref BMP280_RESULT_CODE_OK Successfully read out calibration values.
 * - @ref BMP280_RESULT_CODE_IO_ERR IO transaction to read calibration values failed.
 *
 * @param[in] self BMP280 instance created by @ref bmp280_create.
 * @param[in] cb Callback to execute once calibration values are read out.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BMP280_RESULT_CODE_OK Successfully initiated readout of calibration values.
 * @retval BMP280_RESULT_CODE_INVAL_ARG @p self is NULL.
 */
uint8_t bmp280_init_meas(BMP280 self, BMP280CompleteCb cb, void *user_data);

/**
 * @brief Perform one temperature and/or pressure measurement in forced mode.
 *
 * @pre @ref bmp280_init_meas has been called for this BMP280 instance.
 *
 * This function performs the following steps:
 * 1. Set mode to forced mode in ctrl_meas register.
 * 2. Wait for @p meas_time_ms ms.
 * 3. Read temperature and/or pressure measurement from the registers and convert them to DegC/Pa units.
 *
 * If @p meas_type is BMP280_MEAS_TYPE_ONLY_TEMP, only temperature measurement is read out (3 registers). In this case,
 * "pressure" field of @p meas has undefined value and should not be used.
 *
 * If @p meas_type is BMP280_MEAS_TYPE_TEMP_AND_PRES, both temperature and pressure measurements are read out (6
 * registers). Both "temperature" and "pressure" fields of @p meas are then valid.
 *
 * The choice of @p meas_time_ms depends on the oversampling settings set in ctrl_meas register. The datasheet (p. 18)
 * provides measurement times for different oversampling settings. The maximum measurement time for a given set of
 * settings is a good choice for @p meas_time_ms parameter.
 *
 * The datasheet does not provide measurement times for all possible combinations of oversampling settings. Because of
 * this, the choice of @p meas_time_ms is left to the user of the driver.
 *
 * Once measurement is complete or an error occurrs, @p cb is executed. "rc" parameter of @p cb indicates
 * success or reason for failure:
 * - @ref BMP280_RESULT_CODE_OK Successfully completed the measurement.
 * - @ref BMP280_RESULT_CODE_IO_ERR One of the IO transactions failed.
 * - @ref BMP280_RESULT_CODE_DRIVER_ERR Something went wrong in the code of this driver.
 *
 * @param[in] self BMP280 instance created by @ref bmp280_create.
 * @param[in] meas_type Measurement type - whether to read out only temperature, or both temperature and pressure. Must
 * be one of @ref BMP280MeasType.
 * @param[in] meas_time_ms Number of milliseconds to wait between setting forced mode and reading temperature/pressure
 * registers. Cannot be 0.
 * @param[out] meas Measurement result is written to this parameter. "pressure" field is only valid if @p meas_type is
 * BMP280_MEAS_TYPE_TEMP_AND_PRES. Cannot be NULL.
 * @param[in] cb Callback to execute once measurement is complete.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BMP280_RESULT_CODE_OK Successfully initiated the measurement.
 * @retval BMP280_RESULT_CODE_INVAL_ARG @p self is NULL, @p meas is NULL, @p meas_type is not one of @ref
 * BMP280MeasType, or @p meas_time is 0.
 * @retval BMP280_RESULT_CODE_INVAL_USAGE @ref bmp280_init_meas has not been called for this BMP280 instance.
 */
uint8_t bmp280_read_meas_forced_mode(BMP280 self, uint8_t meas_type, uint32_t meas_time_ms, BMP280Meas *const meas,
                                     BMP280CompleteCb cb, void *user_data);

/**
 * @brief Set temperature oversampling option.
 *
 * Once oversampling option is set or an error occurrs, @p cb is executed. "rc" parameter of @p cb indicates
 * success or reason for failure:
 * - @ref BMP280_RESULT_CODE_OK Successfully set the oversampling option.
 * - @ref BMP280_RESULT_CODE_IO_ERR One of the IO transactions failed.
 *
 * @param[in] self BMP280 instance created by @ref bmp280_create.
 * @param[in] oversampling Oversampling option to set. One of @ref BMP280Oversampling.
 * @param[in] cb Callback to execute once oversampling option is set.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BMP280_RESULT_CODE_OK Successfully initiated setting the oversampling option.
 * @retval BMP280_RESULT_CODE_INVAL_ARG @p self is NULL, or @p oversampling is not one of @ref BMP280Oversampling.
 */
uint8_t bmp280_set_temp_oversampling(BMP280 self, uint8_t oversampling, BMP280CompleteCb cb, void *user_data);

/**
 * @brief Set pressure oversampling option.
 *
 * Once oversampling option is set or an error occurrs, @p cb is executed. "rc" parameter of @p cb indicates
 * success or reason for failure:
 * - @ref BMP280_RESULT_CODE_OK Successfully set the oversampling option.
 * - @ref BMP280_RESULT_CODE_IO_ERR One of the IO transactions failed.
 *
 * @param[in] self BMP280 instance created by @ref bmp280_create.
 * @param[in] oversampling Oversampling option to set. One of @ref BMP280Oversampling.
 * @param[in] cb Callback to execute once oversampling option is set.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BMP280_RESULT_CODE_OK Successfully initiated setting the oversampling option.
 * @retval BMP280_RESULT_CODE_INVAL_ARG @p self is NULL, or @p oversampling is not one of @ref BMP280Oversampling.
 */
uint8_t bmp280_set_pres_oversampling(BMP280 self, uint8_t oversampling, BMP280CompleteCb cb, void *user_data);

/**
 * @brief Set filter coefficient option.
 *
 * Once filter coefficient is set or an error occurrs, @p cb is executed. "rc" parameter of @p cb indicates
 * success or reason for failure:
 * - @ref BMP280_RESULT_CODE_OK Successfully set the filter coefficient.
 * - @ref BMP280_RESULT_CODE_IO_ERR One of the IO transactions failed.
 *
 * @param[in] self BMP280 instance created by @ref bmp280_create.
 * @param[in] filter_coeff Filter coefficient option to set. One of @ref BMP280FilterCoeff.
 * @param[in] cb Callback to execute once filter coefficient is set.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BMP280_RESULT_CODE_OK Successfully initiated setting the filter coefficient.
 * @retval BMP280_RESULT_CODE_INVAL_ARG @p self is NULL, or @p filter_coeff is not one of @ref BMP280FilterCoeff.
 */
uint8_t bmp280_set_filter_coefficient(BMP280 self, uint8_t filter_coeff, BMP280CompleteCb cb, void *user_data);

/**
 * @brief Enable or disable SPI 3 wire interface mode.
 *
 * Once SPI 3 wire mode is set or an error occurrs, @p cb is executed. "rc" parameter of @p cb indicates
 * success or reason for failure:
 * - @ref BMP280_RESULT_CODE_OK Successfully set the SPI 3 wire mode.
 * - @ref BMP280_RESULT_CODE_IO_ERR One of the IO transactions failed.
 *
 * @param[in] self BMP280 instance created by @ref bmp280_create.
 * @param[in] spi_3_wire Whether to enable or disable SPI 3 wire mode. One of @ref BMP280Spi3Wire.
 * @param[in] cb Callback to execute once SPI 3 wire mode is set.
 * @param[in] user_data User data to pass to @p cb.
 *
 * @retval BMP280_RESULT_CODE_OK Successfully initiated setting the SPI 3 wire mode.
 * @retval BMP280_RESULT_CODE_INVAL_ARG @p self is NULL, or @p spi_3_wire is not one of @ref BMP280Spi3Wire.
 */
uint8_t bmp280_set_spi_3_wire_interface(BMP280 self, uint8_t spi_3_wire, BMP280CompleteCb cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* SRC_BMP280_H */
