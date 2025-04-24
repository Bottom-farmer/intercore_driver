# intercore_driver 文件夹说明

## 项目简介
`intercore_driver` 文件夹包含核间通信模型的实现代码和相关资源，是多核通信功能的核心模块之一。该模块旨在提供高效、可靠的核间通信机制，支持多种硬件架构。

## 功能
- 实现核间通信的核心逻辑。
- 提供通信协议的抽象层，便于扩展和维护。
- 支持多种通信模式（如IPI、邮箱等）。

## 目录结构
```
intercore_driver/
├── internuclear/			# 核间通信抽象层源代码
├── operation_interface/	# 平台对接
	├── aarch64/			# aarch64平台对接层源代码
	├── loongarch/			# loongarch平台对接层源代码
├── product/				# 模块生成目录
```

## 使用说明
1. **构建项目**：
   运行构建脚本（目前只支持**aarch64**和**loongarch**平台，其余平台需自行对接）：

   ```bash
   ./build.sh [aarch64|loongarch]
   ```

## 注意事项
- 编译需根据情况自行修改脚本参数

```
KERNEL_SOURCE_AARCH64="/home/bigdog/share/RK3568/rk356x_linux_release_v1.3.1_20221120/kernel"
KERNEL_SOURCE_LOONGARCH="/home/bigdog/share/Loongar/uisrc-lab-loongarch/sources/kernel"
CROSS_COMPILE=/home/bigdog/share/tools/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
```

- 使用IPI进行核间通信时，**linux**内核需提供中断注册函数，如果内核未提供注册函数可自行添加，代码如下：

```c

#ifdef CONFIG_LS_AMP_IPI_CALLBACH
#define IPI_IRQ_START 4
#define IPI_IRQ_END   8

struct ls_ipi_handler {
	void (*handler)(int, void *);
	void *param;
};
static struct ls_ipi_handler ls_ipi_handler[8] = {0};

/* Interrupt function registers callback function */
void amp_ipi_callbackfunc_register(uint8_t ipi_id, void *param, void (*handler)(int, void *))
{
	if((handler == NULL) || !(IPI_IRQ_START < ipi_id && ipi_id < IPI_IRQ_END))
	{
		pr_err("error! amp ipi call back func is empty!\n");
		return;
	}

	ls_ipi_handler[ipi_id].handler = handler;

	if(param != NULL)
	{
		ls_ipi_handler[ipi_id].param = param;
	}
}
EXPORT_SYMBOL(amp_ipi_callbackfunc_register);

void amp_ipi_call_back(int cpu, int ipinr)
{
	unsigned int reg_offset = 0;
	unsigned char i = 0;

	for(i = 0; i < 8; i++)
	{
		reg_offset = ipinr >> (i * 4);

		if(reg_offset & 0x01)
		{
			if(!(IPI_IRQ_START < i && i < IPI_IRQ_END))
			{
				return;
			}

			if(ls_ipi_handler[i].handler != NULL)
			{
				ls_ipi_handler[i].handler(cpu, ls_ipi_handler[i].param);
			}
		}

		if(!reg_offset)
		{
			break;
		}
	}
}
#endif
```

