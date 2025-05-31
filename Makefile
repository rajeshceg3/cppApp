CXX = g++
CXXFLAGS = -std=c++17 -Wall -O1 -I/usr/include
# -lmicrohttpd for libmicrohttpd
# -lcurl for the original libcurl code (though commented out for now)
# -pthread for thread support (libmicrohttpd might need it)
# -ldl for dynamic linking (less likely needed for libmicrohttpd, but doesn't hurt)
LDFLAGS = -lcurl -lmicrohttpd -pthread -ldl
SRCDIR = src
BUILDDIR = build
TARGET = sms_app

# List of all object files - only main.o for now
OBJS = $(BUILDDIR)/main.o

all: $(BUILDDIR)/$(TARGET)

# Rule to link the target executable
$(BUILDDIR)/$(TARGET): $(OBJS)
	@mkdir -p $(BUILDDIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

# Rule to compile main.cpp into main.o
# Depends on main.cpp. Explicit dependency on specific headers like microhttpd.h
# is often not strictly needed here if system include paths are searched by default.
$(BUILDDIR)/main.o: $(SRCDIR)/main.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $(SRCDIR)/main.cpp

clean:
	rm -rf $(BUILDDIR)/* $(TARGET)

.PHONY: all clean
