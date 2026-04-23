SRCS := $(APP_SRCS) $(CORE_SRCS) $(PERIPH_SRCS) $(STARTUP_SRCS) $(BLE_SRCS)
C_SRCS := $(filter %.c,$(SRCS))
ASM_SRCS := $(filter %.S,$(SRCS))
OBJS := $(addprefix $(OUT_DIR)/obj/,$(C_SRCS:.c=.o)) $(addprefix $(OUT_DIR)/obj/,$(ASM_SRCS:.S=.o))
DEPS := $(OBJS:.o=.d)
MAIN_OBJ := $(OUT_DIR)/obj/App/usb_mux_dev/main.o

$(OUT_DIR)/$(TARGET).elf: $(OBJS) | $(OUT_DIR)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

$(OUT_DIR)/$(TARGET).hex: $(OUT_DIR)/$(TARGET).elf | $(OUT_DIR)
	$(OBJCOPY) -O ihex $< $@

$(OUT_DIR)/$(TARGET).openocd.hex: $(OUT_DIR)/$(TARGET).elf | $(OUT_DIR)
	$(OBJCOPY) -O ihex --change-addresses 0x08000000 $< $@

$(OUT_DIR)/$(TARGET).lst: $(OUT_DIR)/$(TARGET).elf | $(OUT_DIR)
	$(OBJDUMP) --source --all-headers --demangle -M xw --line-numbers --wide $< > $@

$(OUT_DIR)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(MAIN_OBJ): $(APP_BUILD_INFO)

$(APP_BUILD_INFO): FORCE | $(OUT_DIR)
	@mkdir -p $(dir $@); \
	tmp="$@.tmp"; \
	git_desc="$$(git describe --always --dirty --tags 2>/dev/null || printf unknown)"; \
	build_date="$$(date '+%Y-%m-%d')"; \
	build_time="$$(date '+%H:%M:%S')"; \
	{ \
		printf '#ifndef APP_BUILD_INFO_H\n'; \
		printf '#define APP_BUILD_INFO_H\n\n'; \
		printf '#define APP_BUILD_GIT_DESC "%s"\n' "$$git_desc"; \
		printf '#define APP_BUILD_DATE "%s"\n' "$$build_date"; \
		printf '#define APP_BUILD_TIME "%s"\n\n' "$$build_time"; \
		printf '#endif\n'; \
	} > "$$tmp"; \
	if [ ! -f "$@" ] || ! cmp -s "$$tmp" "$@"; then mv "$$tmp" "$@"; else rm "$$tmp"; fi

$(OUT_DIR)/obj/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -MMD -MP -c $< -o $@

$(OUT_DIR):
	@mkdir -p $@

compile_commands.json: Scripts/rules.mk Scripts/sources.mk Scripts/toolchain.mk Makefile
	@{ \
		json_escape() { \
			printf '%s' "$$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; \
		}; \
		first=1; \
		printf '[\n' > $@; \
		for src in $(C_SRCS); do \
			out="$(OUT_DIR)/obj/$${src%.c}.o"; \
			cmd='$(CC) $(CFLAGS) -MMD -MP -c '"$$src"' -o '"$$out"; \
			[ $$first -eq 1 ] || printf ',\n' >> $@; \
			first=0; \
			printf '  {"directory":"%s","file":"%s","output":"%s","command":"%s"}' \
				"$$(json_escape "$(CURDIR)")" \
				"$$(json_escape "$$src")" \
				"$$(json_escape "$$out")" \
				"$$(json_escape "$$cmd")" >> $@; \
		done; \
		for src in $(ASM_SRCS); do \
			out="$(OUT_DIR)/obj/$${src%.S}.o"; \
			cmd='$(AS) $(ASFLAGS) -MMD -MP -c '"$$src"' -o '"$$out"; \
			[ $$first -eq 1 ] || printf ',\n' >> $@; \
			first=0; \
			printf '  {"directory":"%s","file":"%s","output":"%s","command":"%s"}' \
				"$$(json_escape "$(CURDIR)")" \
				"$$(json_escape "$$src")" \
				"$$(json_escape "$$out")" \
				"$$(json_escape "$$cmd")" >> $@; \
		done; \
		printf '\n]\n' >> $@; \
	}

FORCE:

-include $(DEPS)
