/**
 * ioctl.h - data structure definictions for walb ioctl interface.
 *
 * @author HOSHINO Takashi <hoshino@labs.cybozu.co.jp>
 */
#ifndef WALB_IOCTL_H
#define WALB_IOCTL_H

#include "walb.h"

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/ioctl.h>
#else /* __KERNEL__ */
#include <stdio.h>
#include <sys/ioctl.h>
#endif /* __KERNEL__ */

/**
 * If you want to assign device minor automatically, specify this.
 */
#define WALB_DYNAMIC_MINOR (-1U)

/**
 * Data structure for walb ioctl.
 */
struct walb_ctl_data {

	unsigned int wmajor; /* walb device major. */
	unsigned int wminor; /* walb device minor.
				walblog device minor is (wminor + 1). */
	unsigned int lmajor;  /* log device major. */
	unsigned int lminor;  /* log device minor. */
	unsigned int dmajor;  /* data device major. */
	unsigned int dminor;  /* data device minor. */
	
	/* These are used for other struct for each control command. */
	size_t buf_size; /* buffer size. */
#ifdef __KERNEL__
	void __user *buf;
#else
	void *buf; /* buffer pointer if data_size > 0. */
#endif
	void *__buf; /* used inside kernel. */
} __attribute__((packed));

/**
 * Data structure for walb ioctl to /dev/walb/control.
 */
struct walb_ctl {

	/* Command id. */
	int command;

	/* Used for integer value transfer. */
	int val_int;
	u32 val_u32;
	u64 val_u64;

	int error; /* error no. */
	
	/* For userland --> kernel. */
	struct walb_ctl_data u2k;

	/* For kernel --> userland. */
	struct walb_ctl_data k2u;
} __attribute__((packed));

/**
 * Print walb_ctl data for debug.
 */
static inline void print_walb_ctl(
	__attribute__((unused)) const struct walb_ctl *ctl)
{
	PRINT(KERN_DEBUG,
		"***** walb_ctl *****\n"
		"command: %d\n"
		"val_int: %d\n"
		"val_u32: %u\n"
		"val_u64: %"PRIu64"\n"
		"error: %d\n"
		 
		"u2k.wdevt: (%u:%u)\n"
		"u2k.ldevt: (%u:%u)\n"
		"u2k.ddevt: (%u:%u)\n"
		"u2k.buf_size: %zu\n"
		 
		"k2u.wdevt: (%u:%u)\n"
		"k2u.ldevt: (%u:%u)\n"
		"k2u.ddevt: (%u:%u)\n"
		"k2u.buf_size: %zu\n",
		ctl->command,
		ctl->val_int,
		ctl->val_u32,
		ctl->val_u64,
		ctl->error,
		 
		ctl->u2k.wmajor, ctl->u2k.wminor,
		ctl->u2k.lmajor, ctl->u2k.lminor,
		ctl->u2k.dmajor, ctl->u2k.dminor,
		ctl->u2k.buf_size,
		 
		ctl->k2u.wmajor, ctl->k2u.wminor,
		ctl->k2u.lmajor, ctl->k2u.lminor,
		ctl->k2u.dmajor, ctl->k2u.dminor,
		ctl->k2u.buf_size);
}

/**
 * Ioctl magic word for walb.
 */
#define WALB_IOCTL_ID 0xfe

/**
 * Ioctl command id.
 */
enum {
	WALB_IOCTL_VERSION_CMD = 0,
	WALB_IOCTL_CONTROL_CMD,
	WALB_IOCTL_WDEV_CMD,
};

/**
 * Ioctl command id.
 *
 * WALB_IOCTL_VERSION is for both. (currently each walb device only.)
 * WALB_IOCTL_CONTROL is for /dev/walb/control device.
 * WALB_IOCTL_WDEV is for each walb device.
 */
#define WALB_IOCTL_VERSION _IOR(WALB_IOCTL_ID, WALB_IOCTL_VERSION_CMD, u32)
#define WALB_IOCTL_CONTROL _IOWR(WALB_IOCTL_ID, WALB_IOCTL_CONTROL_CMD, struct walb_ctl)
#define WALB_IOCTL_WDEV _IOWR(WALB_IOCTL_ID, WALB_IOCTL_WDEV_CMD, struct walb_ctl)

/**
 * For walb_ctl.command.
 */
enum {
	WALB_IOCTL_DUMMY = 0,

	/****************************************
	 * For WALB_IOCTL_CONTROL
	 * The target is /dev/walb/control.
	 ****************************************/

	/*
	 * Start a walb device.
	 *
	 * INPUT:
	 *   ctl->u2k.lmajor, ctl->u2k.lminor as log device major/minor.
	 *   ctl->u2k.dmajor, ctl->u2k.dminor as data device major/minor.
	 *   ctl->u2k.buf as device name (ctl->u2k.buf_size < DISK_NAME_LEN).
	 *     You can specify NULL and 0.
	 *   ctl->u2k.wminor as walb device minor.
	 *     Specify WALB_DYNAMIC_MINOR for automatic assign.
	 * OUTPUT:
	 *   ctl->k2u.wmajor, ctl->k2u.wminor as walb device major/minor.
	 *   ctl->k2u.buf as device name (ctl->k2u.buf_size >= DISK_NAME_LEN).
	 *   ctl->error as error code.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_START_DEV,

	/*
	 * Stop a walb device.
	 *
	 * INPUT:
	 *   ctl->u2k.wmajor, ctl->u2k.wmajor as walb device major/minor.
	 * OUTPUT:
	 *   ctl->error as error code.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_STOP_DEV,

	/*
	 * Get walb device major number.
	 *
	 * INPUT:
	 *   None.
	 * OUTPUT:
	 *   ctl->k2u.wmajor as major number.
	 * RETURN:
	 *   0.
	 */
	WALB_IOCTL_GET_MAJOR,

	/*
	 * Get walb device data list.
	 *
	 * INPUT:
	 *   ctl->u2k.buf as unsigned int *minor.
	 *     ctl->u2k.buf_size >= sizeof(unsigned int) * 2.
	 *     Range: minor[0] <= minor < minor[1].
	 * OUTPUT:
	 *   ctl->k2u.buf as struct disk_data *ddata.
	 *   ctl->val_int as number of stored devices.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_LIST_DEV,

	/*
	 * Get numbr of walb devices.
	 *
	 * INPUT:
	 *   None.
	 * OUTPUT:
	 *   ctl->val_int as number of walb devices.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_NUM_OF_DEV,
	
	/****************************************
	 * For WALB_IOCTL_WDEV
	 * The targets are walb devices.
	 ****************************************/

	/*
	 * Get oldest_lsid.
	 *
	 * INPUT:
	 *   None.
	 * OUTPUT:
	 *   ctl->val_u64 as oldest_lsid
	 */
	WALB_IOCTL_GET_OLDEST_LSID,

	/*
	 * Set oldest_lsid.
	 *
	 * INPUT:
	 *   ctl->val_u64 as new oldest_lsid.
	 * OUTPUT:
	 *   None.
	 */
	WALB_IOCTL_SET_OLDEST_LSID,

	/*
	 * NOT YET IMPLEMENTED.
	 * ???
	 *
	 * INPUT:
	 * OUTPUT:
	 */
	WALB_IOCTL_SEARCH_LSID,

	/*
	 * NOT YET IMPLEMENTED.
	 * ???
	 *
	 * INPUT:
	 * OUTPUT:
	 */
	WALB_IOCTL_STATUS,

	/*
	 * Create a snapshot.
	 *
	 * INPUT:
	 *   ctl->u2k.buf as struct walb_snapshot_record *rec.
	 *   ctl->u2k.buf_size must > (struct walb_snapshot_record).
	 *     If rec->lsid is INVALID_LSID, then completed_lsid will be used.
	 * OUTPUT:
	 *   None.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_CREATE_SNAPSHOT,

	/*
	 * Delete a snapshot.
	 *
	 * INPUT:
	 *   ctl->u2k.buf as struct walb_snapshot_record *rec.
	 *     Only rec->name will be used.
	 * OUTPUT:
	 *   None.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_DELETE_SNAPSHOT,

	/*
	 * Delete all snapshots in a lsid range.
	 *
	 * INPUT:
	 *   ctl->u2k.buf as u64 *lsid.
	 *     ctl->u2k.buf_size must be >= sizeof(u64) * 2;
	 *     The range is lsid[0] <= lsid < lsid[1].
	 * OUTPUT:
	 *   ctl->val_int as the number of deleted records.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_DELETE_SNAPSHOT_RANGE,

	/*
	 * Get snapshot record.
	 *
	 * INPUT:
	 *   ctl->u2k.buf as struct walb_snapshot_record *rec0.
	 *     Only rec0->name will be used.
	 * OUTPUT:
	 *   ctl->k2u.buf as struct walb_snapshot_record *rec1.
	 *     rec1->name must be the same as rec1->name.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_GET_SNAPSHOT,

	/*
	 * Get number of snapshots in a lsid range.
	 *
	 * INPUT:
	 *   ctl->u2k.buf as u64 *lsid.
	 *     ctl->u2k.buf_size must be >= sizeof(u64) * 2;
	 *     The range is lsid[0] <= lsid < lsid[1].
	 * OUTPUT:
	 *   ctl->val_int as the number of deleted records.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_NUM_OF_SNAPSHOT_RANGE,

	/*
	 * Get snapshot records in a lsid range.
	 *
	 * INPUT:
	 *   ctl->u2k.buf as u64 *lsid.
	 *     ctl->u2k.buf_size must be >= sizeof(u64) * 2;
	 *     The range is lsid[0] <= lsid < lsid[1].
	 * OUTPUT:
	 *   ctl->k2u.buf as struct walb_snapshot_record *rec.
	 *   If the buffer size is small,
	 *   all matched records will not be filled.
	 *   ctl->val_int as the number of filled records.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_LIST_SNAPSHOT_RANGE,

	/*
	 * Get checkpoint interval.
	 *
	 * INPUT:
	 *   None.
	 * OUTPUT:
	 *   ctl->val_u32 as interval [ms].
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_GET_CHECKPOINT_INTERVAL,

	/*
	 * Take a checkpoint immediately.
	 *
	 * INPUT:
	 *   None.
	 * OUTPUT:
	 *   None.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_TAKE_CHECKPOINT,

	/*
	 * Set checkpoint interval.
	 *
	 * INPUT:
	 *   ctl->val_u32 as new interval [ms].
	 * OUTPUT:
	 *   None.
	 * RETURN:
	 *   0 in success, or -EFAULT.
	 */
	WALB_IOCTL_SET_CHECKPOINT_INTERVAL,

	/*
	 * Get written_lsid where all IO(s) which lsid < written_lsid
	 * have been written to the underlying both log and data devices.
	 *
	 * INPUT:
	 *   None
	 * OUTPUT:
	 *   ctl->val_u64 as written_lsid.
	 */
	WALB_IOCTL_GET_WRITTEN_LSID,

	/*
	 * Get completed_lsid where all IO(s) which lsid < completed_lsid
	 * have been completed.
	 * For easy algorithm, this is the same as written_lsid.
	 *
	 * INPUT:
	 *   None
	 * OUTPUT:
	 *   ctl->val_u64 as completed_lsid.
	 */
	WALB_IOCTL_GET_COMPLETED_LSID,

	/*
	 * Get log space capacity.
	 * INPUT:
	 *   None
	 * OUTPUT:
	 *   ctl->val_u64 as log space capacity [physical block].
	 */
	WALB_IOCTL_GET_LOG_CAPACITY,

	/*
	 * NOT YET IMPLEMENTED.
	 *
	 * Resize walb device.
	 *
	 * INPUT:
	 * OUTPUT:
	 * RETURN:
	 */
	WALB_IOCTL_RESIZE,

	/*
	 * NOT YET IMPLEMENTED.
	 *
	 * Clear all logs.
	 *
	 * This will revalidate the log space size
	 * when log device size has changed.
	 * This will create a new UUID.
	 */
	WALB_IOCTL_CLEAR_LOG,

	/*
	 * NOT YET IMPLEMENTED.
	 *
	 * Stop write IO processing for a specified period.
	 */
	WALB_IOCTL_FREEZE_TEMPORARILY,
	
	/* NIY means [N]ot [I]mplemented [Y]et. */
};

#endif /* WALB_IOCTL_H */
