/*
 * JZ4780 common routines
 *
 * Copyright (c) 2013 Imagination Technologies
 * Author: Paul Burton <paul.burton@imgtec.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <asm/io.h>
#include <mach/jz4780.h>
#include <mach/jz4780_dram.h>
#include <mmc.h>
#include <spl.h>

#ifdef CONFIG_SPL_BUILD
/* Pointer to the global data structure for SPL */
DECLARE_GLOBAL_DATA_PTR;
gd_t gdata __attribute__ ((section(".bss")));

void board_init_f(ulong dummy)
{
	typedef void __noreturn (*image_entry_noargs_t)(void);
	struct mmc *mmc;
	unsigned long count;
	struct image_header *header;
	int ret;

	/* Set global data pointer */
	gd = &gdata;

	timer_init();
	pll_init();
	sdram_init();
	enable_caches();

	/* Clear the BSS */
	memset(__bss_start, 0, (char *)&__bss_end - __bss_start);

	gd->flags |= GD_FLG_SPL_INIT;

	ret = mmc_initialize(NULL);
	if (ret)
		hang();

	mmc = find_mmc_device(BOOT_DEVICE_MMC1);
	if (ret)
		hang();

	ret = mmc_init(mmc);
	if (ret)
		hang();

	header = (struct image_header *)(CONFIG_SYS_TEXT_BASE -
					 sizeof(struct image_header));

	count = blk_dread(mmc_get_blk_desc(mmc),
		CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR,
		0x800, header);
	if (count == 0)
		hang();

	image_entry_noargs_t image_entry =
		(image_entry_noargs_t)CONFIG_SYS_TEXT_BASE;

	image_entry();

	hang();
}
#endif /* CONFIG_SPL_BUILD */

phys_size_t initdram(int board_type)
{
	return sdram_size(0) + sdram_size(1);
}

ulong board_get_usable_ram_top(ulong total_size)
{
	return CONFIG_SYS_SDRAM_BASE + (256 * 1024 * 1024);
}

int print_cpuinfo(void)
{
	printf("CPU:   Ingenic JZ4780\n");
	return 0;
}

/* WDT */
#define WDT_TDR		0x00
#define WDT_TCER	0x04
#define WDT_TCNT	0x08
#define WDT_TCSR	0x0C

/* Register definition */
#define WDT_TCSR_PRESCALE_BIT	3
#define WDT_TCSR_PRESCALE_MASK	(0x7 << WDT_TCSR_PRESCALE_BIT)
  #define WDT_TCSR_PRESCALE1	(0x0 << WDT_TCSR_PRESCALE_BIT)
  #define WDT_TCSR_PRESCALE4	(0x1 << WDT_TCSR_PRESCALE_BIT)
  #define WDT_TCSR_PRESCALE16	(0x2 << WDT_TCSR_PRESCALE_BIT)
  #define WDT_TCSR_PRESCALE64	(0x3 << WDT_TCSR_PRESCALE_BIT)
  #define WDT_TCSR_PRESCALE256	(0x4 << WDT_TCSR_PRESCALE_BIT)
  #define WDT_TCSR_PRESCALE1024	(0x5 << WDT_TCSR_PRESCALE_BIT)
#define WDT_TCSR_EXT_EN		BIT(2)
#define WDT_TCSR_RTC_EN		BIT(1)
#define WDT_TCSR_PCK_EN		BIT(0)

#define WDT_TCER_TCEN		BIT(0)

void _machine_restart(void)
{
	void __iomem *wdt_regs = (void __iomem *)WDT_BASE;

	mdelay(100);

	/*
	 * Select the EXTAL as the timer clock input, and make each
	 * WDT clock tick equal 4 ticks of the system clock.
	 */
	writew(WDT_TCSR_PRESCALE4 | WDT_TCSR_EXT_EN, wdt_regs + WDT_TCSR);

	/* Reset the WDT counter to zero. */
	writew(0, wdt_regs + WDT_TCNT);

	/*
	 * Reset after 4ms
	 *
	 *         1 sec    CONFIG_SYS_EXTAL ticks
	 *  4ms * ------- * ---------------------- = Number of WDT clock ticks
	 *        1000 ms           1 sec
	 *
	 * As noted above the number of system clock ticks have been
	 * effectively multiplied by 4.  All that's left here for the
	 * computation of WDT clock ticks is to divide by 1000
	 * (one thousand).
	 */
	writew(JZ4780_SYS_EXTAL / 1000, wdt_regs + WDT_TDR);

	jz4780_tcu_wdt_start();

	/* WDT start */
	writeb(WDT_TCER_TCEN, wdt_regs + WDT_TCER);

	for (;;)
		;
}
