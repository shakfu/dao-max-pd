MAX_VERSION := 9
PACKAGE_NAME := dao-max-pd
PACKAGE := "$(HOME)/Documents/Max\ $(MAX_VERSION)/Packages/$(PACKAGE_NAME)"


.PHONY: all build linux macos macos_universal windows dev \
		clean reset sync link setup sign

all: build

build:
	@mkdir -p build && \
		cd build && \
		cmake .. && \
		cmake --build . --config Release

windows: build

linux: build

macos:
	@mkdir -p build && \
		cd build && \
		cmake .. -GXcode && \
		cmake --build . --config Release


macos_universal:
	@mkdir -p build && \
		cd build && \
		cmake .. -GXcode -DMACOS_UNIVERSAL=ON && \
		cmake --build . --config Release

clean:
	@rm -f build/CMakeCache.txt

reset:
	@rm -rf build externals

sign:
	@codesign --sign - --timestamp --force externals/*.mxo/**/MacOS/*
	@/usr/bin/xattr -r -d com.apple.quarantine externals/*.mxo
	@codesign --sign - --timestamp --force externals/pd/**/*.pd_darwin
	@/usr/bin/xattr -r -d com.apple.quarantine externals/pd/**/*.pd_darwin

sync:
	@echo "updating submodule(s)"
	@git submodule init
	@git submodule update

link:
	@if ! [ -L "$(PACKAGE)" ]; then \
		ln -s "$(shell pwd)" "$(PACKAGE)" ; \
		echo "... symlink created" ; \
	else \
		echo "... symlink already exists" ; \
	fi

setup: sync link

