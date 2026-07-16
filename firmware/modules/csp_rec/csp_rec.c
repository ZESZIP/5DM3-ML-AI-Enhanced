/*
 * csp_rec — arms cinema modes and owns the .csp mux session.
 * On camera: hooks REC button / LiveView raw callback into stream_engine.
 */
#include "stream_engine.h"
#include <stdint.h>
#include <string.h>

static const stream_mode_t *current;
static char last_path[64];

int csp_rec_arm_by_id(const char *mode_id, uint32_t measured_mibs, int bridge)
{
    int n = 0;
    const stream_mode_t *cat = stream_mode_catalog(&n);
    const stream_mode_t *chosen = stream_select_mode(measured_mibs, bridge);
    if (mode_id) {
        for (int i = 0; i < n; i++) {
            if (strcmp(cat[i].mode_id, mode_id) == 0) {
                if (cat[i].needs_bridge && !bridge)
                    break;
                chosen = &cat[i];
                break;
            }
        }
    }
    current = chosen;
    return stream_engine_arm(current);
}

int csp_rec_start(const char *path)
{
    if (!path)
        return -1;
    strncpy(last_path, path, sizeof(last_path) - 1);
    return stream_engine_start(path);
}

int csp_rec_stop(void)
{
    return stream_engine_stop();
}

const stream_mode_t *csp_rec_current(void)
{
    return current;
}
