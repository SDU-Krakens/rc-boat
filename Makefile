BUILD_TYPE ?= Debug
ZERO_HOST ?= root@10.10.4.2
ZERO_DIR ?= /home/kraken/main

BUILD_DIR := dist
CONFIG_MK := $(BUILD_DIR)/config/config.mk
PICO_ELF := $(BUILD_DIR)/pico/pico.elf
RPI_BIN := $(BUILD_DIR)/rpi/rc-boat
SWD_SPEED_KHZ := 1000
BRANCH = $(shell git rev-parse --abbrev-ref HEAD)
NPROC := $(shell nproc)

# First ssh asks for the password and becomes the master, later ssh/scp reuse
# it. Closed at the end of each remote recipe, lingers 10 s idle on failure.
SSH_OPTS := -o ControlMaster=auto -o ControlPath=$(BUILD_DIR)/.ssh-%C \
	-o ControlPersist=10
SSH := ssh $(SSH_OPTS)
SCP := scp $(SSH_OPTS)

-include $(CONFIG_MK)

.PHONY: build config pullbuild push flash flash-remote deploy deploy-remote \
	check-config check-remote

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build $(BUILD_DIR) -j$(NPROC)

config:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	ccmake $(BUILD_DIR)

pullbuild:
	git pull origin $(BRANCH)
	$(MAKE) build

push:
	git add .
	git commit -m "fast push"
	git push origin $(BRANCH)

check-config:
	@test -f $(CONFIG_MK) || { \
		echo "$(CONFIG_MK) missing, run 'make build' or 'make config' first"; \
		exit 1; }

check-remote:
	@test -n "$(ZERO_HOST)" || { echo "ZERO_HOST is not set"; exit 1; }
	@test -n "$(ZERO_DIR)" || { echo "ZERO_DIR is not set"; exit 1; }

# Flash the Pico over SWD from the Zero's GPIOs (run on the Zero)
flash: check-config
	openocd -f interface/raspberrypi-native.cfg \
		-c "adapter gpio swdio $(CONF_SWD_SWDIO_PIN)" \
		-c "adapter gpio swclk $(CONF_SWD_SWCLK_PIN)" \
		-c "transport select swd" \
		-c "adapter speed $(SWD_SPEED_KHZ)" \
		-f target/rp2040.cfg \
		-c "program $(PICO_ELF) verify reset exit"

flash-remote: check-remote build
	$(SSH) $(ZERO_HOST) "mkdir -p $(ZERO_DIR)/$(BUILD_DIR)/pico $(ZERO_DIR)/$(BUILD_DIR)/config"
	$(SCP) $(PICO_ELF) $(ZERO_HOST):$(ZERO_DIR)/$(BUILD_DIR)/pico/
	$(SCP) $(CONFIG_MK) $(ZERO_HOST):$(ZERO_DIR)/$(BUILD_DIR)/config/
	$(SSH) $(ZERO_HOST) "cd $(ZERO_DIR) && make flash"
	$(SSH) -O exit $(ZERO_HOST)

# Run on the Zero
deploy:
	git fetch --all
	git reset --hard origin/main
	$(MAKE) build
	$(MAKE) flash
	systemctl restart boat

# Run on the laptop
deploy-remote: check-remote build
	$(SSH) $(ZERO_HOST) "mkdir -p $(ZERO_DIR)/$(BUILD_DIR)/pico $(ZERO_DIR)/$(BUILD_DIR)/rpi $(ZERO_DIR)/$(BUILD_DIR)/config"
	$(SCP) $(RPI_BIN) $(ZERO_HOST):$(ZERO_DIR)/$(BUILD_DIR)/rpi/
	$(SCP) $(PICO_ELF) $(ZERO_HOST):$(ZERO_DIR)/$(BUILD_DIR)/pico/
	$(SCP) $(CONFIG_MK) $(ZERO_HOST):$(ZERO_DIR)/$(BUILD_DIR)/config/
	$(SSH) $(ZERO_HOST) "cd $(ZERO_DIR) && make flash && systemctl restart boat"
	$(SSH) -O exit $(ZERO_HOST)
