/**
 * simple_blk_bio_none.c - make_request_fn which do nothing.
 *
 * Copyright(C) 2012, Cybozu Labs, Inc.
 * @author HOSHINO Takashi <hoshino@labs.cybozu.co.jp>
 */
#include "check_kernel.h"
#include <linux/blkdev.h>
#include "simple_blk_bio.h"

/**
 * Make request that do nothing.
 * Currently do nothing.
 */
int simple_blk_bio_make_request(struct request_queue *q, struct bio *bio)
{
        bio_endio(bio, 0); /* do nothing */
        return 0; /* never retry */
}

/**
 * Do nothing.
 */
bool create_private_data(struct simple_blk_dev *sdev)
{
        return true;
}

/**
 * Do nothing.
 */
void destroy_private_data(struct simple_blk_dev *sdev)
{
}

/**
 * Do nothing.
 */
void customize_sdev(struct simple_blk_dev *sdev)
{
}

/* end of file. */