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
