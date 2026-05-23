#include "imu.h"
#include "motor.h"
#include "ti_msp_dl_config.h"

#define IMU_HEAD0           0x55u
#define IMU_HEAD1           0x55u
#define IMU_CMD_EULER       0x06u
#define IMU_PAYLOAD_MAX     16u
#define IMU_CAPTURE_LEN     16u
#define IMU_FRAME_LEN       21u

/*
 * ATK-IMU901 当前实测输出是 0x55 0x55 开头的帧，而不是 0xAA 0xFF。
 * 解析器先按 [55][55][CMD][LEN][DATA...][SUM] 抓有效帧，再用 CMD=0x06/LEN=8 取姿态角。
 */
typedef enum {
    PARSE_HEAD0,
    PARSE_HEAD1,
    PARSE_CMD,
    PARSE_LEN,
    PARSE_DATA,
    PARSE_CKSUM,
} ParseState;

static volatile uint32_t s_raw_byte_count;
static volatile uint32_t s_cksum_ok_count;
static volatile uint32_t s_attitude_count;
static volatile uint8_t  s_last_cmd;
static volatile uint8_t  s_last_len;
static volatile uint8_t  s_last_frame[IMU_FRAME_LEN];
static volatile uint8_t  s_raw_capture[IMU_CAPTURE_LEN];
static volatile uint8_t  s_raw_capture_pos;

static volatile float s_roll;
static volatile float s_pitch;
static volatile float s_yaw_raw;
static volatile int16_t s_cmd1_value[4];
static volatile int16_t s_cmd2_value[4];
static volatile int16_t s_cmd3_value[4];
static volatile int32_t s_heading_integral;
static volatile int32_t s_heading_bias;
static volatile int32_t s_heading_bias_sum;
static volatile uint16_t s_heading_bias_count;
static volatile bool s_heading_calibrated;
static volatile uint32_t s_heading_last_ms;
static volatile uint32_t s_cmd1_count;
static volatile uint32_t s_cmd2_count;
static volatile uint32_t s_cmd3_count;
static volatile float s_roll_offset;
static volatile float s_pitch_offset;
static volatile float s_yaw_offset;
static volatile bool  s_data_ready;

static ParseState s_state = PARSE_HEAD0;
static uint8_t s_cmd;
static uint8_t s_len;
static uint8_t s_buf[IMU_PAYLOAD_MAX];
static uint8_t s_idx;
static uint8_t s_cksum_acc;

static float normalize_angle(float angle)
{
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

void IMU_Init(void)
{
    s_raw_byte_count = 0;
    s_cksum_ok_count = 0;
    s_attitude_count = 0;
    s_last_cmd       = 0;
    s_last_len       = 0;
    s_roll           = 0.0f;
    s_pitch          = 0.0f;
    s_yaw_raw        = 0.0f;
    s_cmd1_count     = 0;
    s_cmd2_count     = 0;
    s_cmd3_count     = 0;
    s_heading_integral = 0;
    s_heading_bias   = 0;
    s_heading_bias_sum = 0;
    s_heading_bias_count = 0;
    s_heading_calibrated = false;
    s_heading_last_ms = 0;
    for (uint8_t i = 0; i < 4u; i++) {
        s_cmd1_value[i] = 0;
        s_cmd2_value[i] = 0;
        s_cmd3_value[i] = 0;
    }
    s_roll_offset    = 0.0f;
    s_pitch_offset   = 0.0f;
    s_yaw_offset     = 0.0f;
    s_data_ready     = false;
    s_state          = PARSE_HEAD0;
    for (uint8_t i = 0; i < IMU_FRAME_LEN; i++) {
        s_last_frame[i] = 0;
    }
    for (uint8_t i = 0; i < IMU_CAPTURE_LEN; i++) {
        s_raw_capture[i] = 0;
    }
    s_raw_capture_pos = 0;
}

float IMU_GetYaw(void)
{
    return (float)IMU_GetHeadingGyroRaw() * -0.1f;
}

float IMU_GetRelativeRoll(void)
{
    return normalize_angle(s_roll - s_roll_offset);
}

float IMU_GetRelativePitch(void)
{
    return normalize_angle(s_pitch - s_pitch_offset);
}

float IMU_GetRawYaw(void)
{
    return s_yaw_raw;
}

float IMU_GetYawOffset(void)
{
    return s_yaw_offset;
}

float IMU_GetRoll(void)
{
    return s_roll;
}

float IMU_GetPitch(void)
{
    return s_pitch;
}

float IMU_GetGyroX(void)
{
    return (float)s_cmd2_value[0];
}

float IMU_GetGyroY(void)
{
    return (float)s_cmd2_value[1];
}

float IMU_GetGyroZ(void)
{
    return (float)s_cmd2_value[2];
}

void IMU_SendGyroOnlyConfig(void)
{
}

uint32_t IMU_GetConfigSendCount(void)
{
    return 0;
}

void IMU_ResetYaw(void)
{
    s_yaw_offset = s_yaw_raw;
    s_heading_integral = 0;
    s_heading_bias_sum = 0;
    s_heading_bias_count = 0;
    s_heading_calibrated = false;
    s_heading_last_ms = 0;
}

/*
 * 记录当前三轴姿态为零点。
 * ATK-IMU901 直接输出姿态角，诊断时用相对值确认水平旋转对应的 Yaw 是否可信。
 */
void IMU_ResetAngles(void)
{
    s_roll_offset  = s_roll;
    s_pitch_offset = s_pitch;
    s_yaw_offset   = s_yaw_raw;
    s_heading_integral = 0;
    s_heading_bias_sum = 0;
    s_heading_bias_count = 0;
    s_heading_calibrated = false;
    s_heading_last_ms = 0;
}

bool IMU_IsDataReady(void)
{
    if (s_data_ready) {
        s_data_ready = false;
        return true;
    }
    return false;
}

uint32_t IMU_GetRawByteCount(void)
{
    return s_raw_byte_count;
}

uint8_t IMU_GetRawCapture(uint8_t *buf, uint8_t n)
{
    uint8_t cnt = (n < IMU_CAPTURE_LEN) ? n : (uint8_t)IMU_CAPTURE_LEN;
    uint8_t pos = s_raw_capture_pos;
    for (uint8_t i = 0; i < cnt; i++) {
        uint8_t idx = (uint8_t)((pos + IMU_CAPTURE_LEN - cnt + i) % IMU_CAPTURE_LEN);
        buf[i] = s_raw_capture[idx];
    }
    return cnt;
}

uint8_t IMU_GetLastFrame(uint8_t *buf, uint8_t n)
{
    return IMU_GetRawCapture(buf, n);
}

int16_t IMU_GetCmd55Value(uint8_t index)
{
    if (index >= 4u) return 0;
    return s_cmd1_value[index];
}

int16_t IMU_GetCmd3Value(uint8_t index)
{
    if (index >= 4u) return 0;
    return s_cmd3_value[index];
}

uint32_t IMU_GetCmd3Count(void)
{
    return s_cmd3_count;
}

uint32_t IMU_GetCmd55Count(void)
{
    return s_cmd1_count;
}

int16_t IMU_GetHeadingGyroRaw(void)
{
    int32_t delta = (int32_t)s_cmd2_value[0] - s_heading_bias;
    if (delta > 32767) return 32767;
    if (delta < -32768) return -32768;
    return (int16_t)delta;
}

uint32_t IMU_GetCksumOkCount(void)
{
    return s_cksum_ok_count;
}

uint32_t IMU_Get55Count(void)
{
    return s_cmd2_count;
}

uint8_t IMU_GetLastCmd(void)
{
    return s_last_cmd;
}

static void update_heading_integral(void)
{
    uint32_t now = Motor_GetTickMs();
    int32_t gyro;

    if (!s_heading_calibrated) {
        s_heading_bias_sum += s_cmd2_value[0];
        s_heading_bias_count++;
        if (s_heading_bias_count >= 50u) {
            s_heading_bias = s_heading_bias_sum / (int32_t)s_heading_bias_count;
            s_heading_calibrated = true;
            s_heading_integral = 0;
            s_heading_last_ms = now;
        }
        return;
    }

    gyro = (int32_t)s_cmd2_value[0] - s_heading_bias;
    if (gyro > -200 && gyro < 200) {
        gyro = 0;
    }

    if (s_heading_last_ms != 0u) {
        uint32_t dt = now - s_heading_last_ms;
        if (dt < 100u) {
            s_heading_integral += gyro * (int32_t)dt;
        }
    }
    s_heading_last_ms = now;
}

static void parse_payload_values(volatile int16_t *dst)
{
    for (uint8_t i = 0; i < 4u; i++) {
        dst[i] = (int16_t)((uint16_t)s_buf[(uint8_t)(i * 2u)] |
                 ((uint16_t)s_buf[(uint8_t)(i * 2u + 1u)] << 8));
    }
}

static void parse_attitude_payload(void)
{
    int16_t roll_raw  = (int16_t)((uint16_t)s_buf[0] | ((uint16_t)s_buf[1] << 8));
    int16_t pitch_raw = (int16_t)((uint16_t)s_buf[2] | ((uint16_t)s_buf[3] << 8));
    int16_t yaw_raw   = (int16_t)((uint16_t)s_buf[4] | ((uint16_t)s_buf[5] << 8));

    s_roll    = (float)roll_raw * 0.01f;
    s_pitch   = (float)pitch_raw * 0.01f;
    s_yaw_raw = (float)yaw_raw * 0.01f;
    s_attitude_count++;
    s_data_ready = true;
}

void UART1_IRQHandler(void)
{
    uint8_t byte;

    if (DL_UART_Main_getPendingInterrupt(UART_IMU_INST) != DL_UART_MAIN_IIDX_RX) {
        return;
    }

    byte = DL_UART_Main_receiveData(UART_IMU_INST);
    s_raw_byte_count++;
    s_raw_capture[s_raw_capture_pos] = byte;
    s_raw_capture_pos = (uint8_t)((s_raw_capture_pos + 1u) % IMU_CAPTURE_LEN);

    switch (s_state) {
        case PARSE_HEAD0:
            if (byte == IMU_HEAD0) {
                s_cksum_acc = IMU_HEAD0;
                s_last_frame[0] = byte;
                s_state = PARSE_HEAD1;
            }
            break;

        case PARSE_HEAD1:
            if (byte == IMU_HEAD1) {
                s_cksum_acc += byte;
                s_last_frame[1] = byte;
                s_state = PARSE_CMD;
            } else {
                s_state = (byte == IMU_HEAD0) ? PARSE_HEAD1 : PARSE_HEAD0;
                s_last_frame[0] = byte;
                s_cksum_acc = (byte == IMU_HEAD0) ? IMU_HEAD0 : 0u;
            }
            break;

        case PARSE_CMD:
            s_cmd = byte;
            s_cksum_acc += byte;
            s_last_frame[2] = byte;
            s_state = PARSE_LEN;
            break;

        case PARSE_LEN:
            s_len = byte;
            s_cksum_acc += byte;
            s_idx = 0;
            s_last_frame[3] = byte;
            if (s_len == 0u || s_len > IMU_PAYLOAD_MAX) {
                s_state = PARSE_HEAD0;
            } else {
                s_state = PARSE_DATA;
            }
            break;

        case PARSE_DATA:
            s_buf[s_idx] = byte;
            s_cksum_acc += byte;
            s_last_frame[(uint8_t)(4u + s_idx)] = byte;
            s_idx++;
            if (s_idx >= s_len) {
                s_state = PARSE_CKSUM;
            }
            break;

        case PARSE_CKSUM:
            s_last_frame[(uint8_t)(4u + s_len)] = byte;
            if (byte == s_cksum_acc) {
                s_cksum_ok_count++;
                s_last_cmd = s_cmd;
                s_last_len = s_len;
                if (s_len >= 8u) {
                    if (s_cmd == 0x01u) {
                        parse_payload_values(s_cmd1_value);
                        s_cmd1_count++;
                    } else if (s_cmd == 0x02u) {
                        parse_payload_values(s_cmd2_value);
                        update_heading_integral();
                        s_cmd2_count++;
                    } else if (s_cmd == 0x03u) {
                        parse_payload_values(s_cmd3_value);
                        s_cmd3_count++;
                    } else if (s_cmd == IMU_CMD_EULER) {
                        parse_attitude_payload();
                    }
                }
            }
            s_state = PARSE_HEAD0;
            break;

        default:
            s_state = PARSE_HEAD0;
            break;
    }
}
