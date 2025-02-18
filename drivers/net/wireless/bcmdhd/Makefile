# bcmdhd
#####################
# SDIO Basic feature
#####################

DHDCFLAGS += -Wall -Wstrict-prototypes -Dlinux -DLINUX -DBCMDRIVER            \
	-DBCMDONGLEHOST -DUNRELEASEDCHIP -DBCMDMA32 -DBCMFILEIMAGE            \
	-DDHDTHREAD -DSHOW_EVENTS -DBCMDBG -DWLP2P                            \
	-DWIFI_ACT_FRAME -DARP_OFFLOAD_SUPPORT                                \
	-DKEEP_ALIVE -DCSCAN -DPKT_FILTER_SUPPORT                             \
	-DEMBEDDED_PLATFORM -DPNO_SUPPORT                                     \
	-DDHD_DONOT_FORWARD_BCMEVENT_AS_NETWORK_PKT -DGET_CUSTOM_MAC_ENABLE   \
	-DCUSTOMER_HW2 -DENABLE_INSMOD_NO_FW_LOAD -DCUSTOM_COUNTRY_LOCALE     \
	-DNV_COUNTRY_CODE

#################
# Common feature
#################
DHDCFLAGS += -DWL_CFG80211
# Print out kernel panic point of file and line info when assertion happened
DHDCFLAGS += -DBCMASSERT_LOG

# keepalive
DHDCFLAGS += -DCUSTOM_KEEP_ALIVE_SETTING=28000

DHDCFLAGS += -DVSDB

# For p2p connection issue
DHDCFLAGS += -DWL_SCB_TIMEOUT=10

# TDLS enable
DHDCFLAGS += -DWLTDLS -DWLTDLS_AUTO_ENABLE
# For TDLS tear down inactive time 40 sec
DHDCFLAGS += -DCUSTOM_TDLS_IDLE_MODE_SETTING=40000
# for TDLS RSSI HIGH for establishing TDLS link
DHDCFLAGS += -DCUSTOM_TDLS_RSSI_THRESHOLD_HIGH=-60
# for TDLS RSSI HIGH for tearing down TDLS link
DHDCFLAGS += -DCUSTOM_TDLS_RSSI_THRESHOLD_LOW=-70

# Roaming
DHDCFLAGS += -DROAM_AP_ENV_DETECTION
DHDCFLAGS += -DROAM_ENABLE -DROAM_CHANNEL_CACHE -DROAM_API
DHDCFLAGS += -DENABLE_FW_ROAM_SUSPEND
# Roaming trigger
DHDCFLAGS += -DCUSTOM_ROAM_TRIGGER_SETTING=-75
DHDCFLAGS += -DCUSTOM_ROAM_DELTA_SETTING=10
# Set PM 2 always regardless suspend/resume
DHDCFLAGS += -DSUPPORT_PM2_ONLY

# For special PNO Event keep wake lock for 10sec
DHDCFLAGS += -DCUSTOM_PNO_EVENT_LOCK_xTIME=10
DHDCFLAGS += -DMIRACAST_AMPDU_SIZE=8

# Early suspend
DHDCFLAGS += -DDHD_USE_EARLYSUSPEND

# For Scan result patch
DHDCFLAGS += -DESCAN_RESULT_PATCH

# For Static Buffer
ifeq ($(CONFIG_BROADCOM_WIFI_RESERVED_MEM),y)
  DHDCFLAGS += -DCONFIG_DHD_USE_STATIC_BUF
  DHDCFLAGS += -DENHANCED_STATIC_BUF
  DHDCFLAGS += -DSTATIC_WL_PRIV_STRUCT
endif
ifneq ($(CONFIG_DHD_USE_SCHED_SCAN),)
DHDCFLAGS += -DWL_SCHED_SCAN
endif

# Ioctl timeout 5000ms
DHDCFLAGS += -DIOCTL_RESP_TIMEOUT=5000

# Prevent rx thread monopolize
DHDCFLAGS += -DWAIT_DEQUEUE

# Config PM Control
DHDCFLAGS += -DCONFIG_CONTROL_PM

# idle count
DHDCFLAGS += -DDHD_USE_IDLECOUNT

# SKB TAILPAD to avoid out of boundary memory access
DHDCFLAGS += -DDHDENABLE_TAILPAD

# Wi-Fi Direct
DHDCFLAGS += -DWL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST
DHDCFLAGS += -DWL_CFG80211_STA_EVENT
DHDCFLAGS += -DWL_IFACE_COMB_NUM_CHANNELS
DHDCFLAGS += -DWL_ENABLE_P2P_IF

##########################
# driver type
# m: module type driver
# y: built-in type driver
##########################
DRIVER_TYPE ?= $(CONFIG_BCMDHD)

#########################
# Chip dependent feature
#########################
ifneq ($(CONFIG_BCM43241),)
  DHDCFLAGS += -DBCM43241_CHIP
  DHDCFLAGS += -DCUSTOM_SDIO_F2_BLKSIZE=256
  DHDCFLAGS += -DAMPDU_HOSTREORDER
  DHDCFLAGS += -DCUSTOM_AMPDU_BA_WSIZE=32
  DHDCFLAGS += -DPROP_TXSTATUS_VSDB
  DHDCFLAGS += -DCUSTOM_DPC_PRIO_SETTING=MAX_USER_RT_PRIO/2
  DHDCFLAGS += -DRXFRAME_THREAD
  DHDCFLAGS += -DDHDTCPACK_SUPPRESS
  DHDCFLAGS += -DCUSTOM_DPC_CPUCORE=0
  DHDCFLAGS += -DVSDB_BW_ALLOCATE_ENABLE
  DHDCFLAGS += -DSDIO_CRC_ERROR_FIX
  DHDCFLAGS += -DP2P_DISCOVERY_WAR
endif
ifeq ($(DRIVER_TYPE),m)
  DHDCFLAGS += -fno-pic
endif

ifneq ($(CONFIG_BCM4354),)
  DHDCFLAGS += -DBCM4354_CHIP

# tput enhancement
  DHDCFLAGS += -DCUSTOM_GLOM_SETTING=8 -DCUSTOM_RXCHAIN=1 -DRXCB
  DHDCFLAGS += -DUSE_DYNAMIC_F2_BLKSIZE -DDYNAMIC_F2_BLKSIZE_FOR_NONLEGACY=128
  DHDCFLAGS += -DBCMSDIOH_TXGLOM -DCUSTOM_TXGLOM=1 -DBCMSDIOH_TXGLOM_HIGHSPEED
  DHDCFLAGS += -DDHDTCPACK_SUPPRESS
  DHDCFLAGS += -DUSE_WL_TXBF
  DHDCFLAGS += -DUSE_WL_FRAMEBURST
  DHDCFLAGS += -DRXFRAME_THREAD
  DHDCFLAGS += -DREPEAT_READFRAME
  DHDCFLAGS += -DCUSTOM_AMPDU_BA_WSIZE=64
  DHDCFLAGS += -DCUSTOM_DPC_CPUCORE=0
# DHDCFLAGS += -DPROP_TXSTATUS_VSDB
  DHDCFLAGS += -DCUSTOM_MAX_TXGLOM_SIZE=40
  DHDCFLAGS += -DMAX_HDR_READ=128
  DHDCFLAGS += -DDHD_FIRSTREAD=128
  DHDCFLAGS += -DCUSTOM_AMPDU_MPDU=16
  DHDCFLAGS += -DDISABLE_ROAM_EVENT

# New Features
  DHDCFLAGS += -DSUPPORT_P2P_GO_PS
  DHDCFLAGS += -DWL11U
  DHDCFLAGS += -DMFP
  DHDCFLAGS += -DDHD_ENABLE_LPC
  DHDCFLAGS += -DCUSTOM_PSPRETEND_THR=30
  DHDCFLAGS += -DCUSTOM_SDIO_F2_BLKSIZE=256

# Make same as proven good value from other platform
  DHDCFLAGS += -DCUSTOM_DPC_PRIO_SETTING=45

endif

ifneq ($(CONFIG_BCM4339),)
  DHDCFLAGS += -DBCM4339_CHIP

  # tput enhancement
  DHDCFLAGS += -DCUSTOM_GLOM_SETTING=8 -DCUSTOM_RXCHAIN=1
  DHDCFLAGS += -DUSE_DYNAMIC_F2_BLKSIZE -DDYNAMIC_F2_BLKSIZE_FOR_NONLEGACY=128
  DHDCFLAGS += -DBCMSDIOH_TXGLOM -DCUSTOM_TXGLOM=1 -DBCMSDIOH_TXGLOM_HIGHSPEED
  DHDCFLAGS += -DDHDTCPACK_SUPPRESS
  DHDCFLAGS += -DUSE_WL_TXBF
  DHDCFLAGS += -DUSE_WL_FRAMEBURST
  DHDCFLAGS += -DRXFRAME_THREAD
  DHDCFLAGS += -DCUSTOM_AMPDU_BA_WSIZE=64
  DHDCFLAGS += -DCUSTOM_DPC_CPUCORE=0
# DHDCFLAGS += -DPROP_TXSTATUS_VSDB
  DHDCFLAGS += -DCUSTOM_MAX_TXGLOM_SIZE=32

  # New Features
  DHDCFLAGS += -DWL11U
  DHDCFLAGS += -DDHD_ENABLE_LPC
  DHDCFLAGS += -DCUSTOM_PSPRETEND_THR=30
endif

ifneq ($(CONFIG_BCMDHD_SDIO),)
  DHDCFLAGS += -DBDC -DDHD_BCMEVENTS -DMMC_SDIO_ABORT
  DHDCFLAGS += -DBCMSDIO -DBCMLXSDMMC -DUSE_SDIOFIFO_IOVAR
  DHDCFLAGS += -DPROP_TXSTATUS
  DHDCFLAGS += -DQMONITOR
endif

ifeq ($(CONFIG_BCMDHD_HW_OOB),y)
  DHDCFLAGS += -DHW_OOB -DOOB_INTR_ONLY
else
  DHDCFLAGS += -DSDIO_ISR_THREAD
endif

ifneq ($(CONFIG_BCMDHD_PCIE),)
  DHDCFLAGS += -DPCIE_FULL_DONGLE -DBCMPCIE -DCUSTOM_DPC_PRIO_SETTING=-1
endif

#EXTRA_LDFLAGS += --strip-debug

EXTRA_CFLAGS += $(DHDCFLAGS) -DDHD_DEBUG
EXTRA_CFLAGS += -DSRCBASE=\"$(src)\"
EXTRA_CFLAGS += -I$(src)/include/ -I$(src)/
KBUILD_CFLAGS += -I$(LINUXDIR)/include -I$(shell pwd)

DHDOFILES := dhd_pno.o dhd_common.o dhd_ip.o dhd_custom_gpio.o \
	dhd_linux.o dhd_linux_sched.o dhd_cfg80211.o dhd_linux_wq.o aiutils.o bcmevent.o \
	bcmutils.o bcmwifi_channels.o hndpmu.o linux_osl.o sbutils.o siutils.o \
	wl_android.o wl_cfg80211.o wl_cfgp2p.o wl_cfg_btcoex.o wldev_common.o wl_linux_mon.o  \
	dhd_linux_platdev.o dhd_pno.o dhd_linux_wq.o wl_cfg_btcoex.o \
	hnd_pktq.o hnd_pktpool.o

ifneq ($(CONFIG_BCMDHD_SDIO),)
  DHDOFILES += bcmsdh.o bcmsdh_linux.o bcmsdh_sdmmc.o bcmsdh_sdmmc_linux.o
  DHDOFILES += dhd_cdc.o dhd_wlfc.o dhd_sdio.o dhd_qmon.o
endif

ifneq ($(CONFIG_BCMDHD_PCIE),)
  DHDOFILES += dhd_pcie.o dhd_pcie_linux.o dhd_msgbuf.o dhd_flowring.o
  DHDOFILES += pcie_core.o
endif

ifneq ($(CONFIG_BCMDHD_CUSTOM_SYSFS_TEGRA),)
  DHDOFILES += dhd_custom_sysfs_tegra.o
  DHDOFILES += dhd_custom_sysfs_tegra_ping.o
  DHDOFILES += dhd_custom_sysfs_tegra_rssi.o
  DHDOFILES += dhd_custom_sysfs_tegra_scan.o
  DHDOFILES += dhd_custom_sysfs_tegra_stat.o
  DHDOFILES += dhd_custom_sysfs_tegra_tcpdump.o
  DHDOFILES += dhd_custom_sysfs_tegra_netstat.o
endif

bcmdhd-objs := $(DHDOFILES)
obj-$(DRIVER_TYPE)   += bcmdhd.o

all:
	@echo "$(MAKE) --no-print-directory -C $(KDIR) SUBDIRS=$(CURDIR) modules"
	@$(MAKE) --no-print-directory -C $(KDIR) SUBDIRS=$(CURDIR) modules

clean:
	rm -rf *.o *.ko *.mod.c *~ .*.cmd *.o.cmd .*.o.cmd \
	Module.symvers modules.order .tmp_versions modules.builtin

install:
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules_install
