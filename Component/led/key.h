#ifndef KEY_H
#define KEY_H

#include <stdbool.h>

void key_init(void);
bool key_read_user(void);   /* PB21, 按下低电平, 软件消抖 */

#endif
