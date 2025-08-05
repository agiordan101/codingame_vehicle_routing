CC = g++
CXXFLAGS = -Wall -Wextra
CXXOPTIMIZE = -Ofast -funroll-loops -fomit-frame-pointer -finline-functions
CXXOPTION = -O3 -march=native -mtune=native -mno-vzeroupper
CXXTARGET = -mmovbe -maes -mpclmul -mavx -mavx2 -mf16c -mfma -msse3 -mssse3 -msse4.1 -msse4.2 -mrdrnd -mpopcnt -mbmi -mbmi2 -mlzcnt

CPP_FILE = vehicle_routing
TEST_FILE = "testset/1 Example"

all: $(CPP_FILE)

$(CPP_FILE): $(CPP_FILE).cpp
	$(CC) $(CXXFLAGS) $(CXXOPTIMIZE) $(CXXOPTION) $(CXXTARGET) $< -o $@

run: $(CPP_FILE)
	./$(CPP_FILE) < $(TEST_FILE)

clean:
	rm -f $(CPP_FILE)

.PHONY: all run clean