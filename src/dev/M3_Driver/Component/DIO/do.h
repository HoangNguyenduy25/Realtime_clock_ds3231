#ifndef DO_H
#define DO_H

#include <stdbool.h>
#include <stdint.h>

/* Port numbers follow the chapter 4 DIO interface: A=1, etc */
typedef enum
{
    DO_PORT_A = 1,
    DO_PORT_B,
    DO_PORT_C,
    DO_PORT_D
} do_port_t;

typedef struct 
{
    uint32_t port;
    uint32_t pin;
    bool bStatus;
} do_t;

bool do_init(do_t *me);

void do_set(do_t *me);
void do_reset(do_t *me);
void do_toggle(do_t *me);



#endif /* DO_H */