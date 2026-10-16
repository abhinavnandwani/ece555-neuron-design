SHELL := /bin/bash
.SHELLFLAGS := -o pipefail -c

CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -pthread -Wall

.PHONY: all report page clean

all: page

build/neuron: model/neuron.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

# Runs every design variant over all input transitions (about 30 s).
# Writes the text report and the netlist JSON the diagram page is built from.
report: build/neuron
	./build/neuron --json build/netlist.json | tee results/report.txt

# Self-contained diagram page: open build/site/index.html in a browser.
page: report
	python3 tools/build_page.py web/page.html build/netlist.json build/site/index.html

clean:
	rm -rf build
