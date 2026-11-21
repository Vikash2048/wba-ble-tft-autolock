#ifndef SCREEN_LOG_H
#define SCREEN_LOG_H
#include <stdint.h>

/* Log to the TFT instead of the PC (UART TX = PA6 = LCD_D7, so no serial log here).
 * Lines go into a RAM ring buffer; the "10. AutoLock log" screen shows the newest ones.
 * Same printf style as LOG_INFO_APP:  SLOG(">> presence: link 0x%04X\n", h);       */
void     slog_printf(const char *fmt, ...);
uint32_t slog_count(void);                                     /* lines added so far (changes = redraw) */
void     slog_get(char *buf, uint32_t size, uint32_t lines);   /* newest `lines` lines, '\n'-separated  */
void     slog_clear(void);

#define SLOG(...)  slog_printf(__VA_ARGS__)

#endif
