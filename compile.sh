#!/bin/bash
# Script de compilação — Token Ring UDP
set -e

mkdir -p out
echo "Compilando..."
javac -encoding UTF-8 -d out/ src/*.java
echo "✓ Compilado com sucesso em out/"
echo ""
echo "Para executar:"
echo "  java -cp out/ Main config_A.txt   (na máquina A)"
echo "  java -cp out/ Main config_B.txt   (na máquina B)"
echo "  java -cp out/ Main config_C.txt   (na máquina C)"
