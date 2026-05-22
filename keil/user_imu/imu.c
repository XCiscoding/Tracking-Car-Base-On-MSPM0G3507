#include "imu.h"
#include "ti_msp_dl_config.h"
#include <math.h>

/*
 * WIT-Motion 兴定智 (JY901/WT901 兴定) 帧解析状态机（在 UART1_IRQHandler 内运行）
 *
 * 帧格式: [0x55][CMD][D0][D1][D2][D3][D4][D5][D6][D7][SUM]（共 11 字节）
 *   校验: SUM = (0x55 + CMD + D0..D7) & 0xFF
 *   欧拉角帧: CMD=0x53, 数据 8 字节
 *     D[0:1] = Roll  (int16, 小端, 单位: /32768 * 180 度)
 *     D[2:3] = Pitch (int16, 小端, 同上)
 *     D[4:5] = Yaw   (int16, 小端, 同上)
 *     D[6:7] = 电压 (则无用）
 */

#define IMU_HDR0       0x55u
#define IMU_CMD_EULER  0x53u
#define IMU_CMD_QUAT   0x55u  /* 四元数帧: Q0=w, Q1=x, Q2=y, Q3=z，各 int16/32768 */
#define IMU_DATA_LEN   8u
#define IMU_BUF_SIZE   16u

typedef enum {
    PARSE_HDR,     /* 等待 0x55 */
    PARSE_CMD,     /* 接收 CMD 字节 */
    PARSE_DATA,    /* 接收 8 个数据字节 */
    PARSE_CKSUM,   /* 接收校验字节 */
} ParseState;

/* 原始字节计数——诊断用：=0 说明硬件路径断，>0但帧数=0说明协议不匹配 */
static volatile uint32_t s_raw_byte_count = 0;
/* 滚动循环缓冲区——始终保存最近 RAW_CAP_SIZE 个字节（覆盖旧数据） */
#define RAW_CAP_SIZE 8u
static volatile uint8_t  s_raw_cap[RAW_CAP_SIZE];
/* 0x55 帧头字节出现次数：>0 证明 WIT-Motion 帧头正在流动 */
static volatile uint32_t s_0x55_count    = 0;
/* checksum 通过计数（任意 CMD）：>0 说明帧完整解析成功 */
static volatile uint32_t s_cksum_ok_count = 0;
/* 最近一次 PARSE_CMD 阶段收到的 CMD 字节：告诉我们模块在发什么类型的帧 */
static volatile uint8_t  s_last_cmd = 0;
/* checksum 通过的最后一个 CMD：0x52=陀螺仪帧，0x53=欧拉角帧（启用后应显示 53） */
static volatile uint8_t  s_last_cksum_cmd = 0;

/* 欧拉角原始读数（ISR 内更新） */
static volatile float s_roll;
static volatile float s_pitch;
static volatile float s_yaw_raw;     /* IMU 输出的绝对偏转角 */
static volatile float s_yaw_offset;  /* ResetYaw 时记录的基准值 */
static volatile bool  s_data_ready;

/* 解析中间状态 */
static ParseState s_state = PARSE_HDR;
static uint8_t    s_cmd;
static uint8_t    s_buf[IMU_BUF_SIZE];
static uint8_t    s_idx;
static uint8_t    s_cksum_acc;

void IMU_Init(void)
{
    s_roll            = 0.0f;
    s_pitch           = 0.0f;
    s_yaw_raw         = 0.0f;
    s_yaw_offset      = 0.0f;
    s_data_ready      = false;
    s_state           = PARSE_HDR;
    s_0x55_count      = 0;
    s_cksum_ok_count  = 0;
    s_raw_byte_count  = 0;
    s_last_cmd        = 0;
    s_last_cksum_cmd  = 0;

    /*
     * WIT-Motion 配置命令：写入 RSW 寄存器 (0x02)，启用欧拉角帧 (CMD=0x53) 输出
     * 寄存器 0x02 (RSW) = 0x000E：bit1=加速度, bit2=陀螺仪, bit3=角度(欧拉角)
     * 樠式： 0xFF 0xAA [REG] [DATA_L] [DATA_H]
     * 调用前必须已等待模块上电稳定 (≥500ms)
     */
    static const uint8_t wit_rsw[5] = {0xFF, 0xAA, 0x02, 0x0E, 0x00};
    for (uint8_t i = 0; i < 5; i++) {
        DL_UART_Main_transmitData(UART_IMU_INST, wit_rsw[i]);
        delay_cycles(160000);   /* ~5ms / 字节 @ 32MHz，>> 86µs/byte @ 115200 */
    }
    delay_cycles(6400000);          /* ~200ms 等待模块处理配置 */
    /* UART1 RX 中断已在 SYSCFG_DL_UART_IMU_init() 中使能 */
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

uint32_t IMU_GetRawByteCount(void)
{
    return s_raw_byte_count;
}

/* 读取最近 n 个字节（循环缓冲区）；返回实际读取数量 */
uint8_t IMU_GetRawCapture(uint8_t *buf, uint8_t n)
{
    uint8_t  cnt   = (n < RAW_CAP_SIZE) ? n : (uint8_t)RAW_CAP_SIZE;
    uint32_t total = s_raw_byte_count;  /* ISR 写入总数快照 */
    if (total < cnt) cnt = (uint8_t)total;
    for (uint8_t i = 0; i < cnt; i++) {
        uint32_t idx = (total - cnt + i) % RAW_CAP_SIZE;
        buf[i] = s_raw_cap[idx];
    }
    return cnt;
}

uint32_t IMU_GetCksumOkCount(void)
{
    return s_cksum_ok_count;
}

uint32_t IMU_Get55Count(void)
{
    return s_0x55_count;
}

uint8_t IMU_GetLastCmd(void)
{
    return s_last_cksum_cmd;  /* 最后一个 checksum 通过的 CMD：0x52=陀螺仪, 0x53=欧拉角 */
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
    /* 循环覆盖：始终保留最近 RAW_CAP_SIZE 个字节 */
    s_raw_cap[s_raw_byte_count % RAW_CAP_SIZE] = byte;
    s_raw_byte_count++;

    /* 计数 0x55 帧头字节（诊断） */
    if (byte == 0x55u) s_0x55_count++;

    switch (s_state) {
        case PARSE_HDR:
            if (byte == IMU_HDR0) {
                s_cksum_acc = IMU_HDR0;   /* 校验从帧头 0x55 开始累加 */
                s_state = PARSE_CMD;
            }
            break;

        case PARSE_CMD:
            s_cmd        = byte;
            s_last_cmd   = byte;   /* 记录最后一个 CMD，供诊断读取 */
            s_cksum_acc += byte;
            s_idx        = 0;
            s_state      = PARSE_DATA;
            break;

        case PARSE_DATA:
            s_buf[s_idx++] = byte;
            s_cksum_acc   += byte;
            if (s_idx >= IMU_DATA_LEN) {
                s_state = PARSE_CKSUM;
            }
            break;

        case PARSE_CKSUM:
            if (byte == (s_cksum_acc & 0xFFu)) {
                s_cksum_ok_count++;     /* 任意 CMD checksum 通过 */
                s_last_cksum_cmd = s_cmd;  /* 记录通过校验的 CMD，供 K: 诊断 */
                if (s_cmd == IMU_CMD_EULER) {
                    r = (int16_t)((uint16_t)s_buf[0] | ((uint16_t)s_buf[1] << 8));
                    p = (int16_t)((uint16_t)s_buf[2] | ((uint16_t)s_buf[3] << 8));
                    y = (int16_t)((uint16_t)s_buf[4] | ((uint16_t)s_buf[5] << 8));
                    s_roll      = (float)r / 32768.0f * 180.0f;
                    s_pitch     = (float)p / 32768.0f * 180.0f;
                    s_yaw_raw   = (float)y / 32768.0f * 180.0f;
                    s_data_ready = true;
                } else if (s_cmd == IMU_CMD_QUAT) {
                    /* 四元数 → 欧拉角 */
                    float q0 = (float)(int16_t)((uint16_t)s_buf[0] | ((uint16_t)s_buf[1] << 8)) / 32768.0f;
                    float q1 = (float)(int16_t)((uint16_t)s_buf[2] | ((uint16_t)s_buf[3] << 8)) / 32768.0f;
                    float q2 = (float)(int16_t)((uint16_t)s_buf[4] | ((uint16_t)s_buf[5] << 8)) / 32768.0f;
                    float q3 = (float)(int16_t)((uint16_t)s_buf[6] | ((uint16_t)s_buf[7] << 8)) / 32768.0f;
                    s_roll    = atan2f(2.0f*(q0*q1 + q2*q3),
                                      1.0f - 2.0f*(q1*q1 + q2*q2)) * (180.0f / 3.14159265f);
                    float sinp = 2.0f*(q0*q2 - q3*q1);
                    if (sinp >  1.0f) sinp =  1.0f;
                    if (sinp < -1.0f) sinp = -1.0f;
                    s_pitch   = asinf(sinp) * (180.0f / 3.14159265f);
                    s_yaw_raw = atan2f(2.0f*(q0*q3 + q1*q2),
                                      1.0f - 2.0f*(q2*q2 + q3*q3)) * (180.0f / 3.14159265f);
                    s_data_ready = true;
                }
            }
            s_state = PARSE_HDR;
            break;

        default:
            s_state = PARSE_HDR;
            break;
    }
}
