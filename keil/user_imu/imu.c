#include "imu.h"
#include "ti_msp_dl_config.h"

/*
 * ATK-IMU901 帧解析状态机（在 UART1_IRQHandler 内运行）
 *
 * 帧格式: [0xAA][0xFF][CMD][LEN][DATA0..DATAN-1][CKSUM]
 *   校验: CKSUM = (CMD + LEN + DATA0 + ... + DATAN-1) & 0xFF
 *   欧拉角帧: CMD=0x44, LEN=6
 *     DATA[0:1] = Roll  (int16, 小端, 单位 0.01 度)
 *     DATA[2:3] = Pitch (int16, 小端, 单位 0.01 度)
 *     DATA[4:5] = Yaw   (int16, 小端, 单位 0.01 度)
 */

#define IMU_HDR0       0xAAu
#define IMU_HDR1       0xFFu
#define IMU_CMD_EULER  0x44u
#define IMU_EULER_LEN  6u
#define IMU_BUF_SIZE   16u

typedef enum {
    PARSE_HDR0,
    PARSE_HDR1,
    PARSE_CMD,
    PARSE_LEN,
    PARSE_DATA,
    PARSE_CKSUM,
} ParseState;

/* 欧拉角原始读数（ISR 内更新） */
static volatile float s_roll;
static volatile float s_pitch;
static volatile float s_yaw_raw;     /* IMU 输出的绝对偏转角 */
static volatile float s_yaw_offset;  /* ResetYaw 时记录的基准值 */
static volatile bool  s_data_ready;

/* 解析中间状态 */
static ParseState s_state = PARSE_HDR0;
static uint8_t    s_cmd;
static uint8_t    s_len;
static uint8_t    s_buf[IMU_BUF_SIZE];
static uint8_t    s_idx;
static uint8_t    s_cksum_acc;

void IMU_Init(void)
{
    s_roll       = 0.0f;
    s_pitch      = 0.0f;
    s_yaw_raw    = 0.0f;
    s_yaw_offset = 0.0f;
    s_data_ready = false;
    s_state      = PARSE_HDR0;
    /* UART1 已在 SYSCFG_DL_UART_IMU_init() 中完成初始化和中断使能 */
}

float IMU_GetYaw(void)
{
    return s_yaw_raw - s_yaw_offset;
}

float IMU_GetRoll(void)
{
    return s_roll;
}

float IMU_GetPitch(void)
{
    return s_pitch;
}

void IMU_ResetYaw(void)
{
    s_yaw_offset = s_yaw_raw;
}

bool IMU_IsDataReady(void)
{
    if (s_data_ready) {
        s_data_ready = false;
        return true;
    }
    return false;
}

/*
 * UART1 RX 中断服务函数
 * 每收到一个字节触发一次，运行帧解析状态机
 * 解析到欧拉角帧后更新 s_roll/s_pitch/s_yaw_raw 并置位 s_data_ready
 */
void UART1_IRQHandler(void)
{
    uint8_t byte;
    int16_t r, p, y;

    if (DL_UART_Main_getPendingInterrupt(UART_IMU_INST) !=
        DL_UART_MAIN_IIDX_RX) {
        return;
    }

    byte = DL_UART_Main_receiveData(UART_IMU_INST);

    switch (s_state) {
        case PARSE_HDR0:
            if (byte == IMU_HDR0) {
                s_state = PARSE_HDR1;
            }
            break;

        case PARSE_HDR1:
            s_state = (byte == IMU_HDR1) ? PARSE_CMD : PARSE_HDR0;
            break;

        case PARSE_CMD:
            s_cmd       = byte;
            s_cksum_acc = byte;      /* 校验累加从 CMD 开始 */
            s_state     = PARSE_LEN;
            break;

        case PARSE_LEN:
            s_len        = byte;
            s_cksum_acc += byte;
            s_idx        = 0;
            /* 异常帧丢弃 */
            s_state = (s_len > 0u && s_len <= IMU_BUF_SIZE)
                      ? PARSE_DATA : PARSE_HDR0;
            break;

        case PARSE_DATA:
            s_buf[s_idx++] = byte;
            s_cksum_acc   += byte;
            if (s_idx >= s_len) {
                s_state = PARSE_CKSUM;
            }
            break;

        case PARSE_CKSUM:
            if (byte == s_cksum_acc &&
                s_cmd == IMU_CMD_EULER &&
                s_len == IMU_EULER_LEN)
            {
                r = (int16_t)((uint16_t)s_buf[0] | ((uint16_t)s_buf[1] << 8));
                p = (int16_t)((uint16_t)s_buf[2] | ((uint16_t)s_buf[3] << 8));
                y = (int16_t)((uint16_t)s_buf[4] | ((uint16_t)s_buf[5] << 8));
                s_roll      = (float)r * 0.01f;
                s_pitch     = (float)p * 0.01f;
                s_yaw_raw   = (float)y * 0.01f;
                s_data_ready = true;
            }
            s_state = PARSE_HDR0;
            break;

        default:
            s_state = PARSE_HDR0;
            break;
    }
}
