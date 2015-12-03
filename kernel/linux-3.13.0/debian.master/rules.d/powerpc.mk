human_arch	= PowerPC (32 bit userspace)
build_arch	= powerpc
header_arch	= $(build_arch)
defconfig	= pmac32_defconfig
flavours	= powerpc-smp powerpc64-smp powerpc-e500 powerpc-e500mc powerpc64-emb
build_image	= zImage
kernel_file	= $(shell if [ ! -f $(builddir)/build-$*/vmlinux.strip ] && \
		    [ -f $(builddir)/build-$*/vmlinux.strip.gz ]; then \
			gunzip -c $(builddir)/build-$*/vmlinux.strip.gz \
			> $(builddir)/build-$*/vmlinux.strip; \
		    fi && echo vmlinux.strip)
install_file	= vmlinux

# These flavours differ
build_image_powerpc-e500mc	= uImage
kernel_file_powerpc-e500mc	= arch/powerpc/boot/uImage

build_image_powerpc-e500	= uImage
kernel_file_powerpc-e500	= arch/powerpc/boot/uImage

loader		= yaboot

custom_flavours	=

no_dumpfile		= true
do_tools_cpupower	= true
do_tools_perf		= true

family			= ubuntu
