#ifndef VISION_OBSERVATION_H_
#define VISION_OBSERVATION_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 产生观测数据的串口协议类型。
 *
 * @details 该字段仅用于诊断和协议迁移；云台跟踪控制不应依赖某个具体协议类型。
 */
typedef enum {
    VISION_OBSERVATION_PROTOCOL_UNKNOWN = 0U,
    VISION_OBSERVATION_PROTOCOL_TV,
    VISION_OBSERVATION_PROTOCOL_AIM
} vision_observation_protocol_t;

/**
 * @brief 主控侧统一的目标图像观测。
 *
 * @details 视觉协议解析器负责填充本结构，云台控制只读取本结构，不直接解析 UART
 *          文本。target_valid 为真时，target_x/target_y 必须位于给定图像尺寸内。
 *          sequence 对每个成功解析的视觉帧递增；received_at_ms 使用主控单调毫秒
 *          时基记录，用于上层判断通信是否超时。
 */
typedef struct {
    bool target_valid;                        /**< 当前帧是否检测到目标 */
    uint16_t target_x;                        /**< 目标中心横坐标，单位：像素 */
    uint16_t target_y;                        /**< 目标中心纵坐标，单位：像素 */
    uint16_t frame_width;                     /**< 坐标所属图像宽度，单位：像素 */
    uint16_t frame_height;                    /**< 坐标所属图像高度，单位：像素 */
    uint32_t sequence;                        /**< 主控接收的递增视觉帧序号 */
    uint32_t received_at_ms;                  /**< 主控接收完成时刻，单位：ms */
    vision_observation_protocol_t protocol;  /**< 当前帧来源协议 */
} vision_observation_t;

/**
 * @brief Check whether an observation is still within its allowed age.
 * @details Uses unsigned subtraction and is safe across a 32-bit millisecond
 *          counter wrap. A frame with target_valid=false can still be fresh.
 */
bool vision_observation_is_fresh(const vision_observation_t *observation,
                                 uint32_t now_ms, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* VISION_OBSERVATION_H_ */
