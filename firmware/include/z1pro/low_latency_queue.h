#pragma once
#include <stdint.h>
#include <stdbool.h>
#define Z1_MOUSE_QUEUE_CAPACITY 8
struct z1_motion_entry { uint16_t buttons; int32_t axis[4]; };
struct z1_motion_report { uint16_t buttons; int16_t axis[4]; };
struct z1_motion_queue {
    struct z1_motion_entry entries[Z1_MOUSE_QUEUE_CAPACITY];
    uint32_t head, count, produced, merged, overflow, max_depth;
};
void z1_motion_push(struct z1_motion_queue *q, uint16_t buttons,
                    int16_t x, int16_t y, int16_t sy, int16_t sx);
bool z1_motion_pop(struct z1_motion_queue *q, struct z1_motion_report *out);
void z1_motion_clear(struct z1_motion_queue *q);
void z1pro_perf_stats(uint32_t out[8]);
