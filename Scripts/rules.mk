SRCS := $(APP_SRCS) $(CORE_SRCS) $(PERIPH_SRCS) $(STARTUP_SRCS) $(BLE_SRCS)
C_SRCS := $(filter %.c,$(SRCS))
ASM_SRCS := $(filter %.S,$(SRCS))
OBJS := $(addprefix $(OUT_DIR)/obj/,$(C_SRCS:.c=.o)) $(addprefix $(OUT_DIR)/obj/,$(ASM_SRCS:.S=.o))
DEPS := $(OBJS:.o=.d)

$(OUT_DIR)/$(TARGET).elf: $(OBJS) | $(OUT_DIR)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

$(OUT_DIR)/$(TARGET).hex: $(OUT_DIR)/$(TARGET).elf | $(OUT_DIR)
	$(OBJCOPY) -O ihex $< $@

$(OUT_DIR)/$(TARGET).lst: $(OUT_DIR)/$(TARGET).elf | $(OUT_DIR)
	$(OBJDUMP) --source --all-headers --demangle -M xw --line-numbers --wide $< > $@

$(OUT_DIR)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUT_DIR)/obj/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -MMD -MP -c $< -o $@

$(OUT_DIR):
	@mkdir -p $@

-include $(DEPS)
