CXX      := g++
CXXFLAGS := -std=c++14 -O2 -I hls_stubs -Wno-unknown-pragmas

.PHONY: all csim rsa evictor aerp clean

all: csim

csim: rsa evictor aerp

rsa:
	@echo "=== RSA ==="
	$(CXX) $(CXXFLAGS) -I hls/rsa hls/rsa/rsa.cpp hls/rsa/rsa_tb.cpp -o build/tb_rsa
	./build/tb_rsa

evictor:
	@echo "=== EVICTOR ==="
	$(CXX) $(CXXFLAGS) -I hls/evictor hls/evictor/evictor.cpp hls/evictor/evictor_tb.cpp -o build/tb_evictor
	./build/tb_evictor

aerp:
	@echo "=== AERP ==="
	$(CXX) $(CXXFLAGS) -I hls/aerp hls/aerp/aerp.cpp hls/aerp/aerp_tb.cpp -o build/tb_aerp
	./build/tb_aerp

clean:
	rm -f build/tb_rsa build/tb_evictor build/tb_aerp

build:
	mkdir -p build
