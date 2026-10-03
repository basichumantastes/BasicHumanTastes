# Bancs d'essai hors de Rack, liés à la libRack du SDK
RACK_DIR ?= ../../Rack-SDK
FLAGS = -std=c++11 -O2 -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include -Wno-\#warnings
TESTS = test_jules test_odette test_colette

all: $(TESTS)

test_%: test_%.cpp ../src/*.cpp ../src/*.hpp
	c++ $(FLAGS) -o $@ $< -L$(RACK_DIR) -lRack

run: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; DYLD_LIBRARY_PATH=$(abspath $(RACK_DIR)) ./$$t || exit 1; done

clean:
	rm -f $(TESTS)
