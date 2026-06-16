#!/bin/bash

# Script de teste simples em terminais sequenciais
# Use se gnome-terminal não funcionar

PROJECT_DIR="/home/ana/Fund_Redes"
cd "$PROJECT_DIR"

echo "=== Compilando ==="
make clean > /dev/null 2>&1
make

if [ ! -f "ring_network" ]; then
    echo "Erro: Falha na compilação"
    exit 1
fi

echo ""
echo "✓ Compilação OK"
echo ""
echo "=== Iniciando Máquina A ==="
echo "Para iniciar outra máquina em outro terminal, execute:"
echo "  cd $PROJECT_DIR"
echo "  ./ring_network config_B.txt"
echo ""
echo "=== Iniciando Máquina A ==="
./ring_network config_A.txt
