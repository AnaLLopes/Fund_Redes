#!/bin/bash

# Script de teste para rede em anel
# Abre 3 máquinas em terminais separados

set -e

PROJECT_DIR="/home/ana/Fund_Redes"
cd "$PROJECT_DIR"

# Compilar
echo "=== Compilando projeto ==="
make clean > /dev/null 2>&1
make > /dev/null 2>&1

if [ ! -f "ring_network" ]; then
    echo "Erro: Falha na compilação"
    exit 1
fi

echo "✓ Compilação OK"
echo ""
echo "=== Iniciando teste com 3 máquinas ==="
echo "Abrindo 3 terminais (A, B, C)..."
echo ""

# Cores para visual
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Abrir máquina A
echo -e "${GREEN}Abrindo Máquina A...${NC}"
gnome-terminal --tab --title="Máquina A" -- bash -c "cd $PROJECT_DIR && echo 'Máquina A iniciando...' && ./ring_network config_A.txt; exec bash" &

sleep 1

# Abrir máquina B
echo -e "${BLUE}Abrindo Máquina B...${NC}"
gnome-terminal --tab --title="Máquina B" -- bash -c "cd $PROJECT_DIR && echo 'Máquina B iniciando...' && ./ring_network config_B.txt; exec bash" &

sleep 1

# Abrir máquina C
echo -e "${RED}Abrindo Máquina C...${NC}"
gnome-terminal --tab --title="Máquina C" -- bash -c "cd $PROJECT_DIR && echo 'Máquina C iniciando...' && ./ring_network config_C.txt; exec bash" &

sleep 2

echo ""
echo "=== Dicas de teste ==="
echo ""
echo "Em Máquina A, tente:"
echo "  s B Olá B, tudo certo?"
echo "  r     (ver anel)"
echo "  t     (ver status token)"
echo ""
echo "Em Máquina B, tente:"
echo "  s C Olá do B"
echo ""
echo "Ou inicie Máquina D depois:"
echo "  ./ring_network config_D.txt"
echo ""
echo "Para sair: digite 'q' em qualquer máquina"
