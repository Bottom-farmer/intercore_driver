#include "internuclear.h"

#define INTER_DEVICE_NAME          "hw_internuclear"

#define HW_SECONDARY_MAGIC         'D'
#define HW_SECONDARY_CPU_ONLINE    _IOW(HW_SECONDARY_MAGIC, 0, int)
#define HW_SECONDARY_CPU_DOWNLINE  _IOW(HW_SECONDARY_MAGIC, 1, int)
#define HW_SECONDARY_LOAD_FIRMWARE _IOW(HW_SECONDARY_MAGIC, 2, int)
#define HW_SECONDARY_SIGNAL_SEND   _IOW(HW_SECONDARY_MAGIC, 3, int)
#define HW_SECONDARY_IRQ_INSTALL   _IOW(HW_SECONDARY_MAGIC, 4, int)

typedef struct secondary_config {
    int        cpu_id;
    long long  entry;
    int        irq_id;
    const char file_path[1024];
} secondary_config;
typedef secondary_config *secondary_config_t;

static struct cdev              iner_dev;
static dev_t                    inter_dev_num      = 0;
static internuclear_interface_t internuclear_dev   = NULL;
static struct class            *internuclear_class = NULL;

static int load_firmware(const char *fw_path, long long local_address, int pa_size)
{
    struct file  *fw_file = NULL;
    mm_segment_t  old_fs;
    loff_t        pos = 0;
    size_t        fw_size;
    char         *fw_data = NULL;
    void __iomem *mem_vaddr;
    int           ret = 0;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    fw_file = filp_open(fw_path, O_RDONLY, 0);
    if(IS_ERR(fw_file))
    {
        pr_err("Failed to open firmware file %s.\n", fw_path);
        ret = PTR_ERR(fw_file);
        goto out_unmap;
    }

    fw_size = i_size_read(fw_file->f_path.dentry->d_inode);
    if(fw_size > pa_size)
    {
        pr_err("Firmware size is larger than the reserved memory region!\n");
        ret = -EINVAL;
        goto out_close;
    }

    fw_data = kmalloc(PAGE_ALIGN(fw_size), GFP_KERNEL);
    if(!fw_data)
    {
        pr_err("Failed to allocate memory for firmware data!\n");
        ret = -ENOMEM;
        goto out_close;
    }

    ret = kernel_read(fw_file, fw_data, fw_size, &pos);
    if(ret < 0)
    {
        pr_err("Failed to read firmware file\n");
        goto out_free;
    }

    mem_vaddr = ioremap_nocache(local_address, PAGE_ALIGN(fw_size));
    if(!mem_vaddr)
    {
        pr_err("Failed to ioremap memory region.\n");
        return -ENOMEM;
    }

    memcpy_toio(mem_vaddr, fw_data, PAGE_ALIGN(fw_size));
    iounmap(mem_vaddr);

    pr_info("Firmware %s loaded into memory at 0x%llx\n", fw_path, local_address);

out_free:
    kfree(fw_data);
out_close:
    filp_close(fw_file, NULL);
out_unmap:
    set_fs(old_fs);
    return ret;
}

static long hw_internuclear_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    int               ret           = -EINVAL;
    secondary_config *secondary_cpu = NULL;

    secondary_cpu = kmalloc(sizeof(secondary_config), GFP_KERNEL);
    if(secondary_cpu == NULL)
    {
        pr_err("Failed to allocate memory for secondary CPU config\n");
        return -ENOMEM;
    }
    memset(secondary_cpu, 0, sizeof(secondary_config));

    if(internuclear_dev == NULL)
    {
        ret = -EPERM;
        goto _out_free;
    }

    ret = copy_from_user(secondary_cpu, (secondary_config_t __user *)arg, sizeof(secondary_config));
    if(ret != 0)
    {
        goto _out_free;
    }

    if((cmd == HW_SECONDARY_SIGNAL_SEND) && (internuclear_dev->secondary_irq_pending != NULL))
    {
        ret = internuclear_dev->secondary_irq_pending(secondary_cpu->cpu_id, secondary_cpu->irq_id);
        if(ret)
        {
            pr_err("Failed to send signal to CPU %d (error: %d)\n", secondary_cpu->cpu_id, ret);
        }
    }
    else if((cmd == HW_SECONDARY_IRQ_INSTALL) && (internuclear_dev->secondary_irq_install != NULL))
    {
        ret = internuclear_dev->secondary_irq_install(secondary_cpu->irq_id);
        if(ret)
        {
            pr_err("Failed to install IRQ %d (error: %d)\n", secondary_cpu->irq_id, ret);
        }
    }
    else if((cmd == HW_SECONDARY_CPU_ONLINE) && (internuclear_dev->secondary_cpu_online != NULL))
    {
        ret = internuclear_dev->secondary_cpu_online(secondary_cpu->cpu_id, secondary_cpu->entry);
        if(ret)
        {
            pr_err("Failed to start CPU %d (error: %d)\n", secondary_cpu->cpu_id, ret);
        }
    }
    else if(cmd == HW_SECONDARY_LOAD_FIRMWARE)
    {
        ret = load_firmware(secondary_cpu->file_path, secondary_cpu->entry, SHARED_MEMORY_SIZE);
        if(ret < 0)
        {
            pr_err("Failed to load firmware (error: %d)\n", ret);
        }
    }
    else if(cmd == HW_SECONDARY_CPU_DOWNLINE)
    {
        ret = cpu_down(secondary_cpu->cpu_id);
        if(ret < 0)
        {
            pr_err("Failed to offline CPU%d: error %d\n", secondary_cpu->cpu_id, ret);
        }
    }
    else
    {
        pr_err("Unknown command: %u\n", cmd);
        ret = -EINVAL;
    }

_out_free:
    kfree(secondary_cpu);
    return ret;
}

static unsigned int hw_internuclear_poll(struct file *filp, poll_table *wait)
{
    if(internuclear_dev->secondary_wait_queue == NULL || internuclear_dev->secondary_flag == NULL)
    {
        return -EINVAL;
    }

    poll_wait(filp, internuclear_dev->secondary_wait_queue, wait);

    if(*internuclear_dev->secondary_flag)
    {
        *internuclear_dev->secondary_flag = 0;
        return POLLIN | POLLRDNORM;
    }

    return 0;
}

static int hw_internuclear_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long pfn  = vma->vm_pgoff;
    unsigned long size = vma->vm_end - vma->vm_start;

    vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

    if(remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
    {
        return -EAGAIN;
    }
    return 0;
}

__weak internuclear_interface_t internuclear_dev_get(void)
{
    pr_warn("Default weak implementation of internuclear_dev_get called.\n");
    return NULL;
}

static struct file_operations hw_internuclear_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = hw_internuclear_ioctl,
    .poll           = hw_internuclear_poll,
    .mmap           = hw_internuclear_mmap,
};

static int __init hw_internuclear_init(void)
{
    if(alloc_chrdev_region(&inter_dev_num, 0, 1, INTER_DEVICE_NAME) < 0)
    {
        return -1;
    }

    cdev_init(&iner_dev, &hw_internuclear_fops);
    if(cdev_add(&iner_dev, inter_dev_num, 1) < 0)
    {
        unregister_chrdev_region(inter_dev_num, 1);
        return -1;
    }

    internuclear_class = class_create(THIS_MODULE, INTER_DEVICE_NAME);
    if(IS_ERR(internuclear_class))
    {
        pr_err("Failed to create device class\n");
        cdev_del(&iner_dev);
        unregister_chrdev_region(inter_dev_num, 1);
        return PTR_ERR(internuclear_class);
    }

    device_create(internuclear_class, NULL, inter_dev_num, NULL, INTER_DEVICE_NAME);
    pr_info("%s module loaded. Write CPU ID and entry point to /dev/%s\n", INTER_DEVICE_NAME, INTER_DEVICE_NAME);

    internuclear_dev = internuclear_dev_get();
    if(internuclear_dev == NULL)
    {
        return -EINVAL;
    }

    if(internuclear_dev->secondary_init != NULL)
    {
        internuclear_dev->secondary_init();
    }

    return 0;
}

static void __exit hw_internuclear_exit(void)
{
    device_destroy(internuclear_class, inter_dev_num);
    class_destroy(internuclear_class);
    cdev_del(&iner_dev);
    unregister_chrdev_region(inter_dev_num, 1);
    pr_info("%s module unloaded.\n", INTER_DEVICE_NAME);
}

module_init(hw_internuclear_init);
module_exit(hw_internuclear_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("bigdog");
MODULE_DESCRIPTION("Internuclear Module");
