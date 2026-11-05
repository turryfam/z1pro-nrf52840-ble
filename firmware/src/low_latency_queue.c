/* Merge adjacent movement reports, keeping button-state boundaries ordered.
 * Caller supplies synchronization. No allocation, kernel waits or device I/O.
 */
#include <z1pro/low_latency_queue.h>
#include <limits.h>
static int32_t sum32(int32_t a, int32_t b) {
    int64_t v = (int64_t)a + b;
    return v > INT32_MAX ? INT32_MAX : v < INT32_MIN ? INT32_MIN : (int32_t)v;
}
void z1_motion_push(struct z1_motion_queue *q, uint16_t buttons,
                    int16_t x, int16_t y, int16_t sy, int16_t sx) {
    int32_t delta[4] = {x,y,sy,sx}; q->produced++;
    if (q->count) {
        struct z1_motion_entry *last = &q->entries[(q->head + q->count - 1) % Z1_MOUSE_QUEUE_CAPACITY];
        if (last->buttons == buttons) {
            for (int i=0;i<4;i++) last->axis[i] = sum32(last->axis[i], delta[i]);
            q->merged++; return;
        }
    }
    if (q->count == Z1_MOUSE_QUEUE_CAPACITY) {
        /* Extreme button-edge overload: retain total displacement and newest
         * button state, count the evicted edge. Never block the input thread.
         */
        struct z1_motion_entry *old = &q->entries[q->head];
        struct z1_motion_entry *next = &q->entries[(q->head+1) % Z1_MOUSE_QUEUE_CAPACITY];
        for (int i=0;i<4;i++) next->axis[i] = sum32(next->axis[i], old->axis[i]);
        q->head=(q->head+1)%Z1_MOUSE_QUEUE_CAPACITY; q->count--; q->overflow++;
    }
    struct z1_motion_entry *entry = &q->entries[(q->head+q->count)%Z1_MOUSE_QUEUE_CAPACITY];
    entry->buttons=buttons;
    for (int i=0;i<4;i++) entry->axis[i]=delta[i];
    q->count++; if (q->count>q->max_depth) q->max_depth=q->count;
}
bool z1_motion_pop(struct z1_motion_queue *q, struct z1_motion_report *out) {
    if (!q->count) return false;
    struct z1_motion_entry *entry=&q->entries[q->head]; bool remaining=false;
    out->buttons=entry->buttons;
    for (int i=0;i<4;i++) {
        int32_t v=entry->axis[i];
        out->axis[i]=(int16_t)(v>INT16_MAX ? INT16_MAX : v<INT16_MIN ? INT16_MIN : v);
        entry->axis[i]-=out->axis[i]; remaining |= entry->axis[i]!=0;
    }
    if (!remaining) { q->head=(q->head+1)%Z1_MOUSE_QUEUE_CAPACITY; q->count--; }
    return true;
}
void z1_motion_clear(struct z1_motion_queue *q) { q->head=0; q->count=0; }
