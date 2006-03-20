#ifndef IS_EVENT_H
#define IS_EVENT_H

typedef struct {
    int32_t secs;
    int32_t usecs;
    int16_t key;       // multiplex key
    int16_t size;
} event_header;

static inline event_header *event_map(byte *buffer)
{
    return (event_header *)buffer;
}

/**
 *  Compare 2 events timestamps and return :
 *   -1 if event1 < event2
 *    0 if event1 = event2
 *    1 if event1 > event2
 */
static inline int compare_event_ts(event_header *event1, event_header *event2)
{
    if (event1->secs < event2->secs) {
        return -1;
    }
    if (event1->secs > event2->secs) {
        return 1;
    }
    if (event1->usecs < event2->usecs) {
        return -1;
    }
    if (event1->usecs > event2->usecs) {
        return 1;
    }
    return 0;
}

#endif
