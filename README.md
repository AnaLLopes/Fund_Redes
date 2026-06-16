# Rede em Anel com Token Ring - UDP

## 📋 Visão Geral

Este projeto implementa uma simulação de uma rede local em anel (Ring Network) usando o protocolo UDP. Máquinas são conectadas em um anel lógico onde um token circula permitindo que cada máquina transmita mensagens por vez.

## 🏗️ Arquitetura

### Componentes Principais

1. **common.h/c** - Estruturas de dados comuns, configurações e utilitários
2. **crc32.h/c** - Implementação de CRC32 para verificação de integridade
3. **queue.h/c** - Fila thread-safe para armazenar mensagens
4. **packet.h/c** - Serialização/desserialização de pacotes (DISCOVER, HELLO, TOKEN, DATA)
5. **network_ops.h/c** - Operações principais de rede
6. **main.c** - Programa principal com threads

### Threads

- **discover_thread**: Monitora descoberta de máquinas (DISCOVER/HELLO)
- **data_thread**: Recebe tokens e dados
- **token_controller_thread**: Controla geração e detecção de tokens (máquina A)
- **transmission_thread**: Gerencia transmissão de mensagens quando tem token
- **command_thread**: Interface de linha de comando

## 📦 Tipos de Pacotes

### DISCOVER (Tipo 10)
```
10:A:192.168.1.100
```
Enviado em broadcast quando máquina inicia.

### HELLO (Tipo 20)
```
20:B:192.168.1.101
```
Resposta em broadcast para DISCOVER.

### TOKEN (Tipo 1000)
```
1000
```
Token circulante que permite transmissão.

### DATA (Tipo 2000)
```
2000:A:B:maquinainexistente:3489574829:Olá mundo!
```
Formato: `tipo:origem:destino:status:crc:mensagem`

Status: `maquinainexistente`, `NAK` (erro), `ACK` (sucesso)

## ⚙️ Arquivo de Configuração

Formato: 5 linhas
```
A              # Alias (A, B, C, D, etc)
2              # Tempo token e dados (segundos)
20             # Probabilidade de erro (0-100%)
2              # Timeout do token (segundos)
1              # Tempo mínimo entre tokens (segundos)
```

Arquivos fornecidos:
- `config_A.txt` - Máquina A
- `config_B.txt` - Máquina B
- `config_C.txt` - Máquina C
- `config_D.txt` - Máquina D

## 🔧 Compilação

```bash
cd /home/ana/Fund_Redes
make          # Compilar
make clean    # Limpar arquivos de objeto
make debug    # Compilar com debug
```

## ▶️ Execução

### Máquina A (em terminal 1)
```bash
cd /home/ana/Fund_Redes
./ring_network config_A.txt
```

### Máquina B (em terminal 2)
```bash
cd /home/ana/Fund_Redes
./ring_network config_B.txt
```

### Máquina C (em terminal 3)
```bash
cd /home/ana/Fund_Redes
./ring_network config_C.txt
```

### Máquina D (em terminal 4, opcional)
```bash
cd /home/ana/Fund_Redes
./ring_network config_D.txt
```

## 📝 Comandos Interativos

Após iniciar uma máquina, você pode usar:

```
s <destino> <mensagem>  - Enviar mensagem
   Exemplo: s B Olá do A!

t                        - Ver status do token
                         Mostra: se tem token, quantos tokens, total gerados

r                        - Ver anel atual
                         Mostra topologia e próxima máquina

q                        - Sair do programa
```

## 🔄 Fluxo de Funcionamento

### Inicialização
1. Cada máquina inicia e envia DISCOVER em broadcast
2. Todas as máquinas respondem com HELLO
3. Máquinas se organizam em anel alfabético (A→B→C→D→A)
4. Máquina A gera o token inicial

### Transmissão de Dados
1. Máquina com token verifica fila de mensagens
2. Se há mensagens:
   - Calcula CRC32 da mensagem
   - Encapsula em pacote DATA
   - Envia para próxima máquina do anel
3. Pacote circula até chegar ao destino
4. Destino verifica CRC e responde com ACK ou NAK
5. Origem recebe resposta
6. Token é passado para próxima máquina

### Controle de Token
- **Máquina A** monitora passagem de token
- Se timeout: gera novo token (token perdido)
- Se token chega rápido demais: remove um token (token duplicado)

## 🧪 Exemplos de Teste

### Teste 1: Comunicação Simples
```
Terminal A> s B Oi B, tudo bem?
Terminal B> (receberá: DATA recebida de A: Oi B, tudo bem?)
```

### Teste 2: Broadcast (enviar para todos)
```
Terminal A> r  (ver anel: A B C D)
Terminal A> s BROADCAST Olá para todos!
Terminal B> (receberá)
Terminal C> (receberá)
Terminal D> (receberá)
```

### Teste 3: Observar Token
```
Terminal A> t  (STATUS: has=1, count=1, total_gerados=1)
Terminal B> t  (STATUS: has=1, count=1, total_gerados=0)
Terminal A> t  (STATUS: has=0, count=1, total_gerados=1)
```

### Teste 4: Adicionar Máquina Dinâmica
1. Ter A, B, C rodando
2. Iniciar D em novo terminal
3. Observar logs em todos os terminais
4. Ver anel reconstruído

## 🔍 Monitoramento

Os logs mostram:
- `[HH:MM:SS]` Timestamp de cada evento
- Tipos de pacotes recebidos/enviados
- Token recebido/enviado
- Anel reconstruído
- Dados transmitidos
- Erros e alertas

Exemplo de log:
```
[14:32:15] Rede inicializada: A em 192.168.1.100
[14:32:16] DISCOVER enviado: 10:A:192.168.1.100
[14:32:16] HELLO recebido de B (192.168.1.101)
[14:32:16] Anel reconstruído: A B (próximo: B)
[14:32:18] TOKEN INICIAL GERADO
[14:32:20] TOKEN recebido!
```

## 📊 Estrutura de Dados

### Machine
```c
struct {
    char alias;              // A, B, C, etc
    char ip[16];            // IP address
    struct sockaddr_in addr; // Endereço de socket
    int active;             // Máquina ativa?
}
```

### QueueMessage
```c
struct {
    char dest_alias;         // Destino
    char message[1024];      // Mensagem (até 10 por máquina)
    int attempts;           // Tentativas de envio
}
```

### DataPacket
```c
struct {
    int type;               // 2000
    char origin;            // Origem
    char dest;              // Destino
    char status[20];        // ACK, NAK, maquinainexistente
    uint32_t crc;          // CRC32
    char message[1024];    // Mensagem
}
```

## ⚠️ Comportamento do Sistema

### Detecção de Erros
- CRC32 calcula hash da mensagem
- Destino recalcula CRC
- Se diferente: envia NAK
- Origem retransmite na próxima passagem

### Máquina Inexistente
- Se destino não existe no anel
- STATUS = "maquinainexistente"
- Mensagem é descartada
- Token passa para próxima

### Inserção de Falhas
- Configurável via `probabilidade_erro` (0-100%)
- Altera bits aleatoriamente em mensagens
- Destino detecta via CRC

## 🔐 Sincronização

- Mutexes protegem: rede, token, fila
- Condition variables sincronizam transmissão
- Sem deadlocks (ordem consistente de locks)

## 📈 Performance

- CRC32: O(n) onde n = tamanho mensagem
- Fila: O(1) enqueue/dequeue
- Anel: atualizado quando máquina se junta
- Tokens: circulam com latência de rede UDP

## 🚀 Próximas Melhorias

- [ ] Retransmissão automática com backoff
- [ ] Compressão de mensagens
- [ ] Persistência de logs em arquivo
- [ ] GUI para visualizar topologia
- [ ] Suporte a múltiplos tokens (controlado)
- [ ] Métricas de desempenho

## 📞 Troubleshooting

### "cannot open socket"
- Verifique permissões de rede
- Tente com sudo se necessário

### "cannot bind port"
- Porta já em uso
- Espere 30s e tente novamente
- Ou mude as portas em common.h

### Token não circula
- Verifique se máquina A iniciou
- Observar logs para erros
- Verificar conectividade entre máquinas

### Mensagens não chegam
- Verificar se destino está no anel
- Usar comando `r` para ver topologia
- Verificar CRC nos logs

## 📄 Licença

Projeto acadêmico - Fundamentos de Redes 2026

## 👥 Desenvolvimento

- Estrutura modular com 6 componentes principais
- 5 threads simultâneas
- Sincronização com mutexes e condition variables
- Protocolo UDP em portas 6000 (descoberta) e 6001 (dados)
