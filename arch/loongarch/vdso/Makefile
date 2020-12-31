# SPDX-License-Identifier: GPL-2.0
# Objects to go into the VDSO.

# Absolute relocation type $(ARCH_REL_TYPE_ABS) needs to be defined before
# the inclusion of generic Makefile.
ARCH_REL_TYPE_ABS := R_LARCH_32|R_LARCH_64|R_LARCH_JUMP_SLOT
include $(srctree)/lib/vdso/Makefile

obj-vdso-y := elf.o vgettimeofday.o sigreturn.o

# Common compiler flags between ABIs.
ccflags-vdso := \
	$(filter -I%,$(KBUILD_CFLAGS)) \
	$(filter -E%,$(KBUILD_CFLAGS)) \
	$(filter -march=%,$(KBUILD_CFLAGS)) \
	$(filter -m%-float,$(KBUILD_CFLAGS)) \
	-D__VDSO__

ifeq ($(cc-name),clang)
ccflags-vdso += $(filter --target=%,$(KBUILD_CFLAGS))
endif

cflags-vdso := $(ccflags-vdso) \
	$(filter -W%,$(filter-out -Wa$(comma)%,$(KBUILD_CFLAGS))) \
	-O2 -g -fno-strict-aliasing -fno-common -fno-builtin -G 0 \
	-fno-stack-protector -fno-jump-tables -DDISABLE_BRANCH_PROFILING \
	$(call cc-option, -fno-asynchronous-unwind-tables) \
	$(call cc-option, -fno-stack-protector)
aflags-vdso := $(ccflags-vdso) \
	-D__ASSEMBLY__ -Wa,-gdwarf-2

ifneq ($(c-gettimeofday-y),)
  CFLAGS_vgettimeofday.o += -include $(c-gettimeofday-y)
endif

# VDSO linker flags.
ldflags-y := -Bsymbolic --no-undefined -soname=linux-vdso.so.1 \
	$(filter -E%,$(KBUILD_CFLAGS)) -nostdlib -shared \
	--hash-style=sysv --build-id -T

GCOV_PROFILE := n

#
# Shared build commands.
#

quiet_cmd_vdsold_and_vdso_check = LD      $@
      cmd_vdsold_and_vdso_check = $(cmd_ld); $(cmd_vdso_check)

quiet_cmd_vdsoas_o_S = AS       $@
      cmd_vdsoas_o_S = $(CC) $(a_flags) -c -o $@ $<

# Strip rule for the raw .so files
$(obj)/%.so.raw: OBJCOPYFLAGS := -S
$(obj)/%.so.raw: $(obj)/%.so.dbg.raw FORCE
	$(call if_changed,objcopy)

hostprogs := genvdso

quiet_cmd_genvdso = GENVDSO $@
define cmd_genvdso
	$(foreach file,$(filter %.raw,$^),cp $(file) $(file:%.raw=%) &&) \
	$(obj)/genvdso $(<:%.raw=%) $(<:%.dbg.raw=%) $@ $(VDSO_NAME)
endef

#
# Build native VDSO.
#

native-abi := $(filter -mabi=%,$(KBUILD_CFLAGS))

targets += $(obj-vdso-y)
targets += vdso.lds
targets += vdso.so.dbg.raw vdso.so.raw
targets += vdso.so.dbg vdso.so
targets += vdso-image.c

obj-vdso := $(obj-vdso-y:%.o=$(obj)/%.o)

$(obj-vdso): KBUILD_CFLAGS := $(cflags-vdso) $(native-abi)
$(obj-vdso): KBUILD_AFLAGS := $(aflags-vdso) $(native-abi)

$(obj)/vdso.lds: KBUILD_CPPFLAGS := $(ccflags-vdso) $(native-abi)

$(obj)/vdso.so.dbg.raw: $(obj)/vdso.lds $(obj-vdso) FORCE
	$(call if_changed,vdsold_and_vdso_check)

$(obj)/vdso-image.c: $(obj)/vdso.so.dbg.raw $(obj)/vdso.so.raw \
                     $(obj)/genvdso FORCE
	$(call if_changed,genvdso)

obj-y += vdso-image.o



# FIXME: Need install rule for debug.
# Needs to deal with dependency for generation of dbg by cmd_genvdso...
