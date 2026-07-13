#ifndef VISION_CONFIG_H_
#define VISION_CONFIG_H_

/* MaixCAM UART text protocol and image defaults. */
#define VISION_UART_LINE_MAX                 (96U)
#define VISION_UART_RX_RING_CAPACITY         (256U)
#define VISION_DEFAULT_FRAME_WIDTH           (512U)
#define VISION_DEFAULT_FRAME_HEIGHT          (320U)

#define VISION_AIM_PREFIX                    "AIM,"
#define VISION_AIM_PREFIX_LEN                (4U)
#define VISION_AIM_NUMERIC_FIELD_COUNT       (7U)

#define VISION_TV_PREFIX                     "TV,"
#define VISION_TV_PREFIX_LEN                 (3U)
#define VISION_TV_NUMERIC_FIELD_COUNT        (5U)

#endif /* VISION_CONFIG_H_ */
