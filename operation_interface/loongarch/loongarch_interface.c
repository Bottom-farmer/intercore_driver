#include "../../internuclear/internuclear.h"

#define CORE0_IPI_BASE (0x1fe01000)
#define CORE1_IPI_BASE (0x1fe01100)
#define CORE_IPISR     (0x00)
#define CORE_IPIEN     (0x04)
#define CORE_IPISET    (0x08)
#define CORE_IPICLR    (0x0c)
#define CORE_BUF0      (0x20)
#define CORE_BUF1      (0x28)
#define CORE_BUF2      (0x30)
#define CORE_BUF3      (0x38)

static uint32_t secondary_flag = 0;
static DECLARE_WAIT_QUEUE_HEAD(secondary_wait_queue);

static void csr_mail_send(uint64_t data, int cpu, int mailbox)
{
    uint64_t val;

    /* Send high 32 bits */
    val = IOCSR_MBUF_SEND_BLOCKING;
    val |= (IOCSR_MBUF_SEND_BOX_HI(mailbox) << IOCSR_MBUF_SEND_BOX_SHIFT);
    val |= (cpu << IOCSR_MBUF_SEND_CPU_SHIFT);
    val |= (data & IOCSR_MBUF_SEND_H32_MASK);
    iocsr_write64(val, LOONGARCH_IOCSR_MBUF_SEND);

    /* Send low 32 bits */
    val = IOCSR_MBUF_SEND_BLOCKING;
    val |= (IOCSR_MBUF_SEND_BOX_LO(mailbox) << IOCSR_MBUF_SEND_BOX_SHIFT);
    val |= (cpu << IOCSR_MBUF_SEND_CPU_SHIFT);
    val |= (data << IOCSR_MBUF_SEND_BUF_SHIFT);
    iocsr_write64(val, LOONGARCH_IOCSR_MBUF_SEND);
};

static void csr_ipi_write_action(int cpu, uint32_t ipi_id)
{
    uint64_t val = IOCSR_IPI_SEND_BLOCKING;

    val |= (ipi_id - 1);
    val |= (cpu << IOCSR_IPI_SEND_CPU_SHIFT);
    iocsr_write32(val, LOONGARCH_IOCSR_IPI_SEND);
}

static int loongarch_cpu_online(unsigned long cpuid, unsigned long entry_point)
{
    pr_info("cpu %ld entry point %lx\n", cpuid, entry_point + 0x4c);
    csr_mail_send(entry_point + 0x4c, cpuid, 0);
    csr_ipi_write_action(cpuid, SMP_BOOT_CPU);

    return 0;
}

static int loongarch_irq_pending(uint8_t cpuid, unsigned int irq)
{
    uint32_t reg      = 0;
    uint32_t reg_base = 0;

    if(irq >= 8)
    {
        pr_info("ipi id %d is invalid\n", irq);
        return -1;
    }

    if(cpuid > 2)
    {
        pr_info("ipi id %d is invalid\n", irq);
        return -1;
    }

    reg_base = CORE1_IPI_BASE;
    reg      = 1 << (irq * 4);
    writel(reg, (void __iomem *)TO_UNCAC(reg_base + CORE_IPISET));

    return 0;
}

static void loongarch_ipi_callback(int cpu, void *param)
{
    wake_up_interruptible(&secondary_wait_queue);
    secondary_flag = 1;
}

static int loongarch_irq_install(unsigned int irq)
{
    amp_ipi_callbackfunc_register(irq, NULL, loongarch_ipi_callback);
    return 0;
}

static int __init loongarch_interface_init(void)
{
    pr_info("loongarch interface init.\n");
    pr_info("loongarch interface end.\n");

    return 0;
}

static internuclear_interface loongarch_interface_ops = {
    .secondary_wait_queue  = &secondary_wait_queue,
    .secondary_flag        = &secondary_flag,
    .secondary_init        = loongarch_interface_init,
    .secondary_irq_install = loongarch_irq_install,
    .secondary_cpu_online  = loongarch_cpu_online,
    .secondary_irq_pending = loongarch_irq_pending,
};

internuclear_interface_t internuclear_dev_get(void)
{
    return &loongarch_interface_ops;
}
