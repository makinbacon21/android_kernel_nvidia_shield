/*
 * Tegra Graphics Init for T210 Architecture Chips
 *
 * Copyright (c) 2011-2015, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/slab.h>
#include <linux/io.h>

#include <linux/tegra-powergate.h>
#include <linux/tegra-soc.h>

#include <tegra/mc.h>

#include "dev.h"
#include "nvhost_job.h"
#include "class_ids.h"
#include "scale3d.h"

#include "t210.h"
#include "t124/t124.h"
#include "host1x/host1x.h"
#include "t210_hardware.h"
#include "syncpt_t124.h"
#include "flcn/flcn.h"
#include "nvdec/nvdec.h"
#include "tsec/tsec.h"
#include "vi/vi.h"
#include "vii2c/vii2c.h"
#include "isp/isp.h"

#include "../../../../arch/arm/mach-tegra/iomap.h"

#include "chip_support.h"
#include "nvhost_scale.h"

#include "cg_regs.c"

#define HOST_EMC_FLOOR 204000000
#define HOST_NVDEC_EMC_FLOOR 102000000
#define TSEC_POWERGATE_DELAY 500

#define BIT64(nr) (1ULL << (nr))

static struct host1x_device_info host1x04_info = {
	.nb_channels	= T124_NVHOST_NUMCHANNELS,
	.nb_pts		= NV_HOST1X_SYNCPT_NB_PTS,
	.nb_mlocks	= NV_HOST1X_NB_MLOCKS,
	.initialize_chip_support = nvhost_init_t210_support,
	.pts_base	= 0,
	.pts_limit	= NV_HOST1X_SYNCPT_NB_PTS,
	.syncpt_policy	= SYNCPT_PER_CHANNEL,
};

struct nvhost_device_data t21_host1x_info = {
	.clocks			= {{"host1x", 81000000},
				   {"actmon", UINT_MAX}, {} },
	NVHOST_MODULE_NO_POWERGATE_IDS,
	.can_powergate		= true,
	.powergate_delay	= 50,
	.private_data		= &host1x04_info,
	.finalize_poweron = nvhost_host1x_finalize_poweron,
	.prepare_poweroff = nvhost_host1x_prepare_poweroff,
	.bond_out_id		= BOND_OUT_HOST1X,
};

#ifdef CONFIG_TEGRA_GRHOST_ISP
struct nvhost_device_data t21_isp_info = {
	.num_channels		= 1,
	.moduleid		= NVHOST_MODULE_ISP,
	.class			= NV_VIDEO_STREAMING_ISP_CLASS_ID,
	.modulemutexes		= {NVMODMUTEX_ISP_0},
	.exclusive		= true,
	/* HACK: Mark as keepalive until 1188795 is fixed */
	.keepalive		= true,
#ifdef TEGRA_POWERGATE_VE
	.powergate_ids		= {TEGRA_POWERGATE_VE, -1},
#else
	NVHOST_MODULE_NO_POWERGATE_IDS,
#endif
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.powergate_delay	= 500,
	.can_powergate		= true,
	.clocks			= {
		{ "isp", UINT_MAX, 0, TEGRA_MC_CLIENT_ISP },
		{"emc", 0, NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER} },
	.finalize_poweron	= nvhost_isp_t210_finalize_poweron,
	.prepare_poweroff	= nvhost_isp_t124_prepare_poweroff,
	.ctrl_ops		= &tegra_isp_ctrl_ops,
	.gather_filter_enabled	= true,
	.bond_out_id		= BOND_OUT_ISP,
};

struct nvhost_device_data t21_ispb_info = {
	.num_channels		= 1,
	.moduleid		= (1 << 16) | NVHOST_MODULE_ISP,
	.class			= NV_VIDEO_STREAMING_ISPB_CLASS_ID,
	.modulemutexes		= {NVMODMUTEX_ISP_1},
	.exclusive		= true,
	/* HACK: Mark as keepalive until 1188795 is fixed */
	.keepalive		= true,
#ifdef TEGRA_POWERGATE_VE2
	.powergate_ids		= {TEGRA_POWERGATE_VE2, -1},
#else
	NVHOST_MODULE_NO_POWERGATE_IDS,
#endif
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.powergate_delay	= 500,
	.can_powergate		= true,
	.clocks			= {
		{ "isp", UINT_MAX, 0, TEGRA_MC_CLIENT_ISPB },
		{"emc", 0, NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER} },
	.finalize_poweron	= nvhost_isp_t210_finalize_poweron,
	.prepare_poweroff	= nvhost_isp_t124_prepare_poweroff,
	.ctrl_ops		= &tegra_isp_ctrl_ops,
	.gather_filter_enabled	= true,
	.bond_out_id		= BOND_OUT_ISP,
};
#endif

#if defined(CONFIG_TEGRA_GRHOST_VI) || defined(CONFIG_TEGRA_GRHOST_VI_MODULE)
#ifdef CONFIG_VI_ONE_DEVICE
struct nvhost_device_data t21_vi_info = {
	.modulemutexes		= {NVMODMUTEX_VI_0},
	.exclusive		= true,
	.class			= NV_VIDEO_STREAMING_VI_CLASS_ID,
	/* HACK: Mark as keepalive until 1188795 is fixed */
	.keepalive		= true,
#ifdef TEGRA_POWERGATE_VE
	.powergate_ids		= {TEGRA_POWERGATE_VE, -1},
#else
	NVHOST_MODULE_NO_POWERGATE_IDS,
#endif
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.powergate_delay	= 500,
	.can_powergate		= true,
	.moduleid		= NVHOST_MODULE_VI,
	.clocks = {
		{"vi", UINT_MAX},
		{"csi", 0},
		{"cilab", 102000000},
		{"cilcd", 102000000},
		{"cile", 102000000},
		{"vii2c", 86400000},
		{"i2cslow", 1000000},
		{"emc", 0, NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER} },
	.ctrl_ops		= &tegra_vi_ctrl_ops,
	.num_channels		= 6,
	.slcg_notifier_enable	= true,
	.gather_filter_enabled	= true,
	.bond_out_id		= BOND_OUT_VI,
	.prepare_poweroff = nvhost_vi_prepare_poweroff,
	.finalize_poweron = nvhost_vi_finalize_poweron,
};
#else
struct nvhost_device_data t21_vib_info = {
	.modulemutexes		= {NVMODMUTEX_VI_1},
	.class			= NV_VIDEO_STREAMING_VI_CLASS_ID,
	.exclusive		= true,
	/* HACK: Mark as keepalive until 1188795 is fixed */
	.keepalive		= true,
	.clocks	= {
		{"vi", UINT_MAX},
		{"csi", UINT_MAX},
		{"emc", 0, NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER} },
#ifdef TEGRA_POWERGATE_VE
	.powergate_ids		= {TEGRA_POWERGATE_VE, -1},
#else
	NVHOST_MODULE_NO_POWERGATE_IDS,
#endif
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.powergate_delay	= 500,
	.can_powergate		= true,
	.moduleid		= NVHOST_MODULE_VI,
	.ctrl_ops		= &tegra_vi_ctrl_ops,
	.num_channels		= 1,
	.gather_filter_enabled	= true,
	.bond_out_id		= BOND_OUT_VI,
};

static struct platform_device tegra_vi01b_device = {
	.name		= "vi",
	.id		= 1, /* .1 on the dev node */
	.dev		= {
		.platform_data = &t21_vib_info,
	},
};

struct nvhost_device_data t21_vi_info = {
	.modulemutexes		= {NVMODMUTEX_VI_0},
	.class			= NV_VIDEO_STREAMING_VI_CLASS_ID,
	.exclusive		= true,
	/* HACK: Mark as keepalive until 1188795 is fixed */
	.keepalive		= true,
#ifdef TEGRA_POWERGATE_VE
	.powergate_ids		= {TEGRA_POWERGATE_VE, -1},
#else
	NVHOST_MODULE_NO_POWERGATE_IDS,
#endif
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.powergate_delay	= 500,
	.can_powergate		= true,
	.moduleid		= NVHOST_MODULE_VI,
	.clocks = {
		{"vi", UINT_MAX},
		{"csi", 0},
		{"cilab", 102000000},
		{"emc", 0, NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER} },
	.ctrl_ops		= &tegra_vi_ctrl_ops,
	.slave			= &tegra_vi01b_device,
	.num_channels		= 1,
	.gather_filter_enabled	= true,
	.bond_out_id		= BOND_OUT_VI,
	.prepare_poweroff = nvhost_vi_prepare_poweroff,
	.finalize_poweron = nvhost_vi_finalize_poweron,
};
#endif

#endif

#if defined(CONFIG_TEGRA_GRHOST_VII2C)
struct nvhost_device_data t21_vii2c_info = {
	.class			= NV_VIDEO_STREAMING_VII2C_CLASS_ID,
	.exclusive		= true,
	.keepalive		= true,
	.finalize_poweron	= nvhost_vii2c_finalize_poweron,
	.prepare_poweroff	= nvhost_vii2c_prepare_poweroff,
	.reset			= nvhost_vii2c_module_reset,
#ifdef TEGRA_POWERGATE_VE
	.powergate_ids		= {TEGRA_POWERGATE_VE, -1},
#else
	NVHOST_MODULE_NO_POWERGATE_IDS,
#endif
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.gather_filter_enabled	= true,
	.powergate_delay	= 500,
	.can_powergate		= true,
	.moduleid		= NVHOST_MODULE_VII2C,
	.clocks = {
		{"vii2c", 86400000},
		{"i2cslow", 1000000},
	},
	.num_channels		= 1,
};
#endif

struct nvhost_device_data t21_msenc_info = {
	.version		= NVHOST_ENCODE_FLCN_VER(5, 0),
	.class			= NV_VIDEO_ENCODE_NVENC_CLASS_ID,
	.modulemutexes		= {NVMODMUTEX_MSENC},
#ifdef TEGRA_POWERGATE_NVENC
	.powergate_ids		= { TEGRA_POWERGATE_NVENC, -1 },
#else
	NVHOST_MODULE_NO_POWERGATE_IDS,
#endif
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.powergate_delay	= 500,
	.can_powergate		= true,
	.clocks			= {{"msenc", UINT_MAX, 0, TEGRA_MC_CLIENT_MSENC},
				   {"emc", HOST_EMC_FLOOR} },
	.engine_cg_regs		= t21x_nvenc_gating_registers,
	.engine_can_cg		= true,
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_flcn_finalize_poweron,
	.moduleid		= NVHOST_MODULE_MSENC,
	.num_channels		= 1,
	.scaling_init		= nvhost_scale_init,
	.scaling_deinit		= nvhost_scale_deinit,
	.actmon_regs		= HOST1X_CHANNEL_ACTMON1_REG_BASE,
	.mamask_addr		= 0x0000184c,
	.mamask_val		= 0x3d,
	.borps_addr		= 0x00001850,
	.borps_val		= 0x2008,
	.actmon_enabled		= true,
	.gather_filter_enabled	= true,
	.firmware_name		= "nvhost_nvenc050.fw",
	.bond_out_id		= BOND_OUT_NVENC
};

struct nvhost_device_data t21_nvdec_info = {
	.version		= NVHOST_ENCODE_NVDEC_VER(2, 0),
	.class			= NV_NVDEC_CLASS_ID,
	.modulemutexes		= {NVMODMUTEX_NVDEC},
#ifdef TEGRA_POWERGATE_NVDEC
	.powergate_ids		= { TEGRA_POWERGATE_NVDEC, -1 },
#else
	NVHOST_MODULE_NO_POWERGATE_IDS,
#endif
	.clockgate_delay	= 10,
	.powergate_delay	= 500,
	.can_powergate		= true,
	/* Don't change clk indexing order */
	.clocks			= {{"nvdec", UINT_MAX, 0, TEGRA_MC_CLIENT_NVDEC},
				   {"sclk", 80000000},
				   {"emc", HOST_NVDEC_EMC_FLOOR,
				NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER} },
	.engine_cg_regs		= t21x_nvdec_gating_registers,
	.engine_can_cg		= true,
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_nvdec_finalize_poweron,
	.prepare_poweroff	= nvhost_nvdec_prepare_poweroff,
	.moduleid		= NVHOST_MODULE_NVDEC,
	.ctrl_ops		= &tegra_nvdec_ctrl_ops,
	.num_channels		= 1,
	.scaling_init		= nvhost_scale_init,
	.scaling_deinit		= nvhost_scale_deinit,
	.actmon_regs		= HOST1X_CHANNEL_ACTMON3_REG_BASE,
	.mamask_addr		= 0x0000164c,
	.mamask_val		= 0x3d,
	.borps_addr		= 0x00001650,
	.borps_val		= 0x2008,
	.gather_filter_enabled	= true,
	.actmon_enabled		= true,
	.bond_out_id		= BOND_OUT_NVDEC,
};

struct nvhost_device_data t21_nvjpg_info = {
	.version		= NVHOST_ENCODE_FLCN_VER(1, 0),
	.class			= NV_NVJPG_CLASS_ID,
	.modulemutexes		= {NVMODMUTEX_NVJPG},
#ifdef TEGRA_POWERGATE_NVJPG
	.powergate_ids		= { TEGRA_POWERGATE_NVJPG, -1 },
#else
	NVHOST_MODULE_NO_POWERGATE_IDS,
#endif
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.powergate_delay	= 500,
	.can_powergate		= true,
	.clocks			= { {"nvjpg", UINT_MAX, 0, TEGRA_MC_CLIENT_NVJPG},
				    {"emc", HOST_EMC_FLOOR} },
	.engine_cg_regs		= t21x_nvjpg_gating_registers,
	.engine_can_cg		= true,
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_flcn_finalize_poweron,
	.moduleid		= NVHOST_MODULE_NVJPG,
	.num_channels		= 1,
	.scaling_init		= nvhost_scale_init,
	.scaling_deinit		= nvhost_scale_deinit,
	.actmon_regs		= HOST1X_CHANNEL_ACTMON4_REG_BASE,
	.mamask_addr		= 0x0000144c,
	.mamask_val		= 0x3d,
	.borps_addr		= 0x00001450,
	.borps_val		= 0x2008,
	.actmon_enabled		= true,
	.gather_filter_enabled	= true,
	.bond_out_id		= BOND_OUT_NVJPG,
	.firmware_name		= "nvhost_nvjpg010.fw",
};

struct nvhost_device_data t21_tsec_info = {
	.num_channels		= 1,
	.modulemutexes		= {NVMODMUTEX_TSECA},
	.version		= NVHOST_ENCODE_TSEC_VER(1, 0),
	.class			= NV_TSEC_CLASS_ID,
	.exclusive		= false,
	.clocks			= {{"tsec", UINT_MAX, 0, TEGRA_MC_CLIENT_TSEC},
				   {"emc", HOST_EMC_FLOOR} },
	NVHOST_MODULE_NO_POWERGATE_IDS,
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.can_powergate		= true,
	.powergate_delay	= TSEC_POWERGATE_DELAY,
	.keepalive		= true,
	.moduleid		= NVHOST_MODULE_TSEC,
	.engine_can_cg		= true,
	.engine_cg_regs		= t21x_tsec_gating_registers,
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_tsec_finalize_poweron,
	.prepare_poweroff	= nvhost_tsec_prepare_poweroff,
	.gather_filter_enabled	= true,
	.bond_out_id		= BOND_OUT_TSEC,
};

struct nvhost_device_data t21_tsecb_info = {
	.num_channels		= 1,
	.modulemutexes		= {NVMODMUTEX_TSECB},
	.version		= NVHOST_ENCODE_TSEC_VER(1, 0),
	.class			= NV_TSECB_CLASS_ID,
	.exclusive		= true,
	.clocks			= {{"tsecb", UINT_MAX, 0, TEGRA_MC_CLIENT_TSECB},
				   {"emc", HOST_EMC_FLOOR} },
	NVHOST_MODULE_NO_POWERGATE_IDS,
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.can_powergate		= true,
	.powergate_delay	= TSEC_POWERGATE_DELAY,
	.keepalive		= true,
	.engine_can_cg		= true,
	.engine_cg_regs		= t21x_tsec_gating_registers,
	.poweron_reset		= true,
	.finalize_poweron	= nvhost_tsec_finalize_poweron,
	.prepare_poweroff	= nvhost_tsec_prepare_poweroff,
	.gather_filter_enabled	= true,
	.bond_out_id		= BOND_OUT_TSEC,
};
#ifdef CONFIG_ARCH_TEGRA_VIC

struct nvhost_device_data t21_vic_info = {
	.num_channels		= 1,
	.modulemutexes		= {NVMODMUTEX_VIC},
	.clocks			= {{"vic03", 140800000, 0, TEGRA_MC_CLIENT_VIC},
				   {"emc", HOST_EMC_FLOOR,
				   NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER},
				   {"vic_floor", 0,
				   NVHOST_MODULE_ID_CBUS_FLOOR},
				   {"emc_shared", 0,
				   NVHOST_MODULE_ID_EMC_SHARED}, {} },
	.version		= NVHOST_ENCODE_FLCN_VER(4, 0),
#ifdef TEGRA_POWERGATE_VIC
	.powergate_ids	= { TEGRA_POWERGATE_VIC, -1 },
#else
	NVHOST_MODULE_NO_POWERGATE_IDS,
#endif
	NVHOST_DEFAULT_CLOCKGATE_DELAY,
	.can_powergate		= true,
	.powergate_delay	= 500,
	.moduleid		= NVHOST_MODULE_VIC,
	.class			= NV_GRAPHICS_VIC_CLASS_ID,
	.engine_cg_regs		= t21x_vic_gating_registers,
	.engine_can_cg		= true,
	.poweron_toggle_slcg	= true,
	.finalize_poweron	= nvhost_vic_finalize_poweron,
	.scaling_init           = nvhost_scale3d_init,
	.scaling_deinit         = nvhost_scale3d_deinit,
	.scaling_post_cb	= &nvhost_scale3d_callback,
	.actmon_regs            = HOST1X_CHANNEL_ACTMON2_REG_BASE,
	.linear_emc		= true,
	.actmon_enabled         = true,
	.actmon_irq		= 13,
	.devfreq_governor	= "wmark_active",
	.serialize		= true,
	.firmware_name		= "vic04_ucode.bin",
	.gather_filter_enabled	= true,
	.bond_out_id		= BOND_OUT_VIC,
	.aggregate_constraints	= nvhost_vic_aggregate_constraints,
	.num_ppc		= 8,
};
#endif

#include "host1x/host1x_channel.c"

static void t210_set_nvhost_chanops(struct nvhost_channel *ch)
{
	if (ch)
		ch->ops = host1x_channel_ops;
}

static int nvhost_init_t210_channel_support(struct nvhost_master *host,
       struct nvhost_chip_support *op)
{
	op->nvhost_dev.set_nvhost_chanops = t210_set_nvhost_chanops;

	return 0;
}

static void t210_remove_support(struct nvhost_chip_support *op)
{
	kfree(op->priv);
	op->priv = NULL;
}

#include "host1x/host1x_cdma.c"
#include "host1x/host1x_syncpt.c"
#include "host1x/host1x_intr.c"
#define NVHOST_T210_ACTMON
#include "host1x/host1x_actmon_t124.c"
#include "host1x/host1x_debug.c"

int nvhost_init_t210_support(struct nvhost_master *host,
       struct nvhost_chip_support *op)
{
	int err;
	struct t124 *t210 = NULL;

	op->soc_name = "tegra21x";

	/* don't worry about cleaning up on failure... "remove" does it. */
	err = nvhost_init_t210_channel_support(host, op);
	if (err)
		return err;

	op->cdma = host1x_cdma_ops;
	op->push_buffer = host1x_pushbuffer_ops;
	op->debug = host1x_debug_ops;
	host->sync_aperture = host->aperture + HOST1X_CHANNEL_SYNC_REG_BASE;
	op->syncpt = host1x_syncpt_ops;
	op->intr = host1x_intr_ops;
	op->actmon = host1x_actmon_ops;

	t210 = kzalloc(sizeof(struct t124), GFP_KERNEL);
	if (!t210) {
		err = -ENOMEM;
		goto err;
	}

	t210->host = host;
	op->priv = t210;
	op->remove_support = t210_remove_support;

	return 0;

err:
	kfree(t210);

	op->priv = NULL;
	op->remove_support = NULL;
	return err;
}
