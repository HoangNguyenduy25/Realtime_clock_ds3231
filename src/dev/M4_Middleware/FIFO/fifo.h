#ifndef FIFO_H
#define FIFO_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Structure representing a FIFO for ring buffer.
 */

typedef struct {
    uint8_t *buffer;      // Pointer to the buffer array
    uint16_t capacity;    // Maximum number of elements in the buffer
    volatile uint16_t head;       // Index of the head (write position)
    volatile uint16_t tail;       // Index of the tail (read position)
    volatile uint16_t count;       // Size of the buffer
} FIFO_t;

/**
 * Returns false for a null pointer or zero capacity
 * An invalid initialization leaves a non-null FIFO empty and unusable until initialized successfully.
 */
bool FIFO_Init(FIFO_t *fifo, uint8_t *buffer, uint16_t capacity);


/**
 * Nonblocking: push returns false when the FIFO is full, Pop returns false when the FIFO is empty.
 * A failed Push leaves the queue unchanged, a failed Pop leaves *value unchanged.
 */
bool FIFO_Push(FIFO_t *fifo, uint8_t value);
bool FIFO_Pop(FIFO_t *fifo, uint8_t *value);

uint16_t FIFO_Count(const FIFO_t *fifo);
void FIFO_Clear(FIFO_t *fifo);

#endif // FIFO_H
