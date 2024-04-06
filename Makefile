CXX ?= g++
OBJCOPY ?= objcopy
CXXFLAGS := -g -O2 -std=c++17 $(CXXFLAGS)

.PHONY: clean

all: symbol-overlay

%.psf: %.psf.gz
	gunzip -k $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

%.SDL.o: %.SDL.cpp
	$(CXX) $(CXXFLAGS) -c $^ $(sdl2-config --cflags) -o $@

# Debug info hangs compiler
src/x11name_to_utf16.o: src/x11name_to_utf16.cpp
	$(CXX) -O2 -std=c++17 -c $^ -o $@

src/font.o: font.psf
	$(OBJCOPY) -O elf32-littlearm -I binary $< $@

src/font.x86.o: font.psf
	$(OBJCOPY) -O elf64-x86-64 -I binary $< $@

symbol-overlay: src/main.o src/KeymapRender.o src/Overlay.o src/PSF.o src/x11name_to_utf16.o src/font.o
	$(CXX) -static $^ -o $@

symbol-overlay-test: src/main.o src/KeymapRender.o src/Overlay.SDL.o src/PSF.o src/x11name_to_utf16.o src/font.x86.o
	$(CXX) $^ -o $@ $(sdl2-config --libs) -lSDL2

clean:
	rm -f src/*.o symbol-overlay symbol-overlay-test
