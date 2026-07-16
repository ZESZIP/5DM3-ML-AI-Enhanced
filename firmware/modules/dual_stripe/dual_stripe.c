/*
 * Dual storage abstraction: CF / SD / stripe / SSD bridge.
 * Host-buildable logic; on camera this wraps DryOS FIO.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    PATH_CF = 0,
    PATH_SD,
    PATH_STRIPE,
    PATH_SSD_BRIDGE
} storage_path_t;

typedef struct {
    storage_path_t path;
    FILE *a;
    FILE *b;
    uint64_t bytes_a;
    uint64_t bytes_b;
    int bridge;
} stripe_ctx_t;

static stripe_ctx_t ctx;

int dual_stripe_probe_bridge(const char *mount_root)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/CINEMA/BRIDGE.OK", mount_root ? mount_root : "");
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

int dual_stripe_open(storage_path_t path, const char *file_a, const char *file_b)
{
    memset(&ctx, 0, sizeof(ctx));
    ctx.path = path;
    ctx.a = fopen(file_a, "wb");
    if (!ctx.a)
        return -1;
    if (path == PATH_STRIPE) {
        if (!file_b)
            return -1;
        ctx.b = fopen(file_b, "wb");
        if (!ctx.b)
            return -1;
    }
    ctx.bridge = (path == PATH_SSD_BRIDGE);
    return 0;
}

int dual_stripe_write(const void *data, uint32_t len, uint32_t frame_index)
{
    if (!ctx.a)
        return -1;
    if (ctx.path != PATH_STRIPE) {
        if (fwrite(data, 1, len, ctx.a) != len)
            return -1;
        ctx.bytes_a += len;
        return 0;
    }
    /* Even frames -> A (CF), odd -> B (SD) */
    FILE *t = (frame_index & 1u) ? ctx.b : ctx.a;
    if (fwrite(data, 1, len, t) != len)
        return -1;
    if (frame_index & 1u)
        ctx.bytes_b += len;
    else
        ctx.bytes_a += len;
    return 0;
}

void dual_stripe_close(void)
{
    if (ctx.a)
        fclose(ctx.a);
    if (ctx.b)
        fclose(ctx.b);
    memset(&ctx, 0, sizeof(ctx));
}

const char *dual_stripe_ui_label(void)
{
    if (ctx.bridge || ctx.path == PATH_SSD_BRIDGE)
        return "CFast A";
    if (ctx.path == PATH_STRIPE)
        return "CF+SD";
    if (ctx.path == PATH_SD)
        return "SD A";
    return "CF A";
}
