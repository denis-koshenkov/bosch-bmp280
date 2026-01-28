#include "CppUTestExt/MockSupport.h"
#include "mock_cfg_functions.h"

void *mock_bmp280_get_inst_buf(void *user_data)
{
    mock().actualCall("mock_bmp280_get_inst_buf").withParameter("user_data", user_data);
    return mock().pointerReturnValue();
}

void mock_bmp280_read_regs(uint8_t start_addr, size_t num_regs, uint8_t *data, void *user_data, BMP280_IOCompleteCb cb,
                           void *cb_user_data)
{
    BMP280_IOCompleteCb *cb_p = (BMP280_IOCompleteCb *)mock().getData("readRegsCompleteCb").getPointerValue();
    void **cb_user_data_p = (void **)mock().getData("readRegsCompleteCbUserData").getPointerValue();
    *cb_p = cb;
    *cb_user_data_p = cb_user_data;

    mock()
        .actualCall("mock_bmp280_read_regs")
        .withParameter("start_addr", start_addr)
        .withParameter("num_regs", num_regs)
        .withOutputParameter("data", data)
        .withParameter("user_data", user_data)
        .withParameter("cb", cb)
        .withParameter("cb_user_data", cb_user_data);
}

void mock_bmp280_write_reg(uint8_t addr, uint8_t reg_val, void *user_data, BMP280_IOCompleteCb cb, void *cb_user_data)
{
    BMP280_IOCompleteCb *cb_p = (BMP280_IOCompleteCb *)mock().getData("writeRegCompleteCb").getPointerValue();
    void **cb_user_data_p = (void **)mock().getData("writeRegCompleteCbUserData").getPointerValue();
    *cb_p = cb;
    *cb_user_data_p = cb_user_data;

    mock()
        .actualCall("mock_bmp280_write_reg")
        .withParameter("addr", addr)
        .withParameter("reg_val", reg_val)
        .withParameter("user_data", user_data)
        .withParameter("cb", cb)
        .withParameter("cb_user_data", cb_user_data);
}

void mock_bmp280_start_timer(uint32_t duration_ms, void *user_data, BMP280TimerExpiredCb cb, void *cb_user_data)
{
    BMP280TimerExpiredCb *cb_p = (BMP280TimerExpiredCb *)mock().getData("timerExpiredCb").getPointerValue();
    void **cb_user_data_p = (void **)mock().getData("timerExpiredCbUserData").getPointerValue();
    *cb_p = cb;
    *cb_user_data_p = cb_user_data;

    mock()
        .actualCall("mock_bmp280_start_timer")
        .withParameter("duration_ms", duration_ms)
        .withParameter("user_data", user_data)
        .withParameter("cb", cb)
        .withParameter("cb_user_data", cb_user_data);
}
