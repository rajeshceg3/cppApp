CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Wpedantic
BUILDDIR ?= build

# Safe default: this executable contains no production SMS transport or curl link.
all: offline

offline: $(BUILDDIR)/sms_app_offline

$(BUILDDIR)/sms_app_offline: src/main.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -DSMS_OFFLINE_TEST=1 -o $@ $<

# Explicit opt-in build only. Never run this target as a verification scenario.
production: $(BUILDDIR)/sms_app

$(BUILDDIR)/sms_app: src/main.cpp FORCE
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $< -lcurl

verify: offline
	python3 tests/verify_offline.py $(BUILDDIR)/sms_app_offline

clean:
	rm -f $(BUILDDIR)/sms_app_offline
	# Preserve the historical tracked production executable.

FORCE:

.PHONY: all offline production verify clean FORCE
