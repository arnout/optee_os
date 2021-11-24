# 1GB and 512MB DDR targets do not locate secure DDR at the same place.
flavor_dts_file-157A_DK1 = stm32mp157a-dk1.dts
flavor_dts_file-157C_DK2 = stm32mp157c-dk2.dts
flavor_dts_file-157C_ED1 = stm32mp157c-ed1.dts
flavor_dts_file-157C_EV1 = stm32mp157c-ev1.dts

flavor_dts_file-135F_DK = stm32mp135f-dk.dts

flavorlist-cryp-512M = $(flavor_dts_file-157C_DK2) \
		       $(flavor_dts_file-135F_DK)

flavorlist-no_cryp-512M = $(flavor_dts_file-157A_DK1)

flavorlist-cryp-1G = $(flavor_dts_file-157C_ED1) \
		     $(flavor_dts_file-157C_EV1)

flavorlist-no_cryp = $(flavorlist-no_cryp-512M)

flavorlist-512M = $(flavorlist-cryp-512M) \
		  $(flavorlist-no_cryp-512M)

flavorlist-1G = $(flavorlist-cryp-1G)

flavorlist-MP15 = $(flavor_dts_file-157A_DK1) \
		  $(flavor_dts_file-157C_DK2) \
		  $(flavor_dts_file-157C_ED1) \
		  $(flavor_dts_file-157C_EV1)

flavorlist-MP13 = $(flavor_dts_file-135F_DK)

ifneq ($(PLATFORM_FLAVOR),)
ifeq ($(flavor_dts_file-$(PLATFORM_FLAVOR)),)
$(error Invalid platform flavor $(PLATFORM_FLAVOR))
endif
CFG_EMBED_DTB_SOURCE_FILE ?= $(flavor_dts_file-$(PLATFORM_FLAVOR))
endif

ifneq ($(filter $(CFG_EMBED_DTB_SOURCE_FILE),$(flavorlist-no_cryp)),)
$(call force,CFG_STM32_CRYP,n)
endif

ifneq ($(filter $(CFG_EMBED_DTB_SOURCE_FILE),$(flavorlist-MP13)),)
$(call force,CFG_STM32MP13,y)
endif

ifneq ($(filter $(CFG_EMBED_DTB_SOURCE_FILE),$(flavorlist-MP15)),)
$(call force,CFG_STM32MP15,y)
endif

# CFG_STM32MP1x switches are exclusive.
# - CFG_STM32MP15 is enabled for STM32MP15x-* targets (default)
# - CFG_STM32MP13 is enabled for STM32MP13x-* targets
ifeq ($(CFG_STM32MP13),y)
$(call force,CFG_STM32MP15,n)
else
$(call force,CFG_STM32MP15,y)
$(call force,CFG_STM32MP13,n)
endif
ifeq ($(call cfg-one-enabled,CFG_STM32MP15 CFG_STM32MP13),n)
$(error One of CFG_STM32MP15 CFG_STM32MP13 must be enabled)
endif
ifeq ($(call cfg-all-enabled,CFG_STM32MP15 CFG_STM32MP13),y)
$(error Only one of CFG_STM32MP15 CFG_STM32MP13 must be enabled)
endif

include core/arch/arm/cpu/cortex-a7.mk

$(call force,CFG_DRIVERS_CLK,y)
$(call force,CFG_GIC,y)
$(call force,CFG_INIT_CNTVOFF,y)
$(call force,CFG_PSCI_ARM32,y)
$(call force,CFG_REGULATOR_DRIVERS,y)
$(call force,CFG_SECURE_TIME_SOURCE_CNTPCT,y)
$(call force,CFG_SM_PLATFORM_HANDLER,y)
$(call force,CFG_STM32_SHARED_IO,y)

ifeq ($(CFG_STM32MP13),y)
$(call force,CFG_BOOT_SECONDARY_REQUEST,n)
$(call force,CFG_CORE_RESERVED_SHM,n)
$(call force,CFG_DRIVERS_CLK_FIXED,y)
$(call force,CFG_SECONDARY_INIT_CNTFRQ,n)
$(call force,CFG_STM32_GPIO,y)
$(call force,CFG_STM32_EXTI,y)
$(call force,CFG_STM32_RNG,n)
$(call force,CFG_STM32MP_CLK_CORE,y)
$(call force,CFG_STM32MP1_SHARED_RESOURCES,n)
$(call force,CFG_STM32MP13_CLK,y)
$(call force,CFG_TEE_CORE_NB_CORE,1)
$(call force,CFG_WITH_NSEC_GPIOS,n)
CFG_STM32MP_OPP_COUNT ?= 2
CFG_WITH_PAGER ?= n
endif # CFG_STM32MP13

ifeq ($(CFG_STM32MP15),y)
$(call force,CFG_BOOT_SECONDARY_REQUEST,y)
$(call force,CFG_DRIVERS_CLK_FIXED,n)
$(call force,CFG_SECONDARY_INIT_CNTFRQ,y)
$(call force,CFG_STM32_VREFBUF,n)
$(call force,CFG_STM32MP1_SHARED_RESOURCES,y)
$(call force,CFG_STM32MP15_CLK,y)
CFG_CORE_RESERVED_SHM ?= y
CFG_TEE_CORE_NB_CORE ?= 2
CFG_WITH_PAGER ?= y
endif # CFG_STM32MP15
CFG_DRAM_BASE    ?= 0xc0000000
CFG_TZDRAM_START ?= ($(CFG_DRAM_BASE) + $(CFG_DRAM_SIZE) - $(CFG_TZDRAM_SIZE))
CFG_SHMEM_START  ?= ($(CFG_TZDRAM_START) - $(CFG_SHMEM_SIZE))

CFG_WITH_LPAE ?= y
CFG_WITH_SOFTWARE_PRNG ?= y
CFG_MMAP_REGIONS ?= 23
CFG_DTB_MAX_SIZE ?= (256 * 1024)
CFG_CORE_ASLR ?= n
CFG_CORE_HEAP_SIZE ?= 49152

# Default disable RPC command shared memory allocation caching due to
# side effect on TEE session release by the Linux tee & optee drivers.
CFG_PREALLOC_RPC_CACHE ?= n

ifeq ($(CFG_EMBED_DTB_SOURCE_FILE),)
# Some drivers mandate DT support
$(call force,CFG_DRIVERS_CLK_DT,n)
$(call force,CFG_REGULATOR_FIXED,n)
$(call force,CFG_STM32_CRYP,n)
$(call force,CFG_STM32_GPIO,n)
$(call force,CFG_STM32_I2C,n)
$(call force,CFG_STM32_IWDG,n)
$(call force,CFG_STM32_TAMP,n)
$(call force,CFG_STM32_REGULATOR_GPIO,n)
$(call force,CFG_STM32_VREFBUF,y)
$(call force,CFG_STPMIC1,n)
$(call force,CFG_STM32MP1_SCMI_SIP,n)
$(call force,CFG_SCMI_PTA,n)
else
$(call force,CFG_DRIVERS_CLK_DT,y)
endif

ifneq ($(filter $(CFG_EMBED_DTB_SOURCE_FILE),$(flavorlist-512M)),)
CFG_TZDRAM_START ?= 0xde000000
CFG_DRAM_SIZE    ?= 0x20000000
endif

CFG_DRAM_BASE    ?= 0xc0000000
CFG_DRAM_SIZE    ?= 0x40000000
CFG_STM32MP1_SCMI_SHM_BASE ?= 0x2ffff000
CFG_STM32MP1_SCMI_SHM_SIZE ?= 0x00001000
ifeq ($(CFG_STM32MP15),y)
CFG_TZDRAM_START ?= 0xfe000000
ifeq ($(CFG_CORE_RESERVED_SHM),y)
CFG_TZDRAM_SIZE  ?= 0x01e00000
else
CFG_TZDRAM_SIZE  ?= 0x02000000
endif
CFG_TZSRAM_START ?= 0x2ffc0000
CFG_TZSRAM_SIZE  ?= 0x0003f000
ifeq ($(CFG_CORE_RESERVED_SHM),y)
CFG_SHMEM_START  ?= ($(CFG_TZDRAM_START) + $(CFG_TZDRAM_SIZE))
CFG_SHMEM_SIZE   ?= ($(CFG_DRAM_BASE) + $(CFG_DRAM_SIZE) - $(CFG_SHMEM_START))
endif
else
CFG_TZDRAM_SIZE  ?= 0x02000000
CFG_TZDRAM_START ?= ($(CFG_DRAM_BASE) + $(CFG_DRAM_SIZE) - $(CFG_TZDRAM_SIZE))
endif #CFG_STM32MP15

# Enable Early TA NVMEM for provisioning management
CFG_TA_STM32MP_NVMEM ?= y
ifeq ($(CFG_TA_STM32MP_NVMEM),y)
$(call force,CFG_BSEC_PTA,y,Mandated by CFG_TA_STM32MP_NVMEM)
CFG_IN_TREE_EARLY_TAS += stm32mp_nvmem/1a8342cc-81a5-4512-99fe-9e2b3e37d626
endif

# Enable BSEC Pseudo TA for fuses access management
CFG_BSEC_PTA ?= y
ifeq ($(CFG_BSEC_PTA),y)
$(call force,CFG_STM32_BSEC,y,Mandated by CFG_BSEC_PTA)
endif

CFG_REGULATOR_FIXED ?= y
CFG_STM32_BSEC ?= y
CFG_STM32_CRYP ?= y
CFG_STM32_ETZPC ?= y
CFG_STM32_GPIO ?= y
CFG_STM32_I2C ?= y
CFG_STM32_IWDG ?= y
CFG_STM32_REGULATOR_GPIO ?= y
CFG_STM32_RNG ?= y
CFG_STM32_RSTCTRL ?= y
CFG_STM32_TAMP ?= y
CFG_STM32_UART ?= y
CFG_STM32_VREFBUF ?= y
CFG_STM32MP1_CPU_OPP ?= y
CFG_STPMIC1 ?= y
CFG_TZC400 ?= y

ifeq ($(CFG_STM32_ETZPC),y)
$(call force,CFG_STM32_FIREWALL,y)
endif

ifeq ($(CFG_STPMIC1),y)
$(call force,CFG_STM32_I2C,y)
$(call force,CFG_STM32_GPIO,y)
endif

# if any crypto driver is enabled, enable the crypto-framework layer
ifeq ($(call cfg-one-enabled, CFG_STM32_CRYP),y)
$(call force,CFG_STM32_CRYPTO_DRIVER,y)
endif

CFG_DRIVERS_RSTCTRL ?= $(CFG_STM32_RSTCTRL)
$(eval $(call cfg-depends-all,CFG_STM32_RSTCTRL,CFG_DRIVERS_RSTCTRL))

CFG_WDT ?= $(CFG_STM32_IWDG)

# Platform specific configuration
CFG_STM32MP_PANIC_ON_TZC_PERM_VIOLATION ?= y

CFG_STM32MP1_SCMI_SIP ?= n
CFG_STM32_PWR_SIP ?= y
ifeq ($(CFG_STM32MP1_SCMI_SIP),y)
$(call force,CFG_SCMI_MSG_DRIVERS,y,Mandated by CFG_STM32MP1_SCMI_SIP)
$(call force,CFG_SCMI_MSG_SMT,y,Mandated by CFG_STM32MP1_SCMI_SIP)
$(call force,CFG_SCMI_MSG_SMT_FASTCALL_ENTRY,y,Mandated by CFG_STM32MP1_SCMI_SIP)
endif

# Default enable SCMI PTA support
CFG_SCMI_PTA ?= y
ifeq ($(CFG_SCMI_PTA),y)
$(call force,CFG_SCMI_MSG_DRIVERS,y,Mandated by CFG_SCMI_PTA)
$(call force,CFG_SCMI_MSG_SMT_THREAD_ENTRY,y,Mandated by CFG_SCMI_PTA)
CFG_CORE_OCALL ?= y
CFG_SCMI_MSG_SHM_MSG ?= y
CFG_SCMI_MSG_SMT ?= y
endif

CFG_SCMI_MSG_DRIVERS ?= n
ifeq ($(CFG_SCMI_MSG_DRIVERS),y)
$(call force,CFG_SCMI_MSG_CLOCK,y)
$(call force,CFG_SCMI_MSG_RESET_DOMAIN,y)
CFG_SCMI_MSG_SHM_MSG ?= y
CFG_SCMI_MSG_SMT ?= y
CFG_SCMI_MSG_SMT_THREAD_ENTRY ?= y
$(call force,CFG_SCMI_MSG_REGULATOR_CONSUMER,y)
$(call force,CFG_SCMI_MSG_VOLTAGE_DOMAIN,y)
endif

ifneq ($(CFG_WITH_SOFTWARE_PRNG),y)
CFG_HWRNG_PTA ?= y
endif
ifeq ($(CFG_HWRNG_PTA),y)
$(call force,CFG_STM32_RNG,y,Mandated by CFG_HWRNG_PTA)
$(call force,CFG_WITH_SOFTWARE_PRNG,n,Mandated by CFG_HWRNG_PTA)
$(call force,CFG_HWRNG_QUALITY,1024)
endif

# Provision enough threads to pass xtest
ifneq (,$(filter y,$(CFG_SCMI_PTA) $(CFG_STM32MP1_SCMI_SIP)))
ifeq ($(CFG_WITH_PAGER),y)
CFG_NUM_THREADS ?= 3
else
CFG_NUM_THREADS ?= 10
endif
endif

# Default enable some test facitilites
CFG_ENABLE_EMBEDDED_TESTS ?= y
CFG_WITH_STATS ?= y
CFG_WERROR ?= y

# Enable to allow debug
ifeq ($(CFG_TEE_CORE_DEBUG),y)
CFG_STM32_BSEC_WRITE ?= y
endif

# Default disable some support for pager memory size constraint
ifeq ($(CFG_WITH_PAGER),y)
CFG_TEE_CORE_DEBUG ?= n
CFG_UNWIND ?= n
CFG_LOCKDEP ?= n
CFG_TA_BGET_TEST ?= n
# Default disable early TA compression to support a smaller HEAP size
CFG_EARLY_TA_COMPRESS ?= n
CFG_CORE_HEAP_SIZE ?= 49152
endif

# Non-secure UART and GPIO/pinctrl for the output console
CFG_WITH_NSEC_GPIOS ?= y
CFG_WITH_NSEC_UARTS ?= y
# UART instance used for early console (0 disables early console)
CFG_STM32_EARLY_CONSOLE_UART ?= 4

# CFG_STM32MP15_HUK enables use of a HUK read from BSEC fuses.
# Disable the HUK by default as it requires a product specific configuration.
#
# Configuration must provide the HUK generation scheme. The following switches
# are exclusive and at least one must be eable when CFG_STM32MP15_HUK is enable.
# CFG_STM32MP15_HUK_BSEC_KEY makes platform HUK to be the raw fuses content.
# CFG_STM32MP15_HUK_BSEC_DERIVE_UID makes platform HUK to be the HUK fuses
# content derived with the device UID fuses content. See derivation scheme
# in stm32mp15_huk.c implementation.
CFG_STM32MP15_HUK ?= n

ifeq ($(CFG_STM32MP15_HUK),y)
ifeq (,$(CFG_STM32MP15_HUK_BSEC_KEY_0))
$(error Missing configuration switch CFG_STM32MP15_HUK_BSEC_KEY_0)
endif
ifeq (,$(CFG_STM32MP15_HUK_BSEC_KEY_1))
$(error Missing configuration switch CFG_STM32MP15_HUK_BSEC_KEY_1)
endif
ifeq (,$(CFG_STM32MP15_HUK_BSEC_KEY_2))
$(error Missing configuration switch CFG_STM32MP15_HUK_BSEC_KEY_2)
endif
ifeq (,$(CFG_STM32MP15_HUK_BSEC_KEY_3))
$(error Missing configuration switch CFG_STM32MP15_HUK_BSEC_KEY_3)
endif

CFG_STM32MP15_HUK_BSEC_KEY ?= y
CFG_STM32MP15_HUK_BSEC_DERIVE_UID ?= n
ifneq (y,$(call cfg-one-enabled,CFG_STM32MP15_HUK_BSEC_KEY CFG_STM32MP15_HUK_BSEC_DERIVE_UID))
$(error CFG_STM32MP15_HUK mandates one of CFG_STM32MP15_HUK_BSEC_KEY CFG_STM32MP15_HUK_BSEC_DERIVE_UID)
else ifeq ($(CFG_STM32MP15_HUK_BSEC_KEY)-$(CFG_STM32MP15_HUK_BSEC_DERIVE_UID),y-y)
$(error CFG_STM32MP15_HUK_BSEC_KEY and CFG_STM32MP15_HUK_BSEC_DERIVE_UID are exclusive)
endif
endif # CFG_STM32MP15_HUK

# Sanity on choice config switches
ifeq ($(call cfg-all-enabled,CFG_STM32MP15 CFG_STM32MP13),y)
$(error CFG_STM32MP13_CLK and CFG_STM32MP15_CLK are exclusive)
endif
