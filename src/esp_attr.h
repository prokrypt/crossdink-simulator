#pragma once

// ESP-IDF memory-placement attributes. The host has one flat address space,
// so every placement attribute expands to nothing. RTC_NOINIT and
// EXT_RAM_NOINIT data therefore starts zeroed on each launch; firmware guards
// such data with magic values, which a zeroed launch reads as a cold boot.
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#ifndef DRAM_ATTR
#define DRAM_ATTR
#endif
#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif
#ifndef RTC_DATA_ATTR
#define RTC_DATA_ATTR
#endif
#ifndef RTC_FAST_ATTR
#define RTC_FAST_ATTR
#endif
#ifndef RTC_SLOW_ATTR
#define RTC_SLOW_ATTR
#endif
#ifndef RTC_IRAM_ATTR
#define RTC_IRAM_ATTR
#endif
#ifndef EXT_RAM_BSS_ATTR
#define EXT_RAM_BSS_ATTR
#endif
#ifndef EXT_RAM_NOINIT_ATTR
#define EXT_RAM_NOINIT_ATTR
#endif
#ifndef NOINIT_ATTR
#define NOINIT_ATTR
#endif
#ifndef WORD_ALIGNED_ATTR
#define WORD_ALIGNED_ATTR __attribute__((aligned(4)))
#endif
#ifndef DMA_ATTR
#define DMA_ATTR WORD_ALIGNED_ATTR
#endif
#ifndef FORCE_INLINE_ATTR
#define FORCE_INLINE_ATTR static inline __attribute__((always_inline))
#endif
#ifndef NOINLINE_ATTR
#define NOINLINE_ATTR __attribute__((noinline))
#endif
