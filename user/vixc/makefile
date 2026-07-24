LLVM_CFLAGS := $(shell llvm-config --cflags)
LLVM_CXXFLAGS := $(shell llvm-config --cxxflags)
LLVM_LDFLAGS := $(shell llvm-config --ldflags --libs all)
LLD_LIBS := -llldELF -llldCommon

GCC ?= clang 
CXX ?= clang++
CLANG ?= clang
VIXC ?= ./seed/vixc

SRC_DIR := src
BUILD_DIR := build
RUNTIME_DIR := runtime

TARGET := $(BUILD_DIR)/vixc
BOOTSTRAP_OBJ := $(BUILD_DIR)/vixc-bootstrap.o
BOOTSTRAP_TARGET := $(BUILD_DIR)/vixc-bootstrap
HELPER_OBJ := $(BUILD_DIR)/helper.o
RUNTIME_OBJ := $(RUNTIME_DIR)/runtime.o
VIXC_OBJ := $(BUILD_DIR)/vixc.o
SELF_STAGE_OBJ := $(BUILD_DIR)/vixc-self.o
SELF_STAGE_TARGET := $(BUILD_DIR)/vixc-self
FREESTANDING_OBJ := $(BUILD_DIR)/vixc-aukos.o
FREESTANDING_HELPER_OBJ := $(BUILD_DIR)/freestanding-helper.o
FREESTANDING_WRITER_OBJ := $(BUILD_DIR)/elf-writer.o
FREESTANDING_LINKER_OBJ := $(BUILD_DIR)/elf-linker.o
FREESTANDING_HOST := $(BUILD_DIR)/vixc-core-host
FREESTANDING_VIXC ?= $(TARGET)
ELF_WRITER_TEST := $(BUILD_DIR)/elf-writer-test
FREESTANDING_DRIVER_HELPER_TEST := $(BUILD_DIR)/freestanding-driver-helper-test
API_OBJ := $(BUILD_DIR)/api.o
LLC_OBJ := $(BUILD_DIR)/Llc.o
LINKER_OBJ := $(BUILD_DIR)/Linker.o
PASSES_OBJ := $(BUILD_DIR)/Passes.o
LLVM_API_OBJS := $(API_OBJ) $(LLC_OBJ) $(LINKER_OBJ) $(PASSES_OBJ)
COMPILER_SUPPORT_OBJS := $(HELPER_OBJ) $(RUNTIME_OBJ) $(LLVM_API_OBJS)

VIX_SOURCES := $(shell find $(SRC_DIR) -type f -name '*.vix')

all: $(TARGET)
	rm -f vixc


$(BUILD_DIR) $(RUNTIME_DIR):
	mkdir -p $@

$(HELPER_OBJ): $(SRC_DIR)/helper.c | $(BUILD_DIR)
	$(GCC) -c $< -o $@ $(LLVM_CFLAGS) -Wno-deprecated-declarations

$(API_OBJ): lib/api.c | $(BUILD_DIR)
	$(GCC) -c $< -o $@

$(LLC_OBJ): lib/llvm/Llc.cpp | $(BUILD_DIR)
	$(CXX) -c $< -o $@ $(LLVM_CXXFLAGS) -Wno-deprecated-declarations

$(LINKER_OBJ): lib/llvm/Linker.cpp | $(BUILD_DIR)
	$(CXX) -c $< -o $@ $(LLVM_CXXFLAGS) -Wno-deprecated-declarations

$(PASSES_OBJ): lib/llvm/Passes.cpp | $(BUILD_DIR)
	$(CXX) -c $< -o $@ $(LLVM_CXXFLAGS) -Wno-deprecated-declarations

$(RUNTIME_OBJ): $(SRC_DIR)/runtime.c | $(RUNTIME_DIR)
	$(GCC) -c $< -o $@

$(FREESTANDING_HELPER_OBJ): $(SRC_DIR)/freestanding_helper.c | $(BUILD_DIR)
	$(GCC) -std=c17 -Wall -Wextra -Werror -c $< -o $@

$(FREESTANDING_WRITER_OBJ): $(SRC_DIR)/elf_writer.c | $(BUILD_DIR)
	$(GCC) -std=c17 -Wall -Wextra -Werror -c $< -o $@

$(FREESTANDING_LINKER_OBJ): $(SRC_DIR)/elf_linker.c | $(BUILD_DIR)
	$(GCC) -std=c17 -Wall -Wextra -Werror -c $< -o $@

$(FREESTANDING_OBJ): $(VIX_SOURCES) | $(BUILD_DIR)
	@test -x $(FREESTANDING_VIXC) || { \
		echo "missing $(FREESTANDING_VIXC); build the host compiler first" >&2; exit 1; }
	ulimit -s 65536 && $(FREESTANDING_VIXC) --target x86_64-unknown-none -obj \
		$(SRC_DIR)/aukos_main.vix -o $@

$(FREESTANDING_HOST): $(FREESTANDING_OBJ) $(FREESTANDING_HELPER_OBJ) \
		$(FREESTANDING_WRITER_OBJ) $(FREESTANDING_LINKER_OBJ) $(RUNTIME_OBJ) | $(BUILD_DIR)
	$(GCC) -no-pie -o $@ $(FREESTANDING_OBJ) $(FREESTANDING_HELPER_OBJ) \
		$(FREESTANDING_WRITER_OBJ) $(FREESTANDING_LINKER_OBJ) $(RUNTIME_OBJ)

freestanding-core: $(FREESTANDING_HOST)

$(ELF_WRITER_TEST): tests/elf_writer_test.c $(SRC_DIR)/elf_writer.c | $(BUILD_DIR)
	$(GCC) -std=c17 -Wall -Wextra -Werror $< -o $@

writer-test: $(ELF_WRITER_TEST) $(FREESTANDING_HOST) $(RUNTIME_OBJ)
	$(ELF_WRITER_TEST)
	sh tests/elf_writer_integration.sh $(FREESTANDING_HOST)
	sh tests/elf_linker_integration.sh $(FREESTANDING_HOST)
	$(FREESTANDING_DRIVER_HELPER_TEST)
	sh tests/aukos_driver_test.sh $(FREESTANDING_HOST)

$(FREESTANDING_DRIVER_HELPER_TEST): tests/freestanding_driver_helper_test.c \
		$(SRC_DIR)/freestanding_helper.c | $(BUILD_DIR)
	$(GCC) -std=c17 -Wall -Wextra -Werror $^ -o $@

$(VIXC):
	@echo "missing $(VIXC); run ./seed.sh first" >&2
	@exit 1

$(BOOTSTRAP_OBJ): $(VIX_SOURCES) $(VIXC) | $(BUILD_DIR)
	ulimit -s 65536 && $(VIXC) $(SRC_DIR)/main.vix -obj -o $@

$(BOOTSTRAP_TARGET): $(BOOTSTRAP_OBJ) $(COMPILER_SUPPORT_OBJS) | $(BUILD_DIR)
	$(CXX) -fuse-ld=lld -o $@ $^ $(LLVM_LDFLAGS) $(LLD_LIBS)

$(VIXC_OBJ): $(VIX_SOURCES) $(BOOTSTRAP_TARGET) | $(BUILD_DIR)
	ulimit -s 65536 && $(BOOTSTRAP_TARGET) $(SRC_DIR)/main.vix -obj -o $@

$(TARGET): $(VIXC_OBJ) $(COMPILER_SUPPORT_OBJS) | $(BUILD_DIR)
	$(CXX) -fuse-ld=lld -o $@ $^ $(LLVM_LDFLAGS) $(LLD_LIBS)

$(SELF_STAGE_OBJ): $(VIX_SOURCES) $(TARGET) | $(BUILD_DIR)
	ulimit -s 65536 && $(TARGET) --backend=self $(SRC_DIR)/main.vix -obj -o $@

$(SELF_STAGE_TARGET): $(SELF_STAGE_OBJ) $(COMPILER_SUPPORT_OBJS) | $(BUILD_DIR)
	$(CXX) -fuse-ld=lld -o $@ $^ $(LLVM_LDFLAGS) $(LLD_LIBS)

self-stage: $(SELF_STAGE_TARGET)

clean:
	rm -f $(TARGET) $(BOOTSTRAP_TARGET) $(SELF_STAGE_TARGET) $(HELPER_OBJ) $(RUNTIME_OBJ) $(BOOTSTRAP_OBJ) $(VIXC_OBJ) $(SELF_STAGE_OBJ) $(LLVM_API_OBJS) test test.ll

test: all
	sh tests/lexer_error_regression.sh $(TARGET)
	$(TARGET) tests/block_comment_regression.vix -o test
	./test >/dev/null

pytest: all
	python3 -m pytest ../tests/vixc0_tests/*.py

.PHONY: all self-stage freestanding-core writer-test clean test pytest
