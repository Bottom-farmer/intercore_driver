#include "../../internuclear/internuclear.h"

#define ICC_SGI0R_EL1           "S3_0_C12_C11_7"
#define ICC_SGI1R_EL1           "S3_0_C12_C11_5"
#define GET_GICV3_REG(reg, out) __asm__ volatile("mrs %0, " reg : "=r"(out)::"memory");
#define SET_GICV3_REG(reg, in)  __asm__ volatile("msr " reg ", %0" ::"r"(in) : "memory");

static const struct of_device_id __initconst psci_of_match[] __initconst = {
    {.compatible = "arm,psci"},
    {.compatible = "arm,psci-0.2"},
    {.compatible = "arm,psci-1.0"},
    {},
};

typedef unsigned long(psci_fn)(unsigned long, unsigned long, unsigned long, unsigned long);
static psci_fn *invoke_psci_fn = NULL;
static uint32_t secondary_flag = 0;
static DECLARE_WAIT_QUEUE_HEAD(secondary_wait_queue);

static unsigned long __invoke_psci_fn_hvc(unsigned long function_id, unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
    struct arm_smccc_res res;

    arm_smccc_hvc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
    return res.a0;
}

static unsigned long __invoke_psci_fn_smc(unsigned long function_id, unsigned long arg0, unsigned long arg1, unsigned long arg2)
{
    struct arm_smccc_res res;

    arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
    return res.a0;
}

static void psci_init(void)
{
    struct device_node *np;
    const char         *method;

    np = of_find_matching_node_and_match(NULL, psci_of_match, NULL);

    if(!np || !of_device_is_available(np))
        return;

    if(of_property_read_string(np, "method", &method))
    {
        pr_warn("missing \"method\" property\n");
        return;
    }

    of_node_put(np);

    if(!strcmp("hvc", method))
    {
        invoke_psci_fn = __invoke_psci_fn_hvc;
    }
    else if(!strcmp("smc", method))
    {
        invoke_psci_fn = __invoke_psci_fn_smc;
    }
    else
    {
        pr_warn("invalid \"method\" property: %s\n", method);
        return;
    }
}

static int aarch64_cpu_online(unsigned long cpuid, unsigned long entry_point)
{
    int err;

    err = invoke_psci_fn(PSCI_0_2_FN64_CPU_ON, cpuid, entry_point, 0);
    return err;
}

static int aarch64_irq_pending(uint8_t cpuid, unsigned int irq)
{
    u64       val     = 0;
    const u64 clid[4] = {
        [0] = 0x0000001,
        [1] = 0x0010001,
        [2] = 0x0020001,
        [3] = 0x0030001,
    };

    val = (clid[cpuid] | ((irq & 0xf) << 24));

    wmb();
    SET_GICV3_REG(ICC_SGI1R_EL1, val);
    isb();

    return 0;
}

static void aarch64_ipi_callback(int cpu, void *param)
{
    wake_up_interruptible(&secondary_wait_queue);
    secondary_flag = 1;
}

static int aarch64_irq_install(unsigned int irq)
{
    amp_ipi_callbackfunc_register(irq, NULL, aarch64_ipi_callback);
    return 0;
}

static int __init aarch64_interface_init(void)
{
    pr_info("aarch64 interface init.\n");

    psci_init();

    pr_info("aarch64 interface end.\n");

    return 0;
}

static internuclear_interface aarch64_interface_ops = {
    .secondary_wait_queue  = &secondary_wait_queue,
    .secondary_flag        = &secondary_flag,
    .secondary_init        = aarch64_interface_init,
    .secondary_irq_install = aarch64_irq_install,
    .secondary_cpu_online  = aarch64_cpu_online,
    .secondary_irq_pending = aarch64_irq_pending,
};

internuclear_interface_t internuclear_dev_get(void)
{
    return &aarch64_interface_ops;
}
