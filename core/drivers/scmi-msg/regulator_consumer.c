// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2020-2021, STMicroelectronics
 */

#include <assert.h>
#include <drivers/regulator.h>
#include <drivers/scmi-msg.h>
#include <drivers/scmi.h>
#include <drivers/scmi_regulator_consumer.h>
#include <initcall.h>
#include <kernel/boot.h>
#include <kernel/panic.h>
#include <libfdt.h>
#include <malloc.h>
#include <trace.h>

#define VERBOSE_VOLTD(_chan_id, _dom_id, fmt, ...) \
	FMSG("(channel %u/domain %u) " fmt, (_chan_id), (_dom_id), __VA_ARGS__)

struct scmi_voltd {
	uint32_t domain_id;
	struct rdev *rdev;
	bool enabled;
};

struct scmi_voltd_channel {
	unsigned int channel_id;
	struct scmi_voltd *voltd_array;
	size_t voltd_count;
};

struct scmi_voltd_channel *voltd_channel;
static size_t voltd_channel_count;

static struct scmi_voltd_channel *channel_from_id(unsigned int channel_id)
{
	size_t i = 0;

	for (i = 0; i < voltd_channel_count; i++)
		if (channel_id == voltd_channel[i].channel_id)
			return voltd_channel + i;

	return NULL;
}

static struct scmi_voltd *voltd_from_id(unsigned int channel_id,
					unsigned int domain_id)
{
	struct scmi_voltd_channel *chan = channel_from_id(channel_id);

	if (!chan || domain_id >= chan->voltd_count)
		return NULL;

	return chan->voltd_array + domain_id;
}

size_t plat_scmi_voltd_count(unsigned int channel_id)
{
	struct scmi_voltd_channel *chan = channel_from_id(channel_id);

	if (chan)
		return chan->voltd_count;

	return 0;
}

const char *plat_scmi_voltd_get_name(unsigned int channel_id,
				     unsigned int domain_id)
{
	struct scmi_voltd *voltd = voltd_from_id(channel_id, domain_id);

	if (!voltd || !voltd->rdev)
		return NULL;

	return voltd->rdev->reg_name;
}

int32_t plat_scmi_voltd_levels_array(unsigned int channel_id,
				     unsigned int domain_id,
				     size_t start_index,
				     long *microvolt, size_t *nb_elts)
{
	struct scmi_voltd *voltd = voltd_from_id(channel_id, domain_id);
	TEE_Result res = TEE_ERROR_GENERIC;
	uint16_t *levels = NULL;
	size_t full_count = 0;
	size_t out_count = 0;
	size_t i = 0;

	if (!voltd)
		return SCMI_NOT_FOUND;
	if (!voltd->rdev)
		return SCMI_DENIED;

	res = regulator_list_voltages(voltd->rdev, &levels, &full_count);
	if (res)
		return SCMI_GENERIC_ERROR;

	if (!microvolt) {
		*nb_elts = full_count - start_index;

		return SCMI_SUCCESS;
	}

	if (SUB_OVERFLOW(full_count, start_index, &out_count))
		return SCMI_GENERIC_ERROR;

	out_count = MIN(out_count, *nb_elts);

	VERBOSE_VOLTD(channel_id, domain_id,
		      "%zu levels, start %zu req %zu out %zu",
		      full_count, start_index, *nb_elts, out_count);

	for (i = 0; i < out_count; i++)
		microvolt[i] = (long)levels[start_index + i] * 1000;

	*nb_elts = out_count;

	return SCMI_SUCCESS;
}

long plat_scmi_voltd_get_level(unsigned int channel_id,
			       unsigned int domain_id)
{
	struct scmi_voltd *voltd = voltd_from_id(channel_id, domain_id);
	uint16_t level_mv = 0;

	if (!voltd)
		return SCMI_NOT_FOUND;
	if (!voltd->rdev)
		return SCMI_DENIED;

	if (regulator_get_voltage(voltd->rdev, &level_mv))
		return SCMI_GENERIC_ERROR;

	VERBOSE_VOLTD(channel_id, domain_id, "get voltage = %lduV",
		      (long)level_mv * 1000);

	return (long)level_mv * 1000;
}

int32_t plat_scmi_voltd_set_level(unsigned int channel_id,
				  unsigned int domain_id,
				  long microvolt)
{
	struct scmi_voltd *voltd = voltd_from_id(channel_id, domain_id);

	if (!voltd)
		return SCMI_NOT_FOUND;
	if (!voltd->rdev)
		return SCMI_DENIED;

	VERBOSE_VOLTD(channel_id, domain_id, "set voltage to %lduV", microvolt);

	if (regulator_set_voltage(voltd->rdev, microvolt / 1000))
		return SCMI_GENERIC_ERROR;

	return SCMI_SUCCESS;
}

int32_t plat_scmi_voltd_get_config(unsigned int channel_id,
				   unsigned int domain_id,
				   uint32_t *config)
{
	struct scmi_voltd *voltd = voltd_from_id(channel_id, domain_id);

	if (!voltd)
		return SCMI_NOT_FOUND;
	if (!voltd->rdev)
		return SCMI_DENIED;

	if (voltd->enabled)
		*config = SCMI_VOLTAGE_DOMAIN_CONFIG_ARCH_ON;
	else
		*config = SCMI_VOLTAGE_DOMAIN_CONFIG_ARCH_OFF;

	VERBOSE_VOLTD(channel_id, domain_id, "config %#"PRIx32, *config);

	return SCMI_SUCCESS;
}

int32_t plat_scmi_voltd_set_config(unsigned int channel_id,
				   unsigned int domain_id,
				   uint32_t config)
{
	struct scmi_voltd *voltd = voltd_from_id(channel_id, domain_id);

	if (!voltd)
		return SCMI_NOT_FOUND;
	if (!voltd->rdev)
		return SCMI_DENIED;

	VERBOSE_VOLTD(channel_id, domain_id, "set config to %u", config);

	switch (config) {
	case SCMI_VOLTAGE_DOMAIN_CONFIG_ARCH_ON:
		if (!voltd->enabled) {
			if (regulator_enable(voltd->rdev))
				return SCMI_GENERIC_ERROR;

			voltd->enabled = true;
		}

		return TEE_SUCCESS;
	case SCMI_VOLTAGE_DOMAIN_CONFIG_ARCH_OFF:
		if (voltd->enabled) {
			if (regulator_disable(voltd->rdev))
				return SCMI_GENERIC_ERROR;

			voltd->enabled = false;
		}

		return SCMI_SUCCESS;
	default:
		return SCMI_INVALID_PARAMETERS;
	}
}

static TEE_Result scmi_regulator_consumer_init(void)
{
	int node = 0;
	int subnode = 0;
	void *fdt = NULL;
	bool using_reg = false;
	bool using_name = false;
	const fdt32_t *cuint = NULL;
	unsigned int channel_id = 0;
	struct scmi_voltd_channel *chan = NULL;

	fdt = get_embedded_dt();
	if (!fdt)
		return TEE_SUCCESS;

	node = fdt_node_offset_by_compatible(fdt, -1,
					     "st,scmi-regulator-consumer");
	if (node < 0)
		return TEE_SUCCESS;

	/*
	 * Currently supports a single device
	 * TODO: add a reference to the SCMI agent/channel
	 */
	if (fdt_node_offset_by_compatible(fdt, node,
					  "st,scmi-regulator-consumer") !=
	    -FDT_ERR_NOTFOUND)
		panic();

	if (node < 0)
		return TEE_SUCCESS;

	cuint = fdt_getprop(fdt, node, "scmi-channel-id", NULL);
	if (!cuint) {
		DMSG("Missing property scmi-channel-id");
		panic();
	}
	channel_id = fdt32_to_cpu(*cuint);

	assert(!voltd_channel_count && !voltd_channel);
	voltd_channel_count = 1;
	voltd_channel = calloc(voltd_channel_count, sizeof(*voltd_channel));
	assert(voltd_channel);

	chan = voltd_channel;
	chan->channel_id = channel_id;

	fdt_for_each_subnode(subnode, fdt, node) {
		struct scmi_voltd *voltd = NULL;
		struct rdev *rdev = NULL;
		const char *name = NULL;
		uint32_t domain_id = 0;

		name = fdt_getprop(fdt, subnode, "voltd-name", NULL);
		if (!name)
			continue;

		/* Check that the given voltage domain name is unique */
		for (voltd = chan->voltd_array;
		     voltd < chan->voltd_array + chan->voltd_count; voltd++) {
			if (voltd->rdev &&
			    !strcmp(voltd->rdev->reg_name, name)) {
				EMSG("voltd-name %s re-used", name);
				return TEE_ERROR_GENERIC;
			}
		}

		rdev = regulator_get_by_regulator_name(name);
		if (!rdev) {
			EMSG("regulator %s not found, skipped", name);
			continue;
		}

		/* Get an ID for the domain */
		cuint = fdt_getprop(fdt, subnode, "reg", NULL);
		if (cuint) {
			domain_id = fdt32_to_cpu(*cuint);
			using_reg = true;
		} else {
			domain_id = chan->voltd_count;
			using_name = true;
		}

		if (using_reg && using_name) {
			EMSG("Mixed reg usage in device-tree");
			return TEE_ERROR_GENERIC;
		}

		/* Realloc voltd_array with zeroing new cells */
		voltd = calloc(chan->voltd_count + 1, sizeof(*voltd));
		if (!voltd)
			panic();

		memcpy(voltd, chan->voltd_array,
		       chan->voltd_count * sizeof(*voltd));
		chan->voltd_array = voltd;
		chan->voltd_count++;

		voltd = chan->voltd_array + domain_id;

		/* Check that the domain_id is not already used */
		if (voltd->rdev) {
			EMSG("Domain ID %"PRIu32" already used", domain_id);
			return TEE_ERROR_GENERIC;
		}

		voltd->rdev = rdev;

		/* Boot-on: non-secure will be able to disable if necessary */
		if (rdev->flags & REGUL_BOOT_ON) {
			if (regulator_enable(rdev))
				panic();

			voltd->enabled = true;
		}

		DMSG("scmi voltd shares %s (node %s) on domain ID %"PRIu32,
		     rdev->reg_name, rdev->desc->node_name, domain_id);
	}

	return TEE_SUCCESS;
}
driver_init_late(scmi_regulator_consumer_init);
