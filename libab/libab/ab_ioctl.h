#ifndef __AB_IOCTL_H__
#define __AB_IOCTL_H__

#include <linux/ioctl.h>

#define BOARD_SLOT_FREE  (-1)
#define BOARDS_MAX         4
#define DEVS_PER_BOARD_MAX 4
#define CHANS_PER_DEV      2

#define SGATAB_IOC_MAGIC 's'

#define SGAB_GET_BOARDS_COUNT _IO(SGATAB_IOC_MAGIC, 1)
#define SGAB_GET_BOARD_PARAMS _IO(SGATAB_IOC_MAGIC, 2)

typedef enum dev_type_e {
	dev_type_ABSENT = 0x0,
	dev_type_FXO = 0x1,
	dev_type_VF  = 0x2,
	dev_type_FXS = 0x3
} dev_type_t;

typedef struct ab_dev_params_s {
	dev_type_t type;
	unsigned long nBaseAddress;
	unsigned char AccessMode;
	unsigned char chans_idx [CHANS_PER_DEV];
} ab_dev_params_t;

/* IOCTL Structs */
typedef struct ab_board_params_s {
	unsigned char board_idx; /**< get from user */
	unsigned char is_present; /**< return identifier */
	unsigned char is_count_changed; /**< return identifier */
	long nIrqNum;
	ab_dev_params_t devices [DEVS_PER_BOARD_MAX];
} ab_board_params_t;

#endif /* __AB_IOCTL_H__ */

