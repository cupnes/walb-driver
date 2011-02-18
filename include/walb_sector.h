/**
 * walb_sector.h - Sector operations.
 *
 * @author HOSHINO Takashi <hoshino@labs.cybozu.co.jp>
 */
#ifndef _WALB_SECTOR_H
#define _WALB_SECTOR_H

#include "walb.h"

/*******************************************************************************
 * Data definitions.
 *******************************************************************************/

/**
 * Sector data in the memory.
 */
struct sector_data
{
        int size; /* sector size. */
        void *data; /* pointer to buffer. */
};

/**
 * Sector data ary in the memory.
 */
struct sector_data_array
{
        int size;
        struct sector_data **array;
};

/*******************************************************************************
 * Assertions.
 *******************************************************************************/

#define ASSERT_SECTOR_DATA(sect) ASSERT(is_valid_sector_data(sect))
#define ASSERT_SECTOR_DATA_ARRAY(sect_ary) is_valid_sector_data_array(sect_ary)

/*******************************************************************************
 * Prototypes.
 *******************************************************************************/

static inline int is_valid_sector_data(const struct sector_data *sect);
#ifdef __KERNEL__
static inline struct sector_data* sector_alloc(int sector_size, gfp_t gfp_mask);
#else
static inline struct sector_data* sector_alloc(int sector_size);
#endif
static inline void sector_free(struct sector_data *sect);
static inline void sector_copy(struct sector_data *dst, const struct sector_data *src);
static inline int is_same_size_sector(const struct sector_data *sect0,
                                      const struct sector_data *sect1);
static inline int sector_compare(const struct sector_data *sect0,
                                 const struct sector_data *sect1);

static inline int __is_valid_sector_data_array_detail(struct sector_data **ary, int size);
static inline int is_valid_sector_data_array(const struct sector_data_array *sect_ary);
#ifdef __KERNEL__
static inline struct sector_data_array* sector_data_array_alloc(
        int n_sectors, int sector_size, gfp_t mask);
#else
static inline struct sector_data_array* sector_data_array_alloc(
        int n_sectors, int sector_size);
#endif
static inline void sector_data_array_free(struct sector_data_array *sect_ary);
static inline struct sector_data* get_sector_data_in_array(
        struct sector_data_array *sect_ary, int idx);

/*******************************************************************************
 * Functions for sector data.
 *******************************************************************************/

/**
 * Check sector data is valid.
 *
 * @return Non-zero if valid, or 0.
 */
static inline int is_valid_sector_data(const struct sector_data *sect)
{
        return (sect != NULL && sect->size > 0 && sect->data != NULL);
}

/**
 * Allocate sector.
 *
 * @sector_size sector size.
 * @flag GFP flag. This is for kernel code only.
 *
 * @return pointer to allocated sector data in success, or NULL.
 */
#ifdef __KERNEL__
static inline struct sector_data* sector_alloc(int sector_size, gfp_t gfp_mask)
#else
static inline struct sector_data* sector_alloc(int sector_size)
#endif
{
        struct sector_data *sect;

        if (sector_size <= 0) {
#ifdef __KERNEL__
                printk(KERN_ERR "sector_size is 0 or negative %d.\n", sector_size);
#else
                fprintf(stderr, "sector_size is 0 or negative %d.\n", sector_size);
#endif
                goto error0;
        }
        sect = MALLOC(sizeof(struct sector_data), gfp_mask);
        if (sect == NULL) { goto error0; }
        
        sect->size = sector_size;

        sect->data = AMALLOC(sector_size, sector_size, gfp_mask);
        if (sect->data == NULL) { goto error1; }

        ASSERT_SECTOR_DATA(sect);
        return sect;

error1:
        FREE(sect);
error0:
        return NULL;
}

/**
 * Deallocate sector.
 *
 * This must be used for memory allocated with @sector_alloc().
 */
static inline void sector_free(struct sector_data *sect)
{
        if (sect && sect->data) {
                FREE(sect->data);
        }
        FREE(sect);
}

/**
 * Copy sector image.
 *
 * @dst destination sector.
 * @src source sector.
 *      dst->size >= src->size must be satisfied.
 */
static inline void sector_copy(struct sector_data *dst, const struct sector_data *src)
{
        ASSERT_SECTOR_DATA(dst);
        ASSERT_SECTOR_DATA(src);
        ASSERT(dst->size >= src->size);
        
        memcpy(dst->data, src->data, src->size);
}

/**
 * Check size of both sectors are same or not.
 *
 * @return 1 if same, or 0.
 */
static inline int is_same_size_sector(const struct sector_data *sect0,
                                      const struct sector_data *sect1)
{
        ASSERT_SECTOR_DATA(sect0);
        ASSERT_SECTOR_DATA(sect1);

        return (sect0->size == sect1->size ? 1 : 0);
}

/**
 * Compare sector image.
 *
 * @sect0 1st sector.
 * @sect1 2nd sector.
 *
 * @return 0 when their size and their image is completely same.
 */
static inline int sector_compare(const struct sector_data *sect0,
                                 const struct sector_data *sect1)
{
        ASSERT_SECTOR_DATA(sect0);
        ASSERT_SECTOR_DATA(sect1);

        if (is_same_size_sector(sect0, sect1)) {
                return memcmp(sect0->data, sect1->data, sect1->size);
        } else {
                return sect0->size - sect1->size;
        }
}

/*******************************************************************************
 * Functions for sector data array.
 *******************************************************************************/

/**
 * Check each sector data is valid.
 *
 * @return Non-zero if all sectors are valid, or 0.
 */
static inline int __is_valid_sector_data_array_detail(struct sector_data **ary, int size)
{
        int i;

        if (ary == NULL) { return 0; }
        
        for (i = 0; i < size; i ++) {
                if (! is_valid_sector_data(ary[i])) { return 0; }
        }
        return 1;
}

/**
 * Check sector data array.
 * 
 * @return Non-zero if valid, or 0.
 */
static inline int is_valid_sector_data_array(const struct sector_data_array *sect_ary)
{
        return (sect_ary != NULL &&
                sect_ary->size > 0 &&
                __is_valid_sector_data_array_detail(sect_ary->array, sect_ary->size));
}

/**
 * Allocate sector data array.
 *
 * @n number of sectors.
 *    You should satisfy n <= PAGE_SIZE / sizeof(void*) inside kernel.
 * @sector_size sector size.
 *
 * @return pointer to allocated sector data array in success, or NULL.
 */
#ifdef __KERNEL__
static inline struct sector_data_array* sector_data_array_alloc(
        int n_sectors, int sector_size, gfp_t mask)
#else
static inline struct sector_data_array* sector_data_array_alloc(
        int n_sectors, int sector_size)
#endif
{
        int i;
        struct sector_data_array *sect_ary;
        struct sector_data *sect;

        ASSERT(n_sectors > 0);
        ASSERT(sector_size > 0);
        
        /* For itself. */
        sect_ary = MALLOC(sizeof(struct sector_data_array), mask);
        if (! sect_ary) { goto nomem0; }
        
        /* For array of sector pointer. */
        sect_ary->size = n_sectors;
        sect_ary->array = (struct sector_data **)
                ZALLOC(sizeof(struct sector_data *) * n_sectors, mask);
        if (! sect_ary->array) { goto nomem1; }

        /* For each sector. */
        for (i = 0; i < n_sectors; i ++) {
#ifdef __KERNEL__
                sect = sector_alloc(sector_size, mask);
#else
                sect = sector_alloc(sector_size);
#endif                
                if (! sect) { goto nomem1; }
                sect_ary->array[i] = sect;
        }
        
        return sect_ary;
nomem1:
        sector_data_array_free(sect_ary);
nomem0:
        return NULL;
}

/**
 * Deallocate sector data array.
 *
 * @sect_ary sector data array to deallocate.
 */
static inline void sector_data_array_free(struct sector_data_array *sect_ary)
{
        int i;

        if (sect_ary && sect_ary->array) {

                ASSERT_SECTOR_DATA_ARRAY(sect_ary);
                
                for (i = 0; i < sect_ary->size; i ++) {
                        sector_free(sect_ary->array[i]);
                }
                FREE(sect_ary->array);
        }
        FREE(sect_ary);
}

/**
 * Get sector data in sector data array.
 *
 * @sect_ary sector data ary.
 * @idx index in the array.
 *
 * @return pointer to sector data.
 */
static inline struct sector_data* get_sector_data_in_array(
        struct sector_data_array *sect_ary, int idx)
{
        ASSERT_SECTOR_DATA_ARRAY(sect_ary);
        ASSERT(0 <= idx && idx < sect_ary->size);
        return sect_ary->array[idx];
}

#endif /* _WALB_SECTOR_H */