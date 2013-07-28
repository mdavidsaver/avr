#ifndef SERVER_H
#define SERVER_H

#include <inttypes.h>

//! Called once before interrupts are enabled.
void user_init(void);

//! Called in the main program loop
void user_loop(void);

//! Called periodically from a timer ISR
void user_tick(void);

#endif // SERVER_H
