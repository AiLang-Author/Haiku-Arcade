# Haiku-only. On Linux this tree is sources + installer.
#   g++ -O2 -o arcade_shell_haiku arcade_shell_haiku.cxx -lbe -lgame
#   rc arcade_shell_haiku.rdef -o arcade_shell_haiku.rsrc
#   xres -o arcade_shell_haiku arcade_shell_haiku.rsrc
#
# Copyright © 2026 Sean Collins, 2 Paws Machine and Engineering. SCSL v1.0.

CXX ?= g++
CXXFLAGS ?= -O2 -Wall
TARGET = arcade_shell_haiku

.PHONY: all clean

all: $(TARGET)

$(TARGET): arcade_shell_haiku.cxx
	$(CXX) $(CXXFLAGS) -o $@ arcade_shell_haiku.cxx -lbe -lgame
	-rc arcade_shell_haiku.rdef -o arcade_shell_haiku.rsrc
	-xres -o $@ arcade_shell_haiku.rsrc
	-mimeset -f $@

clean:
	rm -f $(TARGET) arcade_shell_haiku.rsrc
