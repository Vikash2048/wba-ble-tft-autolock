/* Screen log: ring buffer of short text lines shown on the TFT ("10. AutoLock log").
 * Called from the Bluetooth code (main loop / sequencer context) – never from interrupts. */
#include "main.h"
#include "screen_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define SLOG_LINES  40
#define SLOG_WIDTH  60

static char     ring[SLOG_LINES][SLOG_WIDTH];
static uint32_t head;            /* next line to write            */
static uint32_t total;           /* lines written since power-up  */

static void add_line(const char *t)
{
    uint32_t s = HAL_GetTick() / 1000U;
    snprintf(ring[head], SLOG_WIDTH, "%02lu:%02lu %s",
             (unsigned long)((s / 60U) % 100U), (unsigned long)(s % 60U), t);
    head = (head + 1U) % SLOG_LINES;
    total++;
}

void slog_printf(const char *fmt, ...)
{
    char tmp[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);

    /* one call may hold several lines; drop empty ones and the ">> " decoration */
    char *p = tmp;
    while (*p)
    {
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        char *t = p;
        while (*t == ' ' || *t == '>' || *t == '*') t++;
        if (*t) add_line(t);
        if (!nl) break;
        p = nl + 1;
    }
}

uint32_t slog_count(void) { return total; }

void slog_clear(void) { total = 0; head = 0; memset(ring, 0, sizeof ring); }

void slog_get(char *buf, uint32_t size, uint32_t lines)
{
    uint32_t n = (total < SLOG_LINES) ? total : SLOG_LINES;
    if (lines > n) lines = n;
    uint32_t idx = (head + SLOG_LINES - lines) % SLOG_LINES;
    uint32_t used = 0;
    buf[0] = '\0';
    for (uint32_t i = 0; i < lines && used + 2 < size; i++)
    {
        int w = snprintf(buf + used, size - used, "%s%s", i ? "\n" : "", ring[idx]);
        if (w < 0) break;
        used += (uint32_t)w;
        if (used >= size) { used = size - 1; break; }
        idx = (idx + 1U) % SLOG_LINES;
    }
}
