#ifndef VISION_UART_H_
#define VISION_UART_H_

#include "vision_observation.h"

#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Board-specific UART binding for one MaixCAM link. */
typedef struct {
    UART_Regs *instance;       /**< SysConfig-generated UART instance. */
    IRQn_Type irqn;            /**< IRQ belonging to instance. */
    uint16_t frame_width;      /**< MaixCAM output image width, in pixels. */
    uint16_t frame_height;     /**< MaixCAM output image height, in pixels. */
} vision_uart_config_t;

/** @brief Receive-side communication statistics since vision_uart_init(). */
typedef struct {
    uint32_t packet_count;       /**< Successfully converted observations. */
    uint32_t parse_error_count;  /**< Complete lines rejected by the parser. */
    uint32_t overrun_count;      /**< RX-ring bytes or oversized lines discarded. */
    uint32_t tx_drop_count;      /**< Lines rejected while another TX is pending. */
    uint32_t tx_timeout_count;   /**< Pending TX lines discarded after timeout. */
} vision_uart_stats_t;

/** @brief Bind and enable the board-selected UART. Returns false for bad config. */
bool vision_uart_init(const vision_uart_config_t *config);

/** @brief Call from the IRQ handler of the UART selected in vision_uart_init(). */
void vision_uart_on_uart_irq(void);

/**
 * @brief Drain received bytes and publish the newest valid observation.
 * @param now_ms Current monotonic millisecond timestamp supplied by the app.
 * @return true only when one or more complete lines became valid observations.
 */
bool vision_uart_process(uint32_t now_ms);

/**
 * @brief Consume the newest unpublished observation.
 * @details Main-loop API. Do not call it from an ISR. It must run in the same
 *          thread/context as vision_uart_process(); the observation copy is
 *          intentionally not protected for concurrent task access.
 */
bool vision_uart_take_latest_observation(vision_observation_t *out);

/** @brief Copy counters. Main-loop API. */
void vision_uart_get_stats(vision_uart_stats_t *out);

/** @brief Queue one newline-terminated line without waiting for hardware TX. */
bool vision_uart_send_line(const char *line);

/** @brief Advance queued TX bytes without blocking; abort a stalled line. */
void vision_uart_service_tx(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* VISION_UART_H_ */
