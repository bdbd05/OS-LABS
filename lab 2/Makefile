MALICIOUS_DIR := malicious_dir

SRC_DIR ?= tester

INTERVAL ?= 3

.PHONY: all build antivirus restore

all: build

build:
	mkdir -p "$(MALICIOUS_DIR)"

antivirus: build
	./antivirusd.sh "$(SRC_DIR)" "$(MALICIOUS_DIR)" "$(INTERVAL)"

restore: build
	./restore.sh "$(SRC_DIR)" "$(MALICIOUS_DIR)"
