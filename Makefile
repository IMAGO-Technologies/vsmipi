# Comment/uncomment the following line to disable/enable debugging
# or call make DEBUG=y default is DEBUG=n
#DEBUG = y

# get directory of this module
M ?= $(shell pwd)

# include dkms.conf for PACKAGE_VERSION
include $(M)/dkms.conf

ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DDEBUG # "-O" is needed to expand inlines
  $(info we use debug flags [${DEBFLAGS}])
else
  DEBFLAGS = -O2  
endif

ccflags-y := $(DEBFLAGS) -DMODVERSION=\"$(PACKAGE_VERSION)\" -Werror -Wall -Wno-unused-parameter -Wno-date-time 
vsmipi-objs := vspv3_sensor.o
obj-m	:= vsmipi.o mx6s_capture.o

# If KERNELDIR is defined, we've been invoked from the DKMS
# otherwise we were called directly from the command line
# '?=' has only an effect if the variable not defined
KERNELDIR ?= /lib/modules/$(shell uname -r)/build


default:
	$(MAKE) -C $(KERNELDIR) M=$(M) modules 

# create a file like 'vsvpfedrv4.9.59-rt23-visioncam-xm-1.0.0.0_armv7l.ko'
deploy:
	make clean
	make
	strip --strip-debug vsmipi.ko
	mv vsmipi.ko vsmipi_$(shell uname -r)_$(shell uname -m).ko

devel:
	make 
	make install
	rmmod vsmipi
	modprobe vsmipi

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions modules.order Module.symvers

#default /lib/modules/$(KERNELRELEASE)/extra
install:
	make -C $(KERNELDIR) M=$(M) modules_install
	depmod -a 

