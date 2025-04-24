# 目标模块名
MODULE_NAME := internuclear
internuclear-objs = ./internuclear/internuclear.o
obj-m := internuclear.o

ifeq ($(CONFIG_USE_AARCH64), y)
internuclear-objs += ./operation_interface/aarch64/aarch64_interface.o
endif

ifeq ($(CONFIG_USE_LONGARCH), y)
internuclear-objs += ./operation_interface/loongarch/loongarch_interface.o
endif

# 内核源码路径，请修改为你的实际路径
KERNEL_SOURCE ?= /kernel
# 当前路径
PWD := $(shell pwd)

# 默认目标：编译内核模块
all:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules

# 清理生成的文件
clean:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) clean
