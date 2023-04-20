all: compilar baudline

compilar: Conexion_AIRA.cpp
	gcc Conexion_AIRA.cpp -o Conexion_AIRA -ljack -lfftw3 -lm	

baudline: correr_baudline.sh
	. ./correr_baudline.sh

