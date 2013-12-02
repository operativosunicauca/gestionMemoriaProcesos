#Archivo de configuracion de la utilidad make.
#Author: Erwin Meza <emezav@gmail.com>
#/** @verbatim */

KERNEL_OBJS = $(patsubst %.S,%.o,$(wildcard src/*.S)) \
		$(patsubst %.c,%.o,$(wildcard src/*.c))

GCC := $(shell util/check_program.sh i386-elf-gcc gcc)
LD := $(shell util/check_program.sh i386-elf-ld ld)
JAVA=java

os := $(shell uname -o)

BOCHSDBG := $(shell util/check_program.sh bochsdbg bochs)

BOCHSDISPLAY := x
ifeq "$(os)" "Msys"
	BOCHSDISPLAY := win32
endif

ifeq "$(os)" "Cygwin"
	BOCHSDISPLAY := win32
endif

all: kernel
	-if test -f disk_template.gz; then \
	   gunzip disk_template.gz; \
	   else true; fi
	#Copiar el kernel al directorio filesys/kernel
	cp -f kernel filesys/boot/kernel
	#Copiar la plantilla a la imagen de disco
	cp -f disk_template disk_image
	#Extraer la particion inicial
	dd if=disk_template of=first_partition bs=512 skip=63
	#Copiar el kernel al sistema de archivos
	e2fsimage -f first_partition -d filesys -n
	#Copiar la particion actualizada a la imagen de disco
	dd if=first_partition of=disk_image bs=512 seek=63
	#Borrar la particion actualizada
	rm -f first_partition	
	
kernel: $(KERNEL_OBJS)
	$(LD) -T link.ld -o kernel $(KERNEL_OBJS)

.S.o:
	$(GCC) -nostdinc -nostdlib -fno-builtin -c -Iinclude -o $@ $<
	
.c.o:
	$(GCC) -nostdinc -nostdlib -fno-builtin -c -Iinclude  -o $@ $<

bochs: all
	-bochs -q 'boot:disk' \
	'ata0-master: type=disk, path="disk_image", cylinders=10, heads=16, spt=63'\
	'megs:32'
	
bochsdbg: all
	-$(BOCHSDBG) -q 'boot:disk' \
	'ata0-master: type=disk, path="disk_image", cylinders=10, heads=16, spt=63'\
	'megs:32' 'display_library:$(BOCHSDISPLAY), options="gui_debug"'
	
qemu: all
	qemu -hda disk_image -boot c -m 64

jpc: all
	$(JAVA) -jar ../jpc/JPCApplication.jar -boot hda -hda disk_image

jpcdbg: all
	$(JAVA) -jar ../jpc/JPCDebugger.jar -boot hda -hda disk_image

clean:
	rm -f kernel $(KERNEL_OBJS) disk_image filesys/boot/kernel
	-if test -f disk_template; then \
	   gzip disk_template; \
	   else true; fi
	-if test -d docs; then \
	   rm -r -f docs; \
	   else true; fi

#/** @endverbatim */