all:
	g++ paq8px.cpp -DUNIX -Ofast -march=native -fomit-frame-pointer -o paq8px -DNOASM
