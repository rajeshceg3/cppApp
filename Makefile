CXX = g++
CXXFLAGS = -std=c++11 -Wall -I/usr/include
LDFLAGS = -lcurl
SRCDIR = src
BUILDDIR = build
TARGET = sms_app

all: $(BUILDDIR)/$(TARGET)

$(BUILDDIR)/$(TARGET): $(BUILDDIR)/main.o
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/main.o: $(SRCDIR)/main.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR)/*

.PHONY: all clean
