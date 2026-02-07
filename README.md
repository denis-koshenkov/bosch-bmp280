# Bosch BMP280 Driver
Non-blocking driver for Bosch BMP280 pressure sensor in C language. It is intended to be used in embedded systems that perform asynchronous IO (I2C/SPI) operations.

This driver code has zero blocking, given that IO transactions can be performed asynchronously.

This driver was developed with Test-Driven Development (TDD) and tested using CppUTest framework.

# Integration Details
Add the following to your build:
- `src/bmp280.c` source file
- `src` directory as include directory

# Usage
In order to use the driver, you need to implement the folllowing functions:
```c
void *bmp280_get_inst_buf(void *user_data);
void bmp280_write_reg(uint8_t addr, uint8_t reg_val, void *user_data, BMP280_IOCompleteCb cb, void *cb_user_data);
void bmp280_read_regs(uint8_t start_addr, size_t num_regs, uint8_t *data, void *user_data, BMP280_IOCompleteCb cb, void *cb_user_data);
void bmp280_start_timer(uint32_t duration_ms, void *user_data, BMP280TimerExpiredCb cb, void *cb_user_data);
```
The function names can be different - you will pass function pointers of your implementations to the driver.

## Function Implementation Guidelines

### Get inst buf
`bmp280_get_inst_buf` must return a memory buffer that will be used for private data of a BMP280 instance. The memory buffer must remain valid as long as the instance is being used.

This function is called once per instance as a part of `bmp280_create`.

There are two main ways to implement this function:
1) Return pointer to statically allocated memory. If you need one driver instance:
```c
void *bmp280_get_inst_buf(void *user_data) {
    static struct BMP280Struct inst;
    return &inst;
}
```
If you need several driver instances:
```c
#define BMP280_NUM_INSTANCES 2

void *bmp280_get_inst_buf(void *user_data) {
    static struct BMP280Struct instances[BMP280_NUM_INSTANCES];
    static size_t idx = 0;
    return (idx < BMP280_NUM_INSTANCES) ? (&(instances[idx++])) : NULL;
}
```

2) Dynamically allocate memory for an instance:
```c
void *bmp280_get_inst_buf(void *user_data) {
    return malloc(sizeof(struct BMP280Struct));
}
```

It depends on the application whether dynamic memory allocation is allowed/desired.

If you are using option 2, then you need to free the memory when the driver instance is destroyed. This is currently not supported by the driver - in the future, `bmp280_destroy` function can be implemented that executes a user-provided callback and passes the instance memory as an argument.

`struct BMP280Struct` is a data type that defines the private data of a BMP280 instance. It is defined in the `bmp280_private.h` file.

That header must not be included by driver users. The only exception is a module that implements `bmp280_get_inst_buf`, because the implementation of that function needs to know about `struct BMP280Struct`, so that it knows the size of the memory buffer it needs to return.

It is recommended to define the implementation of `bmp280_get_inst_buf` in a separate .c file and do `#include "bmp280_private.h"` only in that .c file.

This way, other modules that interact with this driver do not have to include `bmp280_private.h`, which means they cannot access private data of the driver instance directly.

### Read/Write reg(s)
`bmp280_write_reg` must write `reg_val` to BMP280 register with address `addr`. `user_data` parameter will be equal to the `write_reg_user_data` pointer from the init config passed to `bmp280_create`.

`bmp280_read_regs` must read `num_regs` BMP280 registers starting at address `addr`, and write the register values to `data` pointer. `user_data` parameter will be equal to the `read_regs_user_data` pointer from the init config passed to `bmp280_create`.

The implementations can use either I2C or SPI. The implementations must be asynchronous. Once the IO operation is complete, the implementations must invoke `cb`.

`cb` has two parameters: `io_rc` and `user_data`.

When invoking `cb`, the implementation must pass `cb_user_data` from `bmp280_write/read_reg(s)` to the `user_data` parameter of `cb`.

`io_rc` parameter of `cb` signifies whether the IO operation was successful. The implementation must pass `BMP280_IO_RESULT_CODE_OK` if the transaction was successful. Pass `BMP280_IO_RESULT_CODE_ERR` if the transaction failed.

**Important rule**: `cb` must be invoked from the same thread/context as all other public functions of this driver. See [this section](#io-complete-and-timer-expired-callbacks-execution-context-rule) for more details.

### Start Timer
`bmp280_start_timer` must start a one-shot timer that invokes `cb` after at least `duration_ms` pass. `user_data` parameter of `bmp280_start_timer` will be equal to the `start_timer_user_data` pointer from the init config passed to `bmp280_create`.

`cb` has one parameter: `user_data`. The implementation must pass the `cb_user_data` parameter of `bmp280_start_timer` to `cb` as `user_data` parameter.

**Important rule**: `cb` must be invoked from the same thread/context as all other public driver functions of this driver. See [this section](#io-complete-and-timer-expired-callbacks-execution-context-rule) for more details.

### IO Complete and Timer Expired Callbacks Execution Context Rule
`bmp280_write_reg`, `bmp280_read_regs`, and `bmp280_start_timer` are asynchronous functions that need to execute a callback once the IO transaction is complete or the timer expired.

**In order to prevent race conditions, these callbacks must be executed from the same context as other BMP280 public driver functions.**

Imagine that you are using the asynchronous I2C/SPI driver from the MCU vendor to implement `bmp280_write_reg` and `bmp280_read_regs`. The driver executes a user-defined callback once the IO transaction is complete.

It is often the case that this user-defined callback is executed from ISR context. In that case, the implementation of `bmp280_write_reg`/`bmp280_read_regs` **must not directly invoke `cb`** from that user-defined callback.

This would result in `cb` being called from ISR context and may cause race conditions in the BMP280 driver.

It is the responsibility of the driver user to ensure that all public driver functions and all callbacks are executed from the same context.

An example implementation is to have a central event queue - a thread that is blocked on a message queue. Whenever there is an event that needs to be handled in the system, an event is pushed to that message queue.

The thread then processes the event by calling an event handler for that event.

Every time an IO operation is complete or a timer expires, an event can be pushed to the central event queue. The thread will then invoke the handlers for these events - which will execute the `BMP280_IOCompleteCb` and `BMP280TimerExpiredCb` callbacks.

This way, these callbacks are always invoked from the same execution context. Of course, all public BMP280 driver functions also need to be executed from the same event queue thread for this to work.

This is just an example - any implementation works as long as **all public BMP280 driver functions and callbacks are executed from the same context**.

## Driver Usage Example
This example creates a BMP280 driver instance, enables temperature and pressure measurements, reads out calibration trimmings, and reads out one measurement in forced mode.

There is a lot of blocking in this example - this is probably not the way to go in an asynchronous system. The example is done this way to demonstrate driver usage.
```c
static bool set_temp_oversampling_complete = false;
static void set_temp_oversampling_cb(uint8_t rc, void *user_data) {
    if (rc == BMP280_RESULT_CODE_OK) {
        set_temp_oversampling_complete = true;
    }
}

static bool set_pres_oversampling_complete = false;
static void set_pres_oversampling_cb(uint8_t rc, void *user_data) {
    if (rc == BMP280_RESULT_CODE_OK) {
        set_pres_oversampling_complete = true;
    }
}

static bool init_meas_complete = false;
static void init_meas_cb(uint8_t rc, void *user_data) {
    if (rc == BMP280_RESULT_CODE_OK) {
        init_meas_complete = true;
    }
}

static bool read_meas_complete = false;
static void read_meas_cb(uint8_t rc, void *user_data) {
    if (rc == BMP280_RESULT_CODE_OK) {
        read_meas_complete = true;
    }
}

BMP280InitCfg cfg = {
    /* Your implementation of bmp280_get_inst_buf */
    .get_inst_buf = bmp280_get_inst_buf,
    .get_inst_buf_user_data = NULL, // Optional
    /* Your implementation of bmp280_read_regs */
    .read_regs = bmp280_read_regs,
    .read_regs_user_data = NULL, // Optional
    /* Your implementation of bmp280_write_reg */
    .write_reg = bmp280_write_reg,
    .write_reg_user_data = NULL, // Optional
    /* Your implementation of bmp280_start_timer */
    .start_timer = bmp280_start_timer,
    .start_timer_user_data = NULL, // Optional
};
BMP280 inst;
uint8_t rc = bmp280_create(&inst, &cfg);
if (rc != BMP280_RESULT_CODE_OK) {
    // Error handling
}

/* Enable temperature measurement */
rc = bmp280_set_temp_oversampling(inst, BMP280_OVERSAMPLING_1, set_temp_oversampling_cb, NULL);
if (rc != BMP280_RESULT_CODE_OK) {
    // Error handling
}
/* Busy wait until set temp oversampling is complete. This is not the way to go in an async system. This is an example to showcase that set_temp_oversampling_cb must execute before anything else can be done. Same logic applies to all busy waits below. */
while (!set_temp_oversampling_complete) {
    ;
}

/* Enable pressure measurement */
rc = bmp280_set_pres_oversampling(inst, BMP280_OVERSAMPLING_1, set_pres_oversampling_cb, NULL);
if (rc != BMP280_RESULT_CODE_OK) {
    // Error handling
}
while (!set_pres_oversampling_complete) {
    ;
}

/* Initialize measurements. Reads out calibration trimmings from BMP280 registers and makes a RAM copy. Must be done prior to calling bmp280_read_meas_forced_mode. */
rc = bmp280_init_meas(inst, init_meas_cb, NULL);
if (rc != BMP280_RESULT_CODE_OK) {
    // Error handling
}
while (!init_meas_complete) {
    ;
}

/* Read out one measurement in forced mode */

/* Oversampling setting is set to 1 for both temperature and pressure. Max measurement time for these settings is 6.4 ms (according to section 3.8.1 in the datasheet, p. 18). Round it up to 7 ms to make sure that measurement is ready when we read it out. */ 
uint32_t meas_time_ms = 7;
BMP280Meas meas;
rc = bmp280_read_meas_forced_mode(inst, BMP280_MEAS_TYPE_TEMP_AND_PRES, meas_time_ms, &meas, read_meas_cb, NULL);
while (!read_meas_complete) {
    ;
}

/* Measurement is now available in meas.temperature and meas.pressure */
```

# Running Tests
Follow these steps in order to run all unit tests for the driver source code.

Prerequisites:
- CMake
- C compiler (e.g. gcc)
- C++ compiler (e.g. g++)

Steps:
1. Clone this repository
1. Fetch the CppUTest submodule:
    ```
    git submodule init
    git submodule update
    ```
1. Build and run the tests:
    ```
    ./run_tests.sh
    ```
