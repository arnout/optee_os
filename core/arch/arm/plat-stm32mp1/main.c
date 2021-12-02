// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2017-2022, STMicroelectronics
 * Copyright (c) 2016-2018, Linaro Limited
 */

#include <boot_api.h>
#include <config.h>
#include <console.h>
#include <drivers/gic.h>
#include <drivers/stm32_etzpc.h>
#include <drivers/stm32_firewall.h>
#include <drivers/stm32_iwdg.h>
#include <drivers/stm32_tamp.h>
#include <drivers/stm32_uart.h>
#include <drivers/stm32mp_dt_bindings.h>
#include <io.h>
#include <kernel/boot.h>
#include <kernel/dt.h>
#include <kernel/interrupt.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <kernel/spinlock.h>
#include <kernel/tee_common_otp.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <sm/psci.h>
#include <stm32_util.h>
#include <tee/tee_fs.h>
#include <trace.h>

register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB1_BASE, APB1_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB2_BASE, APB2_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB3_BASE, APB3_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB4_BASE, APB4_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, APB5_BASE, APB5_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, AHB4_BASE, AHB4_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_NSEC, AHB5_BASE, AHB5_SIZE);

register_phys_mem_pgdir(MEM_AREA_IO_SEC, APB1_BASE, APB1_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, APB3_BASE, APB3_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, APB4_BASE, APB4_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, APB5_BASE, APB5_SIZE);
#ifdef CFG_STM32MP13
register_phys_mem_pgdir(MEM_AREA_IO_SEC, APB6_BASE, APB6_SIZE);
#endif
register_phys_mem_pgdir(MEM_AREA_IO_SEC, AHB4_BASE, AHB4_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, AHB5_BASE, AHB5_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, GIC_BASE, GIC_SIZE);

register_ddr(DDR_BASE, CFG_DRAM_SIZE);

#ifdef CFG_RPROC_PTA
/* Map MCU RETRAM as read write for Cortex-M4 firmware management */
register_phys_mem(MEM_AREA_IO_SEC, RETRAM_BASE, RETRAM_SIZE);

/* Map MCU SRAM as read write for Cortex-M4 firmware management */
register_phys_mem(MEM_AREA_IO_SEC, MCUSRAM_BASE, MCUSRAM_SIZE);
#endif

#define _ID2STR(id)		(#id)
#define ID2STR(id)		_ID2STR(id)

static TEE_Result platform_banner(void)
{
#ifdef CFG_EMBED_DTB
	IMSG("Platform stm32mp1: flavor %s - DT %s",
		ID2STR(PLATFORM_FLAVOR),
		ID2STR(CFG_EMBED_DTB_SOURCE_FILE));
#else
	IMSG("Platform stm32mp1: flavor %s - no device tree",
		ID2STR(PLATFORM_FLAVOR));
#endif

	return TEE_SUCCESS;
}
service_init(platform_banner);

/*
 * Console
 *
 * CFG_STM32_EARLY_CONSOLE_UART specifies the ID of the UART used for
 * trace console. Value 0 disables the early console.
 *
 * We cannot use the generic serial_console support since probing
 * the console requires the platform clock driver to be already
 * up and ready which is done only once service_init are completed.
 */
static struct stm32_uart_pdata console_data;

void console_init(void)
{
	/* Early console initialization before MMU setup */
	struct uart {
		paddr_t pa;
		bool secure;
	} uarts[] = {
		[0] = { .pa = 0 },
		[1] = { .pa = USART1_BASE, .secure = true, },
		[2] = { .pa = USART2_BASE, .secure = false, },
		[3] = { .pa = USART3_BASE, .secure = false, },
		[4] = { .pa = UART4_BASE, .secure = false, },
		[5] = { .pa = UART5_BASE, .secure = false, },
		[6] = { .pa = USART6_BASE, .secure = false, },
		[7] = { .pa = UART7_BASE, .secure = false, },
		[8] = { .pa = UART8_BASE, .secure = false, },
	};

	COMPILE_TIME_ASSERT(ARRAY_SIZE(uarts) > CFG_STM32_EARLY_CONSOLE_UART);

	if (!uarts[CFG_STM32_EARLY_CONSOLE_UART].pa)
		return;

	/* No clock yet bound to the UART console */
	console_data.clock = NULL;

	console_data.secure = uarts[CFG_STM32_EARLY_CONSOLE_UART].secure;
	stm32_uart_init(&console_data, uarts[CFG_STM32_EARLY_CONSOLE_UART].pa);

	register_serial_console(&console_data.chip);

	IMSG("Early console on UART#%u", CFG_STM32_EARLY_CONSOLE_UART);
}

#ifdef CFG_EMBED_DTB
static TEE_Result init_console_from_dt(void)
{
	struct stm32_uart_pdata *pd = NULL;
	void *fdt = NULL;
	int node = 0;
	TEE_Result res = TEE_ERROR_GENERIC;

	fdt = get_embedded_dt();
	res = get_console_node_from_dt(fdt, &node, NULL, NULL);
	if (res == TEE_ERROR_ITEM_NOT_FOUND) {
		fdt = get_external_dt();
		res = get_console_node_from_dt(fdt, &node, NULL, NULL);
		if (res == TEE_ERROR_ITEM_NOT_FOUND)
			return TEE_SUCCESS;
		if (res != TEE_SUCCESS)
			return res;
	}

	pd = stm32_uart_init_from_dt_node(fdt, node);
	if (!pd) {
		IMSG("DTB disables console");
		register_serial_console(NULL);
		return TEE_SUCCESS;
	}

	/* Replace early console with the new one */
	console_flush();
	console_data = *pd;
	register_serial_console(&console_data.chip);
	IMSG("DTB enables console (%ssecure)", pd->secure ? "" : "non-");
	free(pd);

	return TEE_SUCCESS;
}

/* Probe console from DT once clock inits (service init level) are completed */
service_init_late(init_console_from_dt);
#endif


static uintptr_t stm32_dbgmcu_base(void)
{
	static void *va;

	if (!cpu_mmu_enabled())
		return DBGMCU_BASE;

	if (!va)
		va = phys_to_virt(DBGMCU_BASE, MEM_AREA_IO_SEC, 1);

	return (uintptr_t)va;
}

/* SoC versioning util, returns default ID if can't access DBGMCU */
TEE_Result stm32mp1_dbgmcu_get_chip_version(uint32_t *chip_version)
{
	uint32_t id = STM32MP1_CHIP_DEFAULT_VERSION;

	if (!chip_version)
		return TEE_ERROR_BAD_PARAMETERS;

	if (stm32_bsec_read_debug_conf() & BSEC_DBGSWGEN)
		id = (io_read32(stm32_dbgmcu_base() + DBGMCU_IDC) &
		      DBGMCU_IDC_REV_ID_MASK) >> DBGMCU_IDC_REV_ID_SHIFT;

	*chip_version = id;

	return TEE_SUCCESS;
}

/* SoC device ID util, returns default ID if can't access DBGMCU */
TEE_Result stm32mp1_dbgmcu_get_chip_dev_id(uint32_t *chip_dev_id)
{
	uint32_t id = STM32MP1_CHIP_ID;

	if (!chip_dev_id)
		return TEE_ERROR_BAD_PARAMETERS;

	if (stm32_bsec_read_debug_conf() & BSEC_DBGSWGEN)
		id = io_read32(stm32_dbgmcu_base() + DBGMCU_IDC) &
		     DBGMCU_IDC_DEV_ID_MASK;

	*chip_dev_id = id;

	return TEE_SUCCESS;
}
/*
 * GIC init, used also for primary/secondary boot core wake completion
 */
static struct gic_data gic_data;

void itr_core_handler(void)
{
	gic_it_handle(&gic_data);
}

void main_init_gic(void)
{
	gic_init(&gic_data, GIC_BASE + GICC_OFFSET, GIC_BASE + GICD_OFFSET);
	itr_init(&gic_data.chip);

	stm32mp_register_online_cpu();
}

void main_secondary_init_gic(void)
{
	gic_cpu_init(&gic_data);

	stm32mp_register_online_cpu();
}

#ifdef CFG_STM32MP13
#ifdef CFG_STM32_GPIO

#define NB_PINS_PER_BANK		U(16)
#define NB_PINS_BANK_H			U(14)
#define NB_PINS_BANK_I			U(8)

static TEE_Result set_all_gpios_non_secure(void)
{
	unsigned int bank = 0;
	unsigned int pin = 0;
	unsigned int nb_pin_bank = 0;

	for (bank = 0; bank <= GPIO_BANK_I; bank++) {
		switch (bank) {
		case GPIO_BANK_H:
			nb_pin_bank = NB_PINS_BANK_H;
			break;
		case GPIO_BANK_I:
			nb_pin_bank = NB_PINS_BANK_I;
			break;
		default:
			nb_pin_bank = NB_PINS_PER_BANK;
			break;
		}

		for (pin = 0; pin <= nb_pin_bank; pin++)
			stm32_gpio_set_secure_cfg(bank, pin, false);
	}

	return TEE_SUCCESS;
}

early_init_late(set_all_gpios_non_secure);
#endif /* CFG_STM32_GPIO */
#endif /* CFG_STM32MP13 */

static TEE_Result init_stm32mp1_drivers(void)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	const struct stm32_firewall_cfg sec_cfg[] = {
		{ FWLL_SEC_RW | FWLL_MASTER(0) },
		{ }, /* Null terminated */
	};

	/* Without secure DTB support, some drivers must be inited */
	if (!IS_ENABLED(CFG_EMBED_DTB))
		stm32_etzpc_init(ETZPC_BASE);

#ifdef CFG_TZSRAM_START
	COMPILE_TIME_ASSERT(((SYSRAM_BASE + SYSRAM_SIZE) <= CFG_TZSRAM_START) ||
			    ((SYSRAM_BASE <= CFG_TZSRAM_START) &&
			     (SYSRAM_SEC_SIZE >= CFG_TZSRAM_SIZE)));
#endif /* CFG_TZSRAM_START */

	res = stm32_firewall_set_config(SYSRAM_BASE, SYSRAM_SEC_SIZE, sec_cfg);
	if (res)
		panic("Unable to secure SYSRAM");

	return TEE_SUCCESS;
}

driver_init_late(init_stm32mp1_drivers);

static TEE_Result init_late_stm32mp1_drivers(void)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	/* Set access permission to TAM backup registers */
	if (IS_ENABLED(CFG_STM32_TAMP)) {
		struct stm32_bkpregs_conf conf = {
			.nb_zone1_regs = TAMP_BKP_REGISTER_ZONE1_COUNT,
			.nb_zone2_regs = TAMP_BKP_REGISTER_ZONE2_COUNT,
		};

		res = stm32_tamp_set_secure_bkpregs(&conf);
		if (res == TEE_ERROR_DEFER_DRIVER_INIT) {
			/* TAMP driver was not probed if disabled in the DT */
			res = TEE_SUCCESS;
		}
		if (res)
			panic();
	}

	return TEE_SUCCESS;
}

driver_init_late(init_late_stm32mp1_drivers);

vaddr_t stm32_rcc_base(void)
{
	static struct io_pa_va base = { .pa = RCC_BASE };

	return io_pa_or_va_secure(&base, 1);
}

vaddr_t get_gicd_base(void)
{
	struct io_pa_va base = { .pa = GIC_BASE + GICD_OFFSET };

	return io_pa_or_va_secure(&base, 1);
}

void stm32mp_get_bsec_static_cfg(struct stm32_bsec_static_cfg *cfg)
{
	cfg->base = BSEC_BASE;
	cfg->upper_start = STM32MP1_UPPER_OTP_START;
	cfg->max_id = STM32MP1_OTP_MAX_ID;
}

bool stm32mp_is_closed_device(void)
{
	uint32_t otp_id = 0;
	size_t bit_len = 0;
	uint32_t otp_value = 0;
	TEE_Result result = TEE_ERROR_GENERIC;

	if (stm32_bsec_find_otp_in_nvmem_layout(CFG0_OTP, &otp_id, &bit_len))
		panic();

	if (bit_len != 8)
		panic();

	/* Non closed_device platform expects fuse well programmed to 0 */
	result = stm32_bsec_shadow_read_otp(&otp_value, otp_id);
	if (!result && !(otp_value & BIT(CFG0_OTP_SECURED_POS)))
		return false;

	return true;
}

bool __weak stm32mp_with_pmic(void)
{
	return false;
}

uint32_t may_spin_lock(unsigned int *lock)
{
	if (!lock || !cpu_mmu_enabled())
		return 0;

	return cpu_spin_lock_xsave(lock);
}

void may_spin_unlock(unsigned int *lock, uint32_t exceptions)
{
	if (!lock || !cpu_mmu_enabled())
		return;

	cpu_spin_unlock_xrestore(lock, exceptions);
}

static vaddr_t stm32_tamp_base(void)
{
	static struct io_pa_va base = { .pa = TAMP_BASE };

	return io_pa_or_va_secure(&base, 1);
}

static vaddr_t bkpreg_base(void)
{
	return stm32_tamp_base() + TAMP_BKP_REGISTER_OFF;
}

vaddr_t stm32mp_bkpreg(unsigned int idx)
{
	return bkpreg_base() + (idx * sizeof(uint32_t));
}

#ifdef CFG_STM32_IWDG
TEE_Result stm32_get_iwdg_otp_config(paddr_t pbase,
				     struct stm32_iwdg_otp_data *otp_data)
{
	unsigned int idx = 0;
	uint32_t otp_cfg = 0;
	TEE_Result res = TEE_ERROR_GENERIC;
	uint32_t otp_value = 0;

	switch (pbase) {
	case IWDG1_BASE:
		idx = 0;
		break;
	case IWDG2_BASE:
		idx = 1;
		break;
	default:
		panic();
	}

	res = stm32_bsec_find_otp_in_nvmem_layout(HW2_OTP, &otp_cfg, NULL);
	if (res)
		panic("HW2 OTP not found");

	if (stm32_bsec_read_otp(&otp_value, otp_cfg))
		panic();

	otp_data->hw_enabled = otp_value &
			       BIT(idx + HW2_OTP_IWDG_HW_ENABLE_SHIFT);
	otp_data->disable_on_stop = otp_value &
				    BIT(idx + HW2_OTP_IWDG_FZ_STOP_SHIFT);
	otp_data->disable_on_standby = otp_value &
				       BIT(idx + HW2_OTP_IWDG_FZ_STANDBY_SHIFT);

	return TEE_SUCCESS;
}
#endif /*CFG_STM32_IWDG*/

#if defined(CFG_RPMB_FS)
bool plat_rpmb_key_is_ready(void)
{
	struct tee_hw_unique_key hwkey = { };

	if (tee_otp_get_hw_unique_key(&hwkey))
		return false;

	return stm32mp_is_closed_device();
}
#endif

static int get_chip_dev_id(uint32_t *dev_id)
{
#ifdef CFG_STM32MP13
	*dev_id = stm32mp_syscfg_get_chip_dev_id();
#else /* assume CFG_STM32MP15 */
	if (stm32mp1_dbgmcu_get_chip_dev_id(dev_id) != TEE_SUCCESS)
		return -1;
#endif
	return 0;
}

static int get_part_number(uint32_t *part_nb)
{
	static uint32_t part_number;
	uint32_t dev_id = 0;
	uint32_t otp = 0;
	size_t bit_len = 0;

	assert(part_nb);

	if (part_number) {
		*part_nb = part_number;
		return 0;
	}

	if (get_chip_dev_id(&dev_id) < 0)
		return -1;

	if (stm32_bsec_find_otp_in_nvmem_layout(PART_NUMBER_OTP,
						&otp, &bit_len))
		return -1;

	if (bit_len != PART_NUMBER_OTP_BIT_LENGTH)
		panic();

	if (stm32_bsec_read_otp(&part_number, otp))
		return -1;

	part_number = (part_number & PART_NUMBER_OTP_PART_MASK) >>
		      PART_NUMBER_OTP_PART_SHIFT;

	part_number = part_number | (dev_id << 16);

	*part_nb = part_number;

	return 0;
}

bool stm32mp_supports_hw_cryp(void)
{
	uint32_t part_number = 0;

	if (!IS_ENABLED(CFG_STM32_CRYP))
		return false;

	if (get_part_number(&part_number)) {
		DMSG("Cannot get part number\n");
		panic();
	}

	switch (part_number) {
#ifdef CFG_STM32MP13
	case STM32MP135F_PART_NB:
	case STM32MP135C_PART_NB:
	case STM32MP133F_PART_NB:
	case STM32MP133C_PART_NB:
	case STM32MP131F_PART_NB:
	case STM32MP131C_PART_NB:
		return true;
#endif
#ifdef CFG_STM32MP15
	case STM32MP157F_PART_NB:
	case STM32MP157C_PART_NB:
	case STM32MP153F_PART_NB:
	case STM32MP153C_PART_NB:
	case STM32MP151F_PART_NB:
	case STM32MP151C_PART_NB:
		return true;
#endif
	default:
		return false;
	}
}

bool stm32mp_supports_second_core(void)
{
	uint32_t __maybe_unused part_number = 0;

	if (IS_ENABLED(CFG_STM32MP13))
		return false;

	if (CFG_TEE_CORE_NB_CORE == 1)
		return false;

	if (get_part_number(&part_number)) {
		DMSG("Cannot get part number\n");
		panic();
	}

	switch (part_number) {
#ifdef CFG_STM32MP15
	case STM32MP151F_PART_NB:
	case STM32MP151D_PART_NB:
	case STM32MP151C_PART_NB:
	case STM32MP151A_PART_NB:
		return false;
#endif
	default:
		return true;
	}
}
