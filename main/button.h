#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

extern volatile int64_t button_time; // Time of last button press in microseconds
void button_init(void);

#endif /* BUTTON_H */
