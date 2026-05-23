RATCOM_ENV ?= ratcom_915
RNODE_DIR ?= vendor/rnode_firmware
LAUNCHER_DIR ?= launcher
BUILD_DIR ?= build
DIST_DIR ?= dist

PARTITION_CSV := partitions/rs_cardputer_adv_8mb_dual.csv
PARTITIONS_BIN := $(BUILD_DIR)/rs_cardputer_adv_partitions.bin

FULL_NAME := ratcom-cardputer-adv-full
RATCOM_ONLY_NAME := ratcom-cardputer-adv-ratcom-only
RNODE_ONLY_NAME := ratcom-cardputer-adv-rnode-only

FULL_BIN := $(BUILD_DIR)/$(FULL_NAME).bin
RATCOM_ONLY_BIN := $(BUILD_DIR)/$(RATCOM_ONLY_NAME).bin
RNODE_ONLY_BIN := $(BUILD_DIR)/$(RNODE_ONLY_NAME).bin

LAUNCHER_BIN := $(LAUNCHER_DIR)/.pio/build/cardputer_adv_launcher/firmware.bin
RATCOM_BIN := .pio/build/$(RATCOM_ENV)/firmware.bin
RATCOM_MERGED_BIN := ratcom-merged.bin

RNODE_OUTPUT := $(RNODE_DIR)/build/esp32.esp32.esp32s3
RNODE_BIN := $(RNODE_OUTPUT)/RNode_Firmware.ino.bin
RNODE_BOOTLOADER_BIN := $(RNODE_OUTPUT)/RNode_Firmware.ino.bootloader.bin
RNODE_PARTITIONS_BIN := $(RNODE_OUTPUT)/RNode_Firmware.ino.partitions.bin

BOOTLOADER_BIN := .pio/build/$(RATCOM_ENV)/bootloader.bin
PLATFORMIO_ARDUINO ?= $(HOME)/.platformio/packages/framework-arduinoespressif32
ARDUINO15_ESP32 ?= $(HOME)/Library/Arduino15/packages/esp32/hardware/esp32/2.0.17
GEN_ESPPART ?= $(if $(wildcard $(PLATFORMIO_ARDUINO)/tools/gen_esp32part.py),$(PLATFORMIO_ARDUINO)/tools/gen_esp32part.py,$(ARDUINO15_ESP32)/tools/gen_esp32part.py)
BOOT_APP0_BIN ?= $(if $(wildcard $(PLATFORMIO_ARDUINO)/tools/partitions/boot_app0.bin),$(PLATFORMIO_ARDUINO)/tools/partitions/boot_app0.bin,$(ARDUINO15_ESP32)/tools/partitions/boot_app0.bin)

PORT ?= $(port)
ifeq ($(PORT),)
PORT := /dev/ttyACM0
endif

.PHONY: all build-ratcom build-launcher build-rnode check bundle full-image ratcom-only-image rnode-only-image package release flash clean

all: bundle

build-ratcom:
	python3 -m platformio run -e $(RATCOM_ENV)

build-launcher:
	python3 -m platformio run -d $(LAUNCHER_DIR) -e cardputer_adv_launcher

build-rnode:
	$(MAKE) -C $(RNODE_DIR) firmware-cardputer_adv

$(PARTITIONS_BIN): $(PARTITION_CSV)
	mkdir -p $(BUILD_DIR)
	python3 $(GEN_ESPPART) $(PARTITION_CSV) $(PARTITIONS_BIN)

check: build-launcher build-ratcom build-rnode
	python3 tools/check_image_fit.py --launcher $(LAUNCHER_BIN) --ratcom $(RATCOM_BIN) --rnode $(RNODE_BIN)

full-image: check $(PARTITIONS_BIN)
	python3 tools/make_dual_image.py \
		--bootloader $(BOOTLOADER_BIN) \
		--partitions $(PARTITIONS_BIN) \
		--boot-app0 $(BOOT_APP0_BIN) \
		--launcher $(LAUNCHER_BIN) \
		--ratcom $(RATCOM_BIN) \
		--rnode $(RNODE_BIN) \
		--output $(FULL_BIN)

ratcom-only-image: build-ratcom
	mkdir -p $(BUILD_DIR)
	cp $(RATCOM_MERGED_BIN) $(RATCOM_ONLY_BIN)

rnode-only-image: build-rnode
	mkdir -p $(BUILD_DIR)
	python3 -m esptool --chip esp32s3 merge-bin \
		--flash-mode dio --flash-size 8MB \
		--output $(RNODE_ONLY_BIN) \
		0x0000 $(RNODE_BOOTLOADER_BIN) \
		0x8000 $(RNODE_PARTITIONS_BIN) \
		0xe000 $(BOOT_APP0_BIN) \
		0x10000 $(RNODE_BIN)

bundle: full-image

package: full-image ratcom-only-image rnode-only-image
	mkdir -p $(DIST_DIR)
	cp $(FULL_BIN) $(DIST_DIR)/$(FULL_NAME).bin
	cp $(RATCOM_ONLY_BIN) $(DIST_DIR)/$(RATCOM_ONLY_NAME).bin
	cp $(RNODE_ONLY_BIN) $(DIST_DIR)/$(RNODE_ONLY_NAME).bin
	python3 tools/package_merged_zip.py --image $(FULL_BIN) --name $(FULL_NAME) --output $(DIST_DIR)/$(FULL_NAME).zip
	python3 tools/package_merged_zip.py --image $(RATCOM_ONLY_BIN) --name $(RATCOM_ONLY_NAME) --output $(DIST_DIR)/$(RATCOM_ONLY_NAME).zip
	python3 tools/package_merged_zip.py --image $(RNODE_ONLY_BIN) --name $(RNODE_ONLY_NAME) --output $(DIST_DIR)/$(RNODE_ONLY_NAME).zip

release: package

flash: bundle
	python3 -m esptool --chip esp32s3 --port $(PORT) --baud 460800 --before default_reset --after hard_reset write-flash 0x0 $(FULL_BIN)

clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)
