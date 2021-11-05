# SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
OUTPUT := .output
CLANG ?= clang -v
BPFTOOL ?= $(abspath tools/bpftool)
PAHOLE ?= $(abspath tools/pahole)
READELF ?= readelf
VMLINUX ?= /usr/lib/debug/lib/modules/`uname -r`/vmlinux
VMLINUX_HEADER ?= $(OUTPUT)/vmlinux.h

BTF_PAHOLE_PROBE := $(shell $(READELF) -S $(VMLINUX) | grep .BTF 2>&1)
INCLUDES := -I$(OUTPUT)
CFLAGS := -g -Wall
# ARM版本也需要验证下
ARCH := $(shell uname -m | sed 's/x86_64/x86/')

ifeq ($(BTF_PAHOLE_PROBE),)
	DWARF2BTF =	y
endif

APPS = readahead_tune

CLANG_BPF_SYS_INCLUDES = $(shell $(CLANG) -v -E - </dev/null 2>&1 \
	| sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')

ifeq ($(V),1)
	Q =
	msg =
else
	Q = @
	msg = @printf '  %-8s %s%s\n'					\
			  "$(1)"						\
			  "$(patsubst $(abspath $(OUTPUT))/%,%,$(2))"	\
			  "$(if $(3), $(3))";
	MAKEFLAGS += --no-print-directory
endif

.PHONY: all
all: $(APPS)

debug: DEBUG_FLAGS = -DBPFDEBUG
debug: all

.PHONY: clean
clean:
	$(call msg,CLEAN)
	$(Q)rm -rf $(OUTPUT) $(APPS)

$(OUTPUT):
	$(call msg,MKDIR,$@)
	$(Q)mkdir -p $@

$(VMLINUX_HEADER):
	$(call msg,GEN-VMLINUX_H,$@)
ifeq ($(DWARF2BTF),y)
	$(Q)$(PAHOLE) -J $(VMLINUX)
endif
	$(Q)$(BPFTOOL) btf dump file $(VMLINUX) format c > $@

# Build BPF code
$(OUTPUT)/%.bpf.o: %.bpf.c $(wildcard %.h) | $(OUTPUT) $(VMLINUX_HEADER)
	$(call msg,BPF,$@)
	$(Q)$(CLANG) -D__KERNEL__ -D__ASM_SYSREG_H -D__TARGET_ARCH_$(ARCH)	\
	$(DEBUG_FLAGS) \
	$(INCLUDES) $(CLANG_BPF_SYS_INCLUDES)	\
	-g -O2 -target bpf -c $(filter %.c,$^) -o $@

# Generate BPF skeletons
$(OUTPUT)/%.skel.h: $(OUTPUT)/%.bpf.o | $(OUTPUT)
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

# Build user-space code
$(patsubst %,$(OUTPUT)/%.o,$(APPS)): %.o: %.skel.h

$(OUTPUT)/%.o: %.c $(wildcard %.h) | $(OUTPUT)
	$(call msg,CC,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -c $(filter %.c,$^) -o $@

# Build application binary
# 动态链接的场景下尝试将-lelf和-lz去掉
$(APPS): %: $(OUTPUT)/%.o | $(OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $^ -lbpf -lelf -lz -o $@

# delete failed targets
.DELETE_ON_ERROR:

# keep intermediate (.skel.h, .bpf.o, etc) targets
.SECONDARY:
