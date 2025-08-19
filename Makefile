CC = g++
CXXFLAGS = -Wall -Wextra
CXXOPTIMIZE = -Ofast -funroll-loops -fomit-frame-pointer -finline-functions
CXXOPTION = -O3 -march=native -mtune=native -mno-vzeroupper
CXXTARGET = -mmovbe -maes -mpclmul -mavx -mavx2 -mf16c -mfma -msse3 -mssse3 -msse4.1 -msse4.2 -mrdrnd -mpopcnt -mbmi -mbmi2 -mlzcnt

CPP_FILE = vehicle_routing
TEST_FILE = "testset/13 Benchmark Instance M-n200-k17"
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
	$(CC) $(CXXFLAGS) $(CXXOPTIMIZE) $(CXXOPTION) $(CXXTARGET) $< -o ./bins/$@

# Build and run the target
run: $(CPP_FILE)
	./bins/$(CPP_FILE) < $(TEST_FILE)
# 	@find $(TEST_DIR) -type f | while IFS= read -r test_file; do \
# 		./$(CPP_FILE) < "$$test_file" \
# 	done

evaluate: $(CPP_FILE)
	@echo "Running tests ..."
	@rm -f $(OUTPUT_FILE)
	@find $(TEST_DIR) -type f | while IFS= read -r test_file; do \
		echo "Processing "$$test_file""; \
		result=$$(./bins/$(CPP_FILE) < "$$test_file"); \
		echo "$$result" >> $(OUTPUT_FILE); \
	done
	@echo "All results have been written to $(OUTPUT_FILE)."

bf_finetune: $(CPP_FILE)
	python3 finetuning_bruteforce/ga_params_finetuning.py

ga_finetune: $(CPP_FILE)
	python3 finetuning_with_ga/ga_finetuning.py

clean:
	rm -f $(CPP_FILE)

.PHONY: all run clean
