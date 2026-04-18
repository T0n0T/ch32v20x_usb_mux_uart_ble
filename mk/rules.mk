SRCS := $(APP_SRCS) $(CORE_SRCS) $(PERIPH_SRCS) $(STARTUP_SRCS) $(BLE_SRCS)
C_SRCS := $(filter %.c,$(SRCS))
ASM_SRCS := $(filter %.S,$(SRCS))
OBJS := $(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o)) $(addprefix $(BUILD_DIR)/,$(ASM_SRCS:.S=.o))
DEPS := $(OBJS:.o=.d)

$(OUT_DIR)/$(TARGET).elf: $(OBJS) | $(OUT_DIR)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

$(OUT_DIR)/$(TARGET).hex: $(OUT_DIR)/$(TARGET).elf | $(OUT_DIR)
	$(OBJCOPY) -O ihex $< $@

$(OUT_DIR)/$(TARGET).lst: $(OUT_DIR)/$(TARGET).elf | $(OUT_DIR)
	$(OBJDUMP) --source --all-headers --demangle -M xw --line-numbers --wide $< > $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -MMD -MP -c $< -o $@

$(OUT_DIR):
	@mkdir -p $@

-include $(DEPS)
