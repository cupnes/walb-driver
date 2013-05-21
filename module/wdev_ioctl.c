/**
 * wdev_ioctl.c - walb device ioctl.
 *
 * (C) 2013, Cybozu Labs, Inc.
 * @author HOSHINO Takashi <hoshino@labs.cybozu.co.jp>
 */
#include <linux/module.h>
#include "wdev_ioctl.h"
#include "wdev_util.h"
#include "kern.h"
#include "io.h"
#include "super.h"
#include "snapshot.h"
#include "alldevs.h"
#include "control.h"

/*******************************************************************************
 * Static functions prototype.
 *******************************************************************************/

/* Ioctl details. */
static int ioctl_wdev_get_oldest_lsid(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_set_oldest_lsid(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_status(struct walb_dev *wdev, struct walb_ctl *ctl); /* NYI */
static int ioctl_wdev_create_snapshot(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_delete_snapshot(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_delete_snapshot_range(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_get_snapshot(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_num_of_snapshot_range(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_list_snapshot_range(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_list_snapshot_from(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_take_checkpoint(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_get_checkpoint_interval(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_set_checkpoint_interval(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_get_written_lsid(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_get_permanent_lsid(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_get_completed_lsid(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_get_log_usage(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_get_log_capacity(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_is_flush_capable(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_resize(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_clear_log(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_is_log_overflow(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_freeze(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_is_frozen(struct walb_dev *wdev, struct walb_ctl *ctl);
static int ioctl_wdev_melt(struct walb_dev *wdev, struct walb_ctl *ctl);

/*******************************************************************************
 * Static functions definition.
 *******************************************************************************/

/**
 * Get oldest_lsid.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0.
 */
static int ioctl_wdev_get_oldest_lsid(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	LOGn("WALB_IOCTL_GET_OLDEST_LSID\n");
	ASSERT(ctl->command == WALB_IOCTL_GET_OLDEST_LSID);

	ctl->val_u64 = get_oldest_lsid(wdev);
	return 0;
}

/**
 * Set oldest_lsid.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_set_oldest_lsid(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	u64 lsid, oldest_lsid, written_lsid;

	LOGn("WALB_IOCTL_SET_OLDEST_LSID_SET\n");

	lsid = ctl->val_u64;

	spin_lock(&wdev->lsid_lock);
	written_lsid = wdev->lsids.written;
	oldest_lsid = wdev->lsids.oldest;
	spin_unlock(&wdev->lsid_lock);

	if (!(lsid == written_lsid ||
			(oldest_lsid <= lsid && lsid < written_lsid &&
				walb_check_lsid_valid(wdev, lsid)))) {
		LOGe("lsid %"PRIu64" is not valid.\n", lsid);
		LOGe("You shoud specify valid logpack header lsid"
			" (oldest_lsid (%"PRIu64") <= lsid <= written_lsid (%"PRIu64").\n",
			oldest_lsid, written_lsid);
		return -EFAULT;
	}

	spin_lock(&wdev->lsid_lock);
	wdev->lsids.oldest = lsid;
	spin_unlock(&wdev->lsid_lock);

	if (!walb_sync_super_block(wdev)) {
		LOGe("sync super block failed.\n");
		return -EFAULT;
	}
	return 0;
}

/**
 * Get status.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_status(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	/* not yet implemented */

	LOGn("WALB_IOCTL_STATUS is not supported currently.\n");
	return -EFAULT;
}

/**
 * Create a snapshot.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_create_snapshot(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	int error;
	struct walb_snapshot_record srec;

	LOGn("WALB_IOCTL_CREATE_SNAPSHOT\n");
	ASSERT(ctl->command == WALB_IOCTL_CREATE_SNAPSHOT);

	if (!get_snapshot_record_from_ctl_u2k(&srec, ctl)) {
		return -EFAULT;
	}
	if (srec.lsid == INVALID_LSID) {
		srec.lsid = get_completed_lsid(wdev);
		ASSERT(srec.lsid != INVALID_LSID);
	}
	if (!is_valid_snapshot_name(srec.name)) {
		LOGe("Snapshot name is invalid.\n");
		return -EFAULT;
	}
	LOGn("Create snapshot name %s lsid %"PRIu64" ts %"PRIu64"\n",
		srec.name, srec.lsid, srec.timestamp);
	error = snapshot_add(wdev->snapd, srec.name, srec.lsid, srec.timestamp);
	if (error) {
		ctl->error = error;
		return -EFAULT;
	}
	return 0;
}

/**
 * Delete a snapshot.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_delete_snapshot(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	struct walb_snapshot_record srec;
	int error;

	LOGn("WALB_IOCTL_DELETE_SNAPSHOT\n");
	ASSERT(ctl->command == WALB_IOCTL_DELETE_SNAPSHOT);

	if (!get_snapshot_record_from_ctl_u2k(&srec, ctl)) {
		return -EFAULT;
	}
	if (!is_valid_snapshot_name(srec.name)) {
		LOGe("Invalid snapshot name.\n");
		return -EFAULT;
	}
	error = snapshot_del(wdev->snapd, srec.name);
	if (error) {
		ctl->error = error;
		return -EFAULT;
	}
	return 0;
}

/**
 * Delete snapshots over a lsid range.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_delete_snapshot_range(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	u64 lsid0, lsid1;
	int ret;

	LOGn("WALB_IOCTL_DELETE_SNAPSHOT_RANGE");
	ASSERT(ctl->command == WALB_IOCTL_DELETE_SNAPSHOT_RANGE);

	if (!get_lsid_range_from_ctl(&lsid0, &lsid1, ctl)) {
		return -EFAULT;
	}
	ret = snapshot_del_range(wdev->snapd, lsid0, lsid1);
	if (ret >= 0) {
		ctl->val_int = ret;
	} else {
		ctl->error = ret;
		return -EFAULT;
	}
	return 0;
}

/**
 * Get a snapshot.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_get_snapshot(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	int ret;
	struct walb_snapshot_record srec0t;
	struct walb_snapshot_record *srec1, *srec;

	LOGn("WALB_IOCTL_GET_SNAPSHOT\n");
	ASSERT(ctl->command == WALB_IOCTL_GET_SNAPSHOT);

	if (!get_snapshot_record_from_ctl_u2k(&srec0t, ctl)) {
		return -EFAULT;
	}
	if (sizeof(struct walb_snapshot_record) > ctl->k2u.buf_size) {
		LOGe("buffer size too small.\n");
		return -EFAULT;
	}
	srec1 = (struct walb_snapshot_record *)ctl->k2u.kbuf; /* assign pointer. */
	if (!srec1) {
		LOGe("You must specify buffers for an output snapshot record.\n");
		return -EFAULT;
	}
	ret = snapshot_get(wdev->snapd, srec0t.name, &srec);
	if (!ret) {
		snapshot_record_init(srec1);
		ctl->error = 1;
		return -EFAULT;
	}
	*srec1 = *srec;
	return 0;
}

/**
 * Get number of snapshots over a lsid range.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_num_of_snapshot_range(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	u64 lsid0, lsid1;
	int ret;

	LOGn("WALB_IOCTL_NUM_OF_SNAPSHOT_RANGE\n");
	ASSERT(ctl->command == WALB_IOCTL_NUM_OF_SNAPSHOT_RANGE);

	if (!get_lsid_range_from_ctl(&lsid0, &lsid1, ctl)) {
		return -EFAULT;
	}
	ret = snapshot_n_records_range(
		wdev->snapd, lsid0, lsid1);
	if (ret < 0) {
		ctl->error = ret;
		return -EFAULT;
	}
	ctl->val_int = ret;
	return 0;
}

/**
 * List snapshots over a lsid range.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_list_snapshot_range(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	u64 lsid0, lsid1;
	struct walb_snapshot_record *srec;
	size_t size;
	int n_rec, ret;

	LOGn("WALB_IOCTL_LIST_SNAPSHOT_RANGE\n");
	ASSERT(ctl->command == WALB_IOCTL_LIST_SNAPSHOT_RANGE);

	if (!get_lsid_range_from_ctl(&lsid0, &lsid1, ctl)) {
		return -EFAULT;
	}
	srec = (struct walb_snapshot_record *)ctl->k2u.kbuf;
	size = ctl->k2u.buf_size / sizeof(struct walb_snapshot_record);
	if (size == 0) {
		LOGe("Buffer is to small for results.\n");
		return -EFAULT;
	}
	ret = snapshot_list_range(wdev->snapd, srec, size,
				lsid0, lsid1);
	if (ret < 0) {
		ctl->error = ret;
		return -EFAULT;
	}
	n_rec = ret;
	ctl->val_int = n_rec;
	if (n_rec > 0) {
		ASSERT(srec[n_rec - 1].lsid != INVALID_LSID);
		ctl->val_u64 = srec[n_rec - 1].lsid + 1;
	} else {
		ctl->val_u64 = INVALID_LSID;
	}
	return 0;
}

/**
 * List snapshots from a snapshot_id.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_list_snapshot_from(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	struct walb_snapshot_record *srec;
	size_t size;
	int n_rec, ret;
	u32 sid, next_sid;

	LOGn("WALB_IOCTL_LIST_SNAPSHOT_FROM\n");
	ASSERT(ctl->command == WALB_IOCTL_LIST_SNAPSHOT_FROM);

	sid = ctl->val_u32;
	srec = (struct walb_snapshot_record *)ctl->k2u.kbuf;
	size = ctl->k2u.buf_size / sizeof(struct walb_snapshot_record);
	if (size == 0) {
		LOGe("Buffer is to small for results.\n");
		return -EFAULT;
	}
	ret = snapshot_list_from(wdev->snapd, srec, size, sid);
	if (ret < 0) {
		ctl->error = ret;
		return -EFAULT;
	}
	n_rec = ret;
	ctl->val_int = n_rec;
	if (n_rec > 0) {
		ASSERT(srec[n_rec - 1].snapshot_id != INVALID_SNAPSHOT_ID);
		next_sid = srec[n_rec - 1].snapshot_id + 1;
	} else {
		next_sid = INVALID_SNAPSHOT_ID;
	}
	ctl->val_u32 = next_sid;
	return 0;
}

/**
 * Take a snapshot immedicately.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_take_checkpoint(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	bool ret;

	LOGn("WALB_IOCTL_TAKE_CHECKPOINT\n");
	ASSERT(ctl->command == WALB_IOCTL_TAKE_CHECKPOINT);

	stop_checkpointing(&wdev->cpd);
#ifdef WALB_DEBUG
	down_write(&wdev->cpd.lock);
	ASSERT(wdev->cpd.state == CP_STOPPED);
	up_write(&wdev->cpd.lock);
#endif
	ret = take_checkpoint(&wdev->cpd);
	if (!ret) {
		atomic_set(&wdev->is_read_only, 1);
		LOGe("superblock sync failed.\n");
		return -EFAULT;
	}
	start_checkpointing(&wdev->cpd);
	return 0;
}

/**
 * Get checkpoint interval.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0.
 */
static int ioctl_wdev_get_checkpoint_interval(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	LOGn("WALB_IOCTL_GET_CHECKPOINT_INTERVAL\n");
	ASSERT(ctl->command == WALB_IOCTL_GET_CHECKPOINT_INTERVAL);

	ctl->val_u32 = get_checkpoint_interval(&wdev->cpd);
	return 0;
}

/**
 * Set checkpoint interval.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_set_checkpoint_interval(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	u32 interval;

	LOGn("WALB_IOCTL_SET_CHECKPOINT_INTERVAL\n");
	ASSERT(ctl->command == WALB_IOCTL_SET_CHECKPOINT_INTERVAL);

	interval = ctl->val_u32;
	if (interval > WALB_MAX_CHECKPOINT_INTERVAL) {
		LOGe("Checkpoint interval is too big.\n");
		return -EFAULT;
	}
	set_checkpoint_interval(&wdev->cpd, interval);
	return 0;
}

/**
 * Get written_lsid.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0.
 */
static int ioctl_wdev_get_written_lsid(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	LOGn("WALB_IOCTL_GET_WRITTEN_LSID\n");
	ASSERT(ctl->command == WALB_IOCTL_GET_WRITTEN_LSID);

	ctl->val_u64 = get_written_lsid(wdev);
	return 0;
}

/**
 * Get permanent_lsid.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0.
 */
static int ioctl_wdev_get_permanent_lsid(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	LOGn("WALB_IOCTL_GET_PERMANENT_LSID\n");
	ASSERT(ctl->command == WALB_IOCTL_GET_PERMANENT_LSID);

	ctl->val_u64 = get_permanent_lsid(wdev);
	return 0;
}

/**
 * Get completed_lsid.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0.
 */
static int ioctl_wdev_get_completed_lsid(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	LOGn("WALB_IOCTL_GET_COMPLETED_LSID\n");
	ASSERT(ctl->command == WALB_IOCTL_GET_COMPLETED_LSID);

	ctl->val_u64 = get_completed_lsid(wdev);
	return 0;
}

/**
 * Get log usage.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0.
 */
static int ioctl_wdev_get_log_usage(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	LOGn("WALB_IOCTL_GET_LOG_USAGE\n");
	ASSERT(ctl->command == WALB_IOCTL_GET_LOG_USAGE);

	ctl->val_u64 = walb_get_log_usage(wdev);
	return 0;
}

/**
 * Get log capacity.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0.
 */
static int ioctl_wdev_get_log_capacity(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	LOGn("WALB_IOCTL_GET_LOG_CAPACITY\n");
	ASSERT(ctl->command == WALB_IOCTL_GET_LOG_CAPACITY);

	ctl->val_u64 = walb_get_log_capacity(wdev);
	return 0;
}

/**
 * Get flush request capable or not.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0.
 */
static int ioctl_wdev_is_flush_capable(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	LOGn("WALB_IOCTL_IS_FLUAH_CAPABLE");
	ASSERT(ctl->command == WALB_IOCTL_IS_FLUSH_CAPABLE);

	ctl->val_int = (wdev->queue->flush_flags & REQ_FLUSH) != 0;
	return 0;
}

/**
 * Resize walb device.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_resize(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	u64 ddev_size;
	u64 new_size;
	u64 old_size;

	LOGn("WALB_IOCTL_RESIZE.\n");
	ASSERT(ctl->command == WALB_IOCTL_RESIZE);

	old_size = get_capacity(wdev->gd);
	new_size = ctl->val_u64;
	ddev_size = wdev->ddev->bd_part->nr_sects;

	if (new_size == 0) {
		new_size = ddev_size;
	}
	if (new_size < old_size) {
		LOGe("Shrink size from %"PRIu64" to %"PRIu64" is not supported.\n",
			old_size, new_size);
		return -EFAULT;
	}
	if (new_size > ddev_size) {
		LOGe("new_size %"PRIu64" > data device capacity %"PRIu64".\n",
			new_size, ddev_size);
		return -EFAULT;
	}
	if (new_size == old_size) {
		LOGn("No need to resize.\n");
		return 0;
	}

	spin_lock(&wdev->size_lock);
	wdev->size = new_size;
	wdev->ddev_size = ddev_size;
	spin_unlock(&wdev->size_lock);

	if (!resize_disk(wdev->gd, new_size)) {
		return -EFAULT;
	}

	/* Sync super block for super->device_size */
	if (!walb_sync_super_block(wdev)) {
		LOGe("superblock sync failed.\n");
		return -EFAULT;
	}
	return 0;
}

/**
 * Clear log and detect resize of log device.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_clear_log(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	u64 new_ldev_size, old_ldev_size;
	u8 new_uuid[UUID_SIZE], old_uuid[UUID_SIZE];
	unsigned int pbs = wdev->physical_bs;
	bool is_grown = false;
	int ret;
	struct walb_super_sector *super;
	u64 lsid0_off;
	struct lsid_set lsids;
	u64 old_ring_buffer_size;
	u32 new_salt;

	ASSERT(ctl->command == WALB_IOCTL_CLEAR_LOG);
	LOGn("WALB_IOCTL_CLEAR_LOG.\n");

	/* Freeze iocore and checkpointing.  */
	iocore_freeze(wdev);
	stop_checkpointing(&wdev->cpd);

	/* Get old/new log device size. */
	old_ldev_size = wdev->ldev_size;
	new_ldev_size = wdev->ldev->bd_part->nr_sects;

	if (old_ldev_size > new_ldev_size) {
		LOGe("Log device shrink not supported.\n");
		goto error0;
	}

	/* Backup variables. */
	old_ring_buffer_size = wdev->ring_buffer_size;
	backup_lsid_set(wdev, &lsids);

	/* Initialize lsid(s). */
	spin_lock(&wdev->lsid_lock);
	wdev->lsids.latest = 0;
	wdev->lsids.flush = 0;
#ifdef WALB_FAST_ALGORITHM
	wdev->lsids.completed = 0;
#endif
	wdev->lsids.permanent = 0;
	wdev->lsids.written = 0;
	wdev->lsids.prev_written = 0;
	wdev->lsids.oldest = 0;
	spin_unlock(&wdev->lsid_lock);

	/* Grow the walblog device. */
	if (old_ldev_size < new_ldev_size) {
		LOGn("Detect log device size change.\n");

		/* Grow the disk. */
		is_grown = true;
		if (!resize_disk(wdev->log_gd, new_ldev_size)) {
			LOGe("grow disk failed.\n");
			iocore_set_readonly(wdev);
			goto error1;
		}
		LOGn("Grown log device size from %"PRIu64" to %"PRIu64".\n",
			old_ldev_size, new_ldev_size);
		wdev->ldev_size = new_ldev_size;

		/* Currently you can not change n_snapshots. */

		/* Recalculate ring buffer size. */
		wdev->ring_buffer_size =
			addr_pb(pbs, new_ldev_size)
			- get_ring_buffer_offset(pbs, wdev->n_snapshots);
	}

	/* Generate new uuid and salt. */
	get_random_bytes(new_uuid, 16);
	get_random_bytes(&new_salt, sizeof(new_salt));
	wdev->log_checksum_salt = new_salt;

	/* Update superblock image. */
	spin_lock(&wdev->lsuper0_lock);
	super = get_super_sector(wdev->lsuper0);
	memcpy(old_uuid, super->uuid, UUID_SIZE);
	memcpy(super->uuid, new_uuid, UUID_SIZE);
	super->ring_buffer_size = wdev->ring_buffer_size;
	super->log_checksum_salt = new_salt;
	/* super->snapshot_metadata_size; */
	lsid0_off = get_offset_of_lsid_2(super, 0);
	spin_unlock(&wdev->lsuper0_lock);

	/* Sync super sector. */
	if (!walb_sync_super_block(wdev)) {
		LOGe("sync superblock failed.\n");
		iocore_set_readonly(wdev);
		goto error2;
	}

	/* Update uuid index of alldev data. */
	alldevs_write_lock();
	ret = alldevs_update_uuid(old_uuid, new_uuid);
	alldevs_write_unlock();
	if (ret) {
		LOGe("Update alldevs index failed.\n");
		iocore_set_readonly(wdev);
		goto error2;
	}

	/* Invalidate first logpack */
	if (!invalidate_lsid(wdev, 0)) {
		LOGe("invalidate lsid 0 failed.\n");
		iocore_set_readonly(wdev);
		goto error2;
	}

	/* Delete all snapshots. */
	if (snapshot_del_range(wdev->snapd, 0, MAX_LSID + 1) < 0) {
		LOGe("Delete all snapshots failed.\n");
		iocore_set_readonly(wdev);
		goto error2;

	}
	ASSERT(snapshot_n_records(wdev->snapd) == 0);
	LOGn("Delete all snapshots done.\n");

	/* Clear log overflow. */
	iocore_clear_log_overflow(wdev);

	/* Melt iocore and checkpointing. */
	start_checkpointing(&wdev->cpd);
	iocore_melt(wdev);

	return 0;

error2:
	restore_lsid_set(wdev, &lsids);
	wdev->ring_buffer_size = old_ring_buffer_size;
#if 0
	wdev->ldev_size = old_ldev_size;
	if (!resize_disk(wdev->log_gd, old_ldev_size)) {
		LOGe("resize_disk to shrink failed.\n");
	}
#endif
error1:
	start_checkpointing(&wdev->cpd);
	iocore_melt(wdev);
error0:
	return -EFAULT;
}

/**
 * Check log space overflow.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0.
 */
static int ioctl_wdev_is_log_overflow(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	ASSERT(ctl->command == WALB_IOCTL_IS_LOG_OVERFLOW);
	LOGn("WALB_IOCTL_IS_LOG_OVERFLOW.\n");

	ctl->val_int = iocore_is_log_overflow(wdev);
	return 0;
}

/**
 * Freeze a walb device.
 * Currently write IOs will be frozen but read IOs will not.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_freeze(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	u32 timeout_sec;

	ASSERT(ctl->command == WALB_IOCTL_FREEZE);
	LOGn("WALB_IOCTL_FREEZE\n");

	/* Clip timeout value. */
	timeout_sec = ctl->val_u32;
	if (timeout_sec > 86400) {
		timeout_sec = 86400;
		LOGn("Freeze timeout has been cut to %"PRIu32" seconds.\n",
			timeout_sec);
	}

	cancel_melt_work(wdev);
	if (freeze_if_melted(wdev, timeout_sec)) {
		return 0;
	}
	return -EFAULT;
}

/**
 * Check whether the device is frozen or not.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0.
 */
static int ioctl_wdev_is_frozen(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	ASSERT(ctl->command == WALB_IOCTL_IS_FROZEN);
	LOGn("WALB_IOCTL_IS_FROZEN\n");

	mutex_lock(&wdev->freeze_lock);
	ctl->val_int = (wdev->freeze_state == FRZ_MELTED) ? 0 : 1;
	mutex_unlock(&wdev->freeze_lock);

	return 0;
}

/**
 * Melt a frozen device.
 *
 * @wdev walb dev.
 * @ctl ioctl data.
 * RETURN:
 *   0 in success, or -EFAULT.
 */
static int ioctl_wdev_melt(struct walb_dev *wdev, struct walb_ctl *ctl)
{
	ASSERT(ctl->command == WALB_IOCTL_MELT);
	LOGn("WALB_IOCTL_MELT\n");

	cancel_melt_work(wdev);
	if (melt_if_frozen(wdev, true)) {
		return 0;
	}
	return -EFAULT;
}

/*******************************************************************************
 * Global functions.
 *******************************************************************************/

/**
 * Execute ioctl for WALB_IOCTL_WDEV.
 *
 * return 0 in success, or -EFAULT.
 */
int walb_dispatch_ioctl_wdev(struct walb_dev *wdev, void __user *userctl)
{
	int ret = -EFAULT;
	struct walb_ctl *ctl;

	/* Get ctl data. */
	ctl = walb_get_ctl(userctl, GFP_KERNEL);
	if (!ctl) {
		LOGe("walb_get_ctl failed.\n");
		return -EFAULT;
	}

	/* Execute each command. */
	switch(ctl->command) {
	case WALB_IOCTL_GET_OLDEST_LSID:
		ret = ioctl_wdev_get_oldest_lsid(wdev, ctl);
		break;
	case WALB_IOCTL_SET_OLDEST_LSID:
		ret = ioctl_wdev_set_oldest_lsid(wdev, ctl);
		break;
	case WALB_IOCTL_TAKE_CHECKPOINT:
		ret = ioctl_wdev_take_checkpoint(wdev, ctl);
		break;
	case WALB_IOCTL_GET_CHECKPOINT_INTERVAL:
		ret = ioctl_wdev_get_checkpoint_interval(wdev, ctl);
		break;
	case WALB_IOCTL_SET_CHECKPOINT_INTERVAL:
		ret = ioctl_wdev_set_checkpoint_interval(wdev, ctl);
		break;
	case WALB_IOCTL_GET_WRITTEN_LSID:
		ret = ioctl_wdev_get_written_lsid(wdev, ctl);
		break;
	case WALB_IOCTL_GET_PERMANENT_LSID:
		ret = ioctl_wdev_get_permanent_lsid(wdev, ctl);
		break;
	case WALB_IOCTL_GET_COMPLETED_LSID:
		ret = ioctl_wdev_get_completed_lsid(wdev, ctl);
		break;
	case WALB_IOCTL_GET_LOG_USAGE:
		ret = ioctl_wdev_get_log_usage(wdev, ctl);
		break;
	case WALB_IOCTL_GET_LOG_CAPACITY:
		ret = ioctl_wdev_get_log_capacity(wdev, ctl);
		break;
	case WALB_IOCTL_IS_FLUSH_CAPABLE:
		ret = ioctl_wdev_is_flush_capable(wdev, ctl);
		break;
	case WALB_IOCTL_CREATE_SNAPSHOT:
		ret = ioctl_wdev_create_snapshot(wdev, ctl);
		break;
	case WALB_IOCTL_DELETE_SNAPSHOT:
		ret = ioctl_wdev_delete_snapshot(wdev, ctl);
		break;
	case WALB_IOCTL_DELETE_SNAPSHOT_RANGE:
		ret = ioctl_wdev_delete_snapshot_range(wdev, ctl);
		break;
	case WALB_IOCTL_GET_SNAPSHOT:
		ret = ioctl_wdev_get_snapshot(wdev, ctl);
		break;
	case WALB_IOCTL_NUM_OF_SNAPSHOT_RANGE:
		ret = ioctl_wdev_num_of_snapshot_range(wdev, ctl);
		break;
	case WALB_IOCTL_LIST_SNAPSHOT_RANGE:
		ret = ioctl_wdev_list_snapshot_range(wdev, ctl);
		break;
	case WALB_IOCTL_LIST_SNAPSHOT_FROM:
		ret = ioctl_wdev_list_snapshot_from(wdev, ctl);
		break;
	case WALB_IOCTL_STATUS:
		ret = ioctl_wdev_status(wdev, ctl);
		break;
	case WALB_IOCTL_RESIZE:
		ret = ioctl_wdev_resize(wdev, ctl);
		break;
	case WALB_IOCTL_CLEAR_LOG:
		ret = ioctl_wdev_clear_log(wdev, ctl);
		break;
	case WALB_IOCTL_IS_LOG_OVERFLOW:
		ret = ioctl_wdev_is_log_overflow(wdev, ctl);
		break;
	case WALB_IOCTL_FREEZE:
		ret = ioctl_wdev_freeze(wdev, ctl);
		break;
	case WALB_IOCTL_MELT:
		ret = ioctl_wdev_melt(wdev, ctl);
		break;
	case WALB_IOCTL_IS_FROZEN:
		ret = ioctl_wdev_is_frozen(wdev, ctl);
		break;
	default:
		LOGn("WALB_IOCTL_WDEV %d is not supported.\n",
			ctl->command);
	}

	/* Put ctl data. */
	if (walb_put_ctl(userctl, ctl) != 0) {
		LOGe("walb_put_ctl failed.\n");
		return -EFAULT;
	}
	return ret;
}

MODULE_LICENSE("Dual BSD/GPL");