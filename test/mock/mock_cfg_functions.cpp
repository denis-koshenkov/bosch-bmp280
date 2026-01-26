#include "CppUTestExt/MockSupport.h"
#include "mock_cfg_functions.h"

void *mock_bmp280_get_inst_buf(void *user_data)
{
    mock().actualCall("mock_bmp280_get_inst_buf").withParameter("user_data", user_data);
    return mock().pointerReturnValue();
}
