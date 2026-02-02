#ifndef SRC_BMP280_PRIVATE_H
#define SRC_BMP280_PRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

/* This header should be included only by the user module implementing the BMP280GetInstBuf callback which is a
 * part of InitCfg passed to bmp280_create. All other user modules are not allowed to include this header, because
 * otherwise they would know about the BMP280Struct struct definition and can manipulate private data of a BMP280
 * instance directly. */

/** Must be equal to the maximum number of registers that can be read in a single call to read_regs. 24 because
 * calibration values occupy 24 registers, and they are read out in one transaction. */
#define BMP280_READ_BUF_SIZE 24

typedef struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
} CalibTemp;

typedef struct {
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
} CalibPres;

/* Defined in a separate header, so that both bmp280.c and the user module implementing BMP280GetInstBuf callback
 * can include this header. The user module needs to know sizeof(struct BMP280Struct), so that it knows the size of
 * BMP280 instances at compile time. This way, it has an option to allocate a static array with size equal to the
 * required number of instances. */
struct BMP280Struct {
    /** User-defined function that performs BMP280 register read. */
    BMP280ReadRegs read_regs;
    /** User data to pass to read_regs. */
    void *read_regs_user_data;
    /** User-defined function that performs BMP280 register write. */
    BMP280WriteReg write_reg;
    /** User data to pass to write_reg. */
    void *write_reg_user_data;
    /** User-defined function to start a timer that schedules a callback execution. */
    BMP280StartTimer start_timer;
    /** User data to pass to start_timer function. */
    void *start_timer_user_data;
    /** Callback to execute once the current sequence is complete. */
    BMP280CompleteCb complete_cb;
    /** User data to pass to complete_cb. */
    void *complete_cb_user_data;
    /** Address to write the resulting measurements to. */
    BMP280Meas *meas;
    /** Timer period to use for read_meas_forced_mode. */
    uint32_t timer_period_ms;
    /** Measurement type of the current sequence. One of @ref BMP280MeasType. */
    uint8_t meas_type;
    /** Buffer to use for read reg operations. */
    uint8_t read_buf[BMP280_READ_BUF_SIZE];
    /** Temperature calibration values. Used for converting raw temperature values to DegC. */
    CalibTemp calib_temp;
    /** Pressure calibration values. Used for converting raw pressure values to Pa. */
    CalibPres calib_pres;
};

#ifdef __cplusplus
}
#endif

#endif /* SRC_BMP280_PRIVATE_H */
