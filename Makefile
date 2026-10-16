SHELL := /bin/bash
.SHELLFLAGS := -o pipefail -c

PYTHON ?= python3
CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -pthread -Wall

.PHONY: all report check page clean

all: page

build/neuron: model/neuron.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

# Runs every design variant over all input transitions (a few seconds).
# Writes the text report and the netlist JSON the diagram page is built from.
report: build/neuron
	./build/neuron --json build/netlist.json | tee results/report.txt

# Validate saved evidence without invoking licensed tools.
check:
	$(PYTHON) tools/check_evidence.py

# Self-contained diagram page: open build/site/index.html in a browser.
page: check report
	$(PYTHON) tools/build_page.py web/page.html build/netlist.json build/site/index.html

clean:
	rm -rf build
