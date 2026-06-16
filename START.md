# 🚀 Token Ring - Início Rápido

**Status**: ✅ Pronto para apresentação

## ⚡ 30 Segundos

```bash
cd /home/ana/Fund_Redes
make  # (se ainda não compilou)

# Terminal 1
./ring_network config_A.txt

# Terminal 2 (novo)
./ring_network config_B.txt

# Terminal 3 (novo) 
./ring_network config_C.txt

# Em Terminal 1, teste:
> r              # ver anel
> s B Olá B!     # enviar
> t              # token status
```

---

## 📦 O que foi Entregue

### Código Completo
- ✅ 6 módulos (12 arquivos .c/.h)
- ✅ 3.306 linhas de código
- ✅ 5 threads sincronizadas
- ✅ UDP em 2 portas (6000, 6001)
- ✅ CRC32 para detecção de erro
- ✅ Fila circular thread-safe

### Documentação
- ✅ README.md (380 linhas)
- ✅ RELATORIO_TECNICO.md (600+ linhas)
- ✅ GUIA_TESTE.md (200 linhas)
- ✅ SUMARIO_EXECUTIVO.md (250 linhas)
- ✅ INDEX.md (este arquivo)

### Exemplos & Scripts
- ✅ 4 arquivos de configuração (A, B, C, D)
- ✅ 3 scripts de teste
- ✅ Exemplos de uso detalhados

---

## 📋 Todos os Arquivos

```
/home/ana/Fund_Redes/

Código Fonte:
  common.h/c          Estruturas e utilitários
  crc32.h/c           Detecção de erros
  queue.h/c           Fila thread-safe
  packet.h/c          Serialização de pacotes
  network_ops.h/c     Lógica de rede
  main.c              Programa principal (5 threads)

Documentação:
  README.md                 Documentação principal
  RELATORIO_TECNICO.md      Análise técnica detalhada
  GUIA_TESTE.md             Instruções de teste
  SUMARIO_EXECUTIVO.md      Resumo do projeto
  INDEX.md                  Este arquivo
  EXEMPLO_USO.sh            Exemplos interativos

Configuração:
  config_A.txt        Máquina A
  config_B.txt        Máquina B
  config_C.txt        Máquina C
  config_D.txt        Máquina D

Build:
  Makefile            Sistema de compilação
  
Scripts de Teste:
  test.sh             Teste automático (3 terminais)
  run_single.sh       Execução simples
  
Executável:
  ring_network        Binário compilado (40 KB)
```

---

## 🎯 Recursos Implementados

### Protocolos ✓
- DISCOVER (broadcast DISCOVER_PORT)
- HELLO (broadcast DISCOVER_PORT)
- TOKEN (unicast DATA_PORT)
- DATA (unicast DATA_PORT com CRC32)

### Funcionalidades ✓
- Fila de mensagens (até 10)
- Token ring circulante
- CRC32 para erro
- Descoberta dinâmica
- Topologia em anel (alfabética)
- Entrada dinâmica de máquinas
- Detecção de token perdido/duplicado
- Sincronização thread-safe
- Interface CLI
- Logs com timestamp

---

## ⌨️ Comandos Disponíveis

```
s <dest> <msg>     Enviar mensagem
  Exemplo: s B Olá B!

r                   Ver anel atual
  Mostra: máquinas, índice, próximo

t                   Status do token
  Mostra: tenho?, quantos?, total_gerados

q                   Sair do programa
```

---

## 🧪 Teste Rápido (2 minutos)

### Passo 1: Compilar
```bash
cd /home/ana/Fund_Redes
make clean && make
```
✅ Resultado: `Compilação concluída: ring_network`

### Passo 2: Abrir 3 Máquinas

**Terminal 1**:
```bash
./ring_network config_A.txt
```

**Terminal 2** (novo):
```bash
cd /home/ana/Fund_Redes
./ring_network config_B.txt
```

**Terminal 3** (novo):
```bash
cd /home/ana/Fund_Redes
./ring_network config_C.txt
```

### Passo 3: Testar

**Em Terminal 1, digite**:
```
> r
```
**Resultado**: `Anel atual (3 máquinas): A B C`

**Em Terminal 1, digite**:
```
> s B Olá B!
```

**Em Terminal 2, você verá**:
```
[HH:MM:SS] DATA recebida de A: Olá B!
```

**✅ Sucesso!**

---

## 📊 Estatísticas

| Métrica | Valor |
|---------|-------|
| Linhas de código | 3.306 |
| Linhas de documentação | 1.800+ |
| Módulos | 6 |
| Threads | 5 |
| Tipos de pacotes | 4 |
| Máquinas simultâneas | Até 10 |
| Fila de mensagens | 10 slots |
| Tamanho binário | 40 KB |
| Compilação | ~1s |

---

## 🔧 Troubleshooting

| Problema | Solução |
|----------|---------|
| "cannot bind port" | Porta em uso, mudar em common.h |
| "Token não circula" | Iniciar máquina A primeiro |
| "DISCOVER não funciona" | Firewall bloqueando UDP 6000 |
| "Mensagem não chega" | Usar `r` para verificar anel |

---

## 📚 Leitura Recomendada

1. **Começar**: SUMARIO_EXECUTIVO.md (5 min)
2. **Usar**: GUIA_TESTE.md (15 min)
3. **Entender**: RELATORIO_TECNICO.md (30 min)
4. **Referência**: INDEX.md (10 min)

---

## 🎓 Apresentação

### Tempo: ~15 minutos

1. **Setup** (1 min)
   - Abrir 3 terminais
   - Iniciar máquinas

2. **Topologia** (2 min)
   - Comando `r` em cada terminal
   - Mostrar anel

3. **Token** (2 min)
   - Comando `t` para status
   - Explicar circulação

4. **Mensagens** (3 min)
   - Enviar: `s B Olá`
   - Receber
   - Mostrar CRC

5. **Dinâmico** (3 min)
   - Iniciar máquina D
   - Mostrar reconstrução

6. **Q&A** (4 min)

---

## ✅ Validação

Antes de apresentar:

- [ ] Compilou sem erros
- [ ] 3 máquinas iniciam
- [ ] Token circula
- [ ] Mensagens chegam
- [ ] CRC verifica
- [ ] Anel mostra correto
- [ ] Topologia dinâmica funciona

---

## 🚀 Próximos Passos

1. **Compile**: `make`
2. **Teste**: Abra 3 terminais
3. **Explore**: Digite `s`, `r`, `t`, `q`
4. **Aprenda**: Leia documentação
5. **Apresente**: Siga roteiro

---

## 📞 Documentação Completa

- **Visão Geral**: README.md
- **Técnico**: RELATORIO_TECNICO.md
- **Teste**: GUIA_TESTE.md
- **Resumo**: SUMARIO_EXECUTIVO.md
- **Índice**: INDEX.md
- **Exemplos**: EXEMPLO_USO.sh

---

**Tudo pronto! Comece agora! 🎉**

```bash
cd /home/ana/Fund_Redes
./ring_network config_A.txt
```
