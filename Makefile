QHOME ?= $(HOME)/q
AMPS_HOME ?= $(HOME)/amps-client
AMPS_LIB ?= $(AMPS_HOME)/lib/linux64

CC := gcc
CXX := g++

CFLAGS := -O2 -g -fPIC -m64 -DKXVER=3 \
          -I$(QHOME)/c/c \
          -I$(AMPS_HOME)/include

CXXFLAGS := $(CFLAGS) -std=c++17

LDFLAGS := -shared -m64
LDLIBS := -L$(AMPS_LIB) -lamps -lpthread -ldl

BUILD := build

all: $(BUILD)/libampsq.so

generator: $(BUILD)/ampsq-generator

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/ampsq.o: src/ampsq.c include/ampsq.h include/ampsq_backend.h | $(BUILD)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(BUILD)/ampsq_backend.o: src/ampsq_backend.cpp include/ampsq_backend.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -Iinclude -c $< -o $@

$(BUILD)/libampsq.so: $(BUILD)/ampsq.o $(BUILD)/ampsq_backend.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -rf $(BUILD)

install-q: $(BUILD)/libampsq.so
	mkdir -p $(QHOME)/l64
	cp $(BUILD)/libampsq.so $(QHOME)/l64/

.PHONY: all clean install-q


$(BUILD)/generator.o: generator/src/generator.cpp generator/include/generator.hpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -Igenerator/include -c $< -o $@

$(BUILD)/generator_main.o: generator/src/main.cpp generator/include/generator.hpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -Igenerator/include -c $< -o $@

$(BUILD)/ampsq-generator: $(BUILD)/generator.o $(BUILD)/generator_main.o
	$(CXX) -m64 -o $@ $^ $(LDLIBS)

