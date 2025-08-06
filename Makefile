CC = g++
CXXFLAGS = -Wall -Wextra
CXXOPTIMIZE = -Ofast -funroll-loops -fomit-frame-pointer -finline-functions
CXXOPTION = -O3 -march=native -mtune=native -mno-vzeroupper
CXXTARGET = -mmovbe -maes -mpclmul -mavx -mavx2 -mf16c -mfma -msse3 -mssse3 -msse4.1 -msse4.2 -mrdrnd -mpopcnt -mbmi -mbmi2 -mlzcnt

CPP_FILE = vehicle_routing
TEST_FILE = "testset/3 Cocorico"
OUTPUT_FILE = "testoutput.txt"

SHELL := /bin/bash

# Test files
TEST_DIR := testset
TEST_FILES := $(wildcard testset/*)

# Results file
RESULTS_FILE := results.txt

all: $(CPP_FILE)

# Build the target
$(CPP_FILE): $(CPP_FILE).cpp
	$(CC) $(CXXFLAGS) $(CXXOPTIMIZE) $(CXXOPTION) $(CXXTARGET) $< -o $@

# Build and run the target
run: $(CPP_FILE)
	./$(CPP_FILE) < $(TEST_FILE)
# 	@find $(TEST_DIR) -type f | while IFS= read -r test_file; do \
# 		./$(CPP_FILE) < "$$test_file" \
# 	done

evaluate: $(CPP_FILE)
	@echo "Running tests ..."
	@rm -f $(OUTPUT_FILE)
	@find $(TEST_DIR) -type f | while IFS= read -r test_file; do \
		echo "Processing "$$test_file""; \
		result=$$(./$(CPP_FILE) < "$$test_file"); \
		echo "$$result" >> $(OUTPUT_FILE); \
	done
	@echo "All results have been written to $(OUTPUT_FILE)."

clean:
	rm -f $(CPP_FILE)

.PHONY: all run clean
