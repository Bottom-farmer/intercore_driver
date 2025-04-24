#ifndef __DEV_DOCK_H_
#define __DEV_DOCK_H_

#include <linux/acpi.h>
#include <linux/arm-smccc.h>
#include <linux/cdev.h>
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/poll.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/psci.h>
#include <linux/reboot.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>
#include <uapi/linux/psci.h>
#include <asm/pgtable.h>

#define SHARED_MEMORY_SIZE 0x1000000
typedef struct internuclear_interface {
    wait_queue_head_t *secondary_wait_queue;
    uint32_t          *secondary_flag;
    int (*secondary_init)(void);
    int (*secondary_irq_install)(unsigned int irq);
    int (*secondary_cpu_online)(unsigned long cpuid, unsigned long entry_point);
    int (*secondary_irq_pending)(uint8_t cpuid, unsigned int irq);
} internuclear_interface;

typedef internuclear_interface *internuclear_interface_t;

internuclear_interface_t internuclear_dev_get(void);

#endif
