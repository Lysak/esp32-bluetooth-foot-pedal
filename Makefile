DEVICE ?= $(shell cat esphome/.device_ip 2>/dev/null)

.PHONY: help esphome-compile esphome-flash clean gen-api-key

help:
	@echo "esphome-compile  validate firmware config and compile locally"
	@echo "esphome-flash    upload via USB or OTA (DEVICE=<serial-or-ip>)"
	@echo "gen-api-key      generate a random ESPHome API encryption key"
	@echo "clean            remove ESPHome build artifacts"

gen-api-key:
	@python3 -c "import base64, os; print(base64.b64encode(os.urandom(32)).decode())"

esphome-compile:
	cd esphome && uvx esphome compile esp32-bluetooth-foot-pedal.yaml

esphome-flash:
	cd esphome && uvx esphome upload esp32-bluetooth-foot-pedal.yaml $(if $(DEVICE),--device $(DEVICE),)

clean:
	rm -rf esphome/.esphome esphome/build build
