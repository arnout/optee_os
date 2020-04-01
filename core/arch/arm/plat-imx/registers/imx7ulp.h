/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright 2017-2019 NXP
 */

#ifndef __IMX7ULP_H__
#define __IMX7ULP_H__

#include <registers/imx7ulp-crm.h>

#define GIC_BASE		0x40020000
#define GIC_SIZE		0x8000
#define GICC_OFFSET		0x2000
#define GICD_OFFSET		0x1000

#define AIPS0_BASE		0x40000000
#define AIPS0_SIZE		0x800000
#define AIPS1_BASE		0x40800000
#define AIPS1_SIZE		0x800000
#define M4_AIPS_BASE		0x41000000
#define M4_AIPS_SIZE		0x100000
#define M4_AIPS0_BASE		0x41000000
#define M4_AIPS0_SIZE		0x80000
#define M4_AIPS1_BASE		0x41080000
#define M4_AIPS1_SIZE		0x80000

#define GPIOC_BASE		0x400f0000
#define GPIOD_BASE		0x400f0040
#define GPIOE_BASE		0x400f0080
#define GPIOF_BASE		0x400f00c0
#define TPM5_BASE		0x40260000
#define WDOG_BASE		0x403d0000
#define WDOG_SIZE		0x10
#define SCG1_BASE		0x403e0000
#define PCC2_BASE		0x403f0000
#define PMC1_BASE		0x40400000
#define SMC1_BASE		0x40410000
#define MMDC_BASE		0x40ab0000
#define IOMUXC1_BASE		0x40ac0000
#define MMDC_IO_BASE		0x40ad0000
#define PCC3_BASE		0x40b30000
#define SNVS_BASE		0x41070000
#define OCOTP_BASE		0x410A6000
#define OCOTP_SIZE		0x4000
#define PMC0_BASE		0x410a1000
#define SIM_BASE		0x410a3000
#define OCOTP_BASE		0x410A6000
#define OCOTP_SIZE		0x4000

#define CAAM_BASE		0x40240000
#define CAAM_SIZE		0x10000
#define UART4_BASE		0x402d0000
#define UART5_BASE		0x402e0000
#define UART6_BASE		0x40a60000
#define UART7_BASE		0x40a70000

#define IRAM_BASE		0x1FFFC000
#define IRAM_SIZE		0x4000

#define LP_OCRAM_START		IRAM_BASE

#endif /* __IMX7ULP_H__ */
