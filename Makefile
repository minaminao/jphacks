build: estimate.cpp
	g++ -o build/estimate estimate.cpp -std=c++17 -Wall -O3 -lboost_serialization
setup:
	cd data && wget -xnH -i filelist
	find data/ -name "*.xz"|xargs unxz
