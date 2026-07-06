DEVICE ?= $(shell cat esphome/.device_ip 2>/dev/null)
ESPHOME_VERSION := $(strip $(shell cat .esphome-version))
ESPHOME_PYTHON_VERSION := $(strip $(shell cat .esphome-python-version))
ESPHOME := uv run --python $(ESPHOME_PYTHON_VERSION) --with esphome==$(ESPHOME_VERSION) --with "setuptools<81" python -m esphome

.PHONY: help esphome-version esphome-compile esphome-flash clean gen-api-key

help:
	@echo "esphome-compile  validate firmware config and compile locally"
	@echo "esphome-flash    upload via USB or OTA (DEVICE=<serial-or-ip>)"
	@echo "esphome-version  show pinned ESPHome version for HA compatibility"
	@echo "esphome-python-version show pinned Python version for old ESPHome"
	@echo "gen-api-key      generate a random ESPHome API encryption key"
	@echo "clean            remove ESPHome build artifacts"

gen-api-key:
	@python3 -c "import base64, os; print(base64.b64encode(os.urandom(32)).decode())"

esphome-version:
	@echo $(ESPHOME_VERSION)

esphome-python-version:
	@echo $(ESPHOME_PYTHON_VERSION)

esphome-compile:
	cd esphome && $(ESPHOME) compile esp32-bluetooth-foot-pedal.yaml

esphome-flash:
	cd esphome && $(ESPHOME) upload esp32-bluetooth-foot-pedal.yaml $(if $(DEVICE),--device $(DEVICE),)

clean:
	rm -rf esphome/.esphome esphome/build build
