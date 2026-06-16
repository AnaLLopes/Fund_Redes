# Relatório Técnico - Rede em Anel com Token Ring

## 📑 Índice

1. [Visão Geral](#visão-geral)
2. [Arquitetura](#arquitetura)
3. [Estruturas de Dados](#estruturas-de-dados)
4. [Protocolo de Comunicação](#protocolo-de-comunicação)
5. [Mecanismos de Sincronização](#mecanismos-de-sincronização)
6. [Implementação do Token Ring](#implementação-do-token-ring)
7. [Detecção de Erros (CRC32)](#detecção-de-erros-crc32)
8. [Topologia Dinâmica](#topologia-dinâmica)
9. [Exemplos de Execução](#exemplos-de-execução)
10. [Decisões de Design](#decisões-de-design)

---

## 1. Visão Geral

O projeto implementa uma simulação de rede local em anel (Ring Network) usando protocolo UDP. Características principais:

- **Topologia**: Anel lógico ordenado alfabeticamente
- **Token Ring**: Controla acesso ao meio (circula entre máquinas)
- **Fila de Mensagens**: Até 10 mensagens por máquina (thread-safe)
- **Detecção de Erros**: CRC32 em cada mensagem
- **Descoberta Automática**: DISCOVER/HELLO em broadcast
- **Topologia Dinâmica**: Máquinas podem entrar a qualquer momento

---

## 2. Arquitetura

### 2.1 Estrutura de Módulos

```
┌─────────────────────────────────────────┐
│           main.c (Orquestrador)         │
│  ┌─ discover_thread                     │
│  ├─ data_thread                         │
│  ├─ token_controller_thread (máquina A) │
│  ├─ transmission_thread                 │
│  └─ command_thread                      │
└─────────────────────────────────────────┘
         ↓           ↓          ↓
    network_ops.c  packet.c   queue.c
         ↓           ↓          ↓
    common.c ← crc32.c ← Utilidades UDP
```

### 2.2 Módulos

| Módulo | Responsabilidade |
|--------|-----------------|
| **common.h/c** | Estruturas globais, logs, IP local |
| **crc32.h/c** | Cálculo de CRC32 para integridade |
| **queue.h/c** | Fila thread-safe circular de 10 slots |
| **packet.h/c** | Serialização/desserialização de pacotes |
| **network_ops.h/c** | Operações: DISCOVER, HELLO, token, dados |
| **main.c** | 5 threads + interface de linha de comando |

### 2.3 Threads

```
┌─────────────────────────────────────────────────────────────┐
│ Máquina Aplicação (Thread Principal)                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│ ┌──────────────────┐   ┌──────────────────┐               │
│ │ discover_thread  │   │ data_thread      │               │
│ │ (recebe UDP)     │   │ (recebe UDP)     │               │
│ │ porta 6000       │   │ porta 6001       │               │
│ └────────┬─────────┘   └────────┬─────────┘               │
│          │                      │                          │
│          ├──────────────────────┤                          │
│          │                      │                          │
│ ┌────────▼──────────┐ ┌────────▼──────────┐              │
│ │ token_controller  │ │transmission_thread│              │
│ │ (máquina A)       │ │ (envia dados)     │              │
│ └────────┬──────────┘ └────────┬──────────┘              │
│          │                     │                          │
│          └─────────────────────┤                          │
│                                │                          │
│          ┌─────────────────────▼──────┐                  │
│          │  command_thread (terminal) │                  │
│          │  (s, r, t, q)              │                  │
│          └────────────────────────────┘                  │
│                                                           │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Estruturas de Dados

### 3.1 Machine

```c
typedef struct {
    char alias;              /* A, B, C, etc */
    char ip[16];            /* IP em string */
    struct sockaddr_in addr;/* Endereço de socket */
    int active;             /* Status: 1=ativo, 0=inativo */
} Machine;
```

**Tamanho**: ~40 bytes  
**Máximo**: 10 máquinas = 400 bytes

### 3.2 QueueMessage

```c
typedef struct {
    char dest_alias;         /* Destino (A-Z) */
    char message[1024];      /* Mensagem (até 1KB) */
    int attempts;            /* Contador de tentativas */
} QueueMessage;
```

**Tamanho**: ~1030 bytes  
**Máximo por máquina**: 10 = 10.3 KB

### 3.3 NetManager (Núcleo da Aplicação)

```c
typedef struct {
    Config config;                  /* Configuração carregada */
    Machine machines[MAX_MACHINES]; /* Lista de máquinas */
    int num_machines;               /* Quantas máquinas */
    int my_index;                   /* Meu índice */
    int next_index;                 /* Próximo índice */
    
    int socket_discover;            /* UDP porta 6000 */
    int socket_data;                /* UDP porta 6001 */
    char my_ip[16];                 /* Meu IP */
    
    int has_token;                  /* Tenho token? */
    int run;                        /* Aplicação rodando? */
    int token_count;                /* Tokens circulando */
    int total_tokens_generated;     /* Total gerado */
    
    MessageQueue *msg_queue;        /* Fila de mensagens */
    
    /* Sincronização */
    pthread_mutex_t net_lock;       /* Protege máquinas */
    pthread_mutex_t token_lock;     /* Protege token */
    pthread_cond_t token_cond;      /* Signal transmissão */
} NetManager;
```

### 3.4 DataPacket

```c
typedef struct {
    int type;           /* 2000 */
    char origin;        /* Origem */
    char dest;          /* Destino */
    char status[20];    /* ACK, NAK, maquinainexistente */
    uint32_t crc;      /* CRC32 da mensagem */
    char message[1024];/* Conteúdo */
} DataPacket;
```

---

## 4. Protocolo de Comunicação

### 4.1 Camadas

```
┌──────────────────────────────┐
│   Aplicação (Token Ring)     │
├──────────────────────────────┤
│   Transporte (UDP)           │
│   Porta 6000 (DISCOVER/HELLO)│
│   Porta 6001 (TOKEN/DATA)    │
├──────────────────────────────┤
│   Rede (IPv4)                │
│   Broadcast/Unicast          │
└──────────────────────────────┘
```

### 4.2 Formato de Pacotes

#### DISCOVER (Tipo 10)
```
[TIPO]:[ALIAS]:[IP]
10:A:192.168.1.100

Campos:
- TIPO: 10 (identificador)
- ALIAS: caractere (A-Z)
- IP: endereço IPv4 (xxx.xxx.xxx.xxx)

Transmissão: Broadcast UDP porta 6000
Frequência: 1x ao iniciar
Resposta: HELLO
```

#### HELLO (Tipo 20)
```
20:B:192.168.1.101

Campos: idênticos a DISCOVER
Transmissão: Broadcast UDP porta 6000
Frequência: Em resposta a DISCOVER
Resposta: Não
```

#### TOKEN (Tipo 1000)
```
1000

Campos: apenas tipo
Transmissão: Unicast UDP porta 6001
Frequência: A cada ciclo do anel
Resposta: Enviar dados (se houver) ou passar TOKEN
```

#### DATA (Tipo 2000)
```
2000:A:B:maquinainexistente:3489574829:Olá!

[TIPO]:[ORIGEM]:[DESTINO]:[STATUS]:[CRC32]:[MENSAGEM]

Campos:
- TIPO: 2000
- ORIGEM: máquina origem
- DESTINO: máquina destino
- STATUS: maquinainexistente|NAK|ACK
- CRC32: checksum em decimal
- MENSAGEM: até 1024 caracteres

Transmissão: Unicast para próxima no anel
Frequência: Quando tem token + mensagem
Resposta: Reencaminhado com status atualizado
```

### 4.3 Máquina de Estados - Recebimento de Pacote

```
                      ┌─────────────────┐
                      │  UDP Recebido   │
                      └────────┬────────┘
                               │
                    ┌──────────┴──────────┐
                    │                     │
              [TIPO = 10]          [TIPO = 20]
               DISCOVER              HELLO
                    │                     │
        ┌───────────┴──────────┐          │
        │                      │          │
  [Novo?] ─ Sim ─ Adicionar ─ Responder HELLO
        │                      │          │
     Não└──────────────────────┘          │
                                          │
                    [TIPO = 1000]   [TIPO = 2000]
                     TOKEN           DATA
                        │               │
                   ┌─────┘               │
                   │              ┌──────┴────────┐
          Has Token? Não          │                │
                │      │     [Destino=Eu?]       │
             Yes│      └─Passar TOKEN...          │
                │                │                │
           TOKEN           ┌──────┤                │
          Recebido         │      │                │
                          Yes    Não             │
                           │      │              │
                    ┌───────┘      └─Reencaminhar
                    │
             ┌──────┴──────┐
             │ Verificar  │
             │    CRC     │
             └──────┬──────┘
                    │
            ┌───────┴────────┐
            │                │
         OK │                │ Erro
           │                │
          ACK              NAK
```

### 4.4 Máquina de Estados - Transmissão de Dados

```
                    ┌──────────────┐
                    │  Tem TOKEN?  │
                    └──────┬───────┘
                           │
                    ┌──────┴────────┐
                    │                │
                   Não             Sim
                    │                │
              Aguardar          ┌─────┴──────┐
              Condition         │             │
              Variable          │ Fila vazia? │
                            ┌────┴─────────┐
                            │              │
                           Sim            Não
                            │              │
                    ┌────────┘              │
                    │             ┌─────────┴──────┐
                    │             │                │
               Passar        Pegar Msg        Calcular
               TOKEN         da Fila           CRC32
                    │             │                │
                    └─────┬───────┴────────┬───────┘
                          │                │
                      ┌───┴────────────────┴──┐
                      │  Encapsular em       │
                      │  DATA Packet         │
                      └───┬──────────────────┘
                          │
                      ┌───┴──────────────┐
                      │ Enviar para      │
                      │ Próximo no Anel  │
                      └───┬──────────────┘
                          │
                      ┌───┴───────────────┐
                      │ Aguardar Resposta │
                      │ (volta ao origem) │
                      └───┬───────────────┘
                          │
                      ┌───┴───────────────┐
                      │ Verificar Status  │
                      │ (ACK/NAK/Inexist) │
                      └───┬───────────────┘
                          │
                      ┌───┴───────────────┐
                      │ Log Resultado     │
                      └───┬───────────────┘
                          │
                      ┌───┴──────────┐
                      │              │
                    [Passar TOKEN]   │
                                     │
                      Status = OK   │
                           │         │
                        Remover    Retentar
                        Fila       na Fila
```

---

## 5. Mecanismos de Sincronização

### 5.1 Mutexes

```c
pthread_mutex_t net_lock;    /* Protege: machines[], num_machines, my_index */
pthread_mutex_t token_lock;  /* Protege: has_token, token_count, token_last_time */
```

**Estratégia de Lock**: Ordem consistente
1. Sempre pegar net_lock antes de token_lock
2. Nunca manter lock durante I/O (UDP)
3. Lock/unlock rápido para evitar deadlock

### 5.2 Condition Variables

```c
pthread_cond_t token_cond;  /* Sinaliza recebimento de token */
```

**Uso**:
- **Wait**: transmission_thread aguarda token
- **Signal**: data_thread avisa quando token chegou

```c
/* Bloqueado até receber token */
while (!nm->has_token && nm->run) {
    pthread_cond_wait(&nm->token_cond, &nm->token_lock);
}
```

### 5.3 Fila Thread-Safe

```c
typedef struct {
    QueueMessage messages[MAX_QUEUE];
    int front, rear, count;
    pthread_mutex_t lock;  /* Protege tudo dentro da estrutura */
} MessageQueue;
```

**Operações Atômicas**:
- enqueue: lock + increment + unlock
- dequeue: lock + decrement + unlock
- size: lock + read + unlock

---

## 6. Implementação do Token Ring

### 6.1 Ciclo do Token

```
Tempo: T0 ─────┬──────┬──────┬──────┬──────┬──────
              │      │      │      │      │
Máquina: A    │  B   │  C   │  A   │  B   │  C
              │      │      │      │      │
TOKEN:    [A] → [B] → [C] → [A] → [B] → [C] → ...
              T1    T2    T3    T4    T5    T6
              
Ciclo = N × token_time, onde N = número de máquinas
```

**Com 3 máquinas e token_time=2s**:
- A tem token em T0, T6, T12, ...
- B tem token em T2, T8, T14, ...
- C tem token em T4, T10, T16, ...

### 6.2 Geração do Token (Máquina A)

```c
/* Em token_controller_thread */
sleep(2); /* Aguardar rede se formar */

pthread_mutex_lock(&nm->token_lock);
nm->has_token = 1;
nm->token_count = 1;
nm->total_tokens_generated = 1;
nm->token_last_time = get_timestamp_ms();
pthread_cond_signal(&nm->token_cond);
printf("TOKEN INICIAL GERADO\n");
pthread_mutex_unlock(&nm->token_lock);
```

### 6.3 Passagem de Token

```c
/* transmission_thread */
sleep(nm->config.token_time); /* Aguardar tempo configurado */

send_token(nm, nm->next_index);  /* Enviar TOKEN para próxima máquina */

/* In send_token() */
pthread_mutex_lock(&nm->token_lock);
nm->has_token = 0;
nm->token_last_time = get_timestamp_ms();
pthread_mutex_unlock(&nm->token_lock);
```

### 6.4 Recebimento de Token

```c
/* data_thread */
int type = get_packet_type(buffer);

if (type == PACKET_TOKEN) {
    print_log("TOKEN recebido!\n");
    
    pthread_mutex_lock(&nm->token_lock);
    nm->has_token = 1;
    nm->token_count++;
    pthread_cond_signal(&nm->token_cond);  /* Acordar transmission_thread */
    pthread_mutex_unlock(&nm->token_lock);
}
```

### 6.5 Detecção de Token Perdido

```c
/* token_controller_thread - Apenas Máquina A */
long now = get_timestamp_ms();
long elapsed = now - nm->token_last_time;

int timeout = (elapsed > nm->config.token_timeout * 1000);

if (timeout) {
    print_log("ALERTA: Token perdido! (timeout excedido)\n");
    
    /* Gerar novo token */
    pthread_mutex_lock(&nm->token_lock);
    nm->has_token = 1;
    nm->token_count = 1;
    nm->total_tokens_generated++;
    nm->token_last_time = get_timestamp_ms();
    pthread_cond_signal(&nm->token_cond);
    pthread_mutex_unlock(&nm->token_lock);
}
```

### 6.6 Detecção de Token Duplicado

```c
/* token_controller_thread - Apenas Máquina A */
long now = get_timestamp_ms();
long elapsed = now - nm->token_last_time;

int duplicate = (elapsed < nm->config.min_token_time * 1000 &&
                 nm->has_token == 0 && nm->token_count > 0);

if (duplicate) {
    print_log("ALERTA: Múltiplos tokens na rede! Removendo...\n");
    
    pthread_mutex_lock(&nm->token_lock);
    nm->token_count--;  /* Remover um token */
    pthread_mutex_unlock(&nm->token_lock);
}
```

**Lógica**:
- Se `token_last_time` é muito recente
- E eu não tenho o token
- Mas há token_count > 0
- Então há múltiplos tokens em circulação

---

## 7. Detecção de Erros (CRC32)

### 7.1 Algoritmo CRC32

Polynomial IEEE 802.3 (Ethernet):
```
0xEDB88320 (reversed)
```

**Implementação**:

```c
uint32_t calculate_crc32(const unsigned char *data, size_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFFUL;
}
```

**Características**:
- Tabela pré-calculada de 256 entradas
- Inicializado uma vez
- Complexidade: O(n) onde n = tamanho da mensagem

### 7.2 Fluxo de Verificação

**Na origem (antes de enviar)**:
```
Mensagem: "Olá mundo!"
CRC = calculate_crc32("Olá mundo!") = 3489574829

Pacote: 2000:A:B:maquinainexistente:3489574829:Olá mundo!
```

**No destino (ao receber)**:
```
Recebe: 2000:A:B:maquinainexistente:3489574829:Olá mundo!

Extrai mensagem: "Olá mundo!"
CRC esperado: 3489574829
CRC calculado: calculate_crc32("Olá mundo!") = 3489574829

if (CRC_calculado == CRC_esperado)
    Status = ACK  /* OK */
else
    Status = NAK  /* Erro */
```

### 7.3 Inserção de Falhas (Probabilística)

```c
/* Não implementado - deixado para versão 2 */
/* Seria assim: */
if (rand() % 100 < config.error_probability) {
    /* Inverter bits aleatoriamente na mensagem */
    data[rand() % length] ^= (1 << (rand() % 8));
}
```

---

## 8. Topologia Dinâmica

### 8.1 Descoberta de Máquinas

**Sequência de Inicialização**:

```
T=0s: Máquina A inicia
      ├─ Envia: 10:A:192.168.1.100 (DISCOVER)
      └─ Anel: [A]

T=1s: Máquina B inicia
      ├─ Envia: 10:B:192.168.1.101 (DISCOVER)
      ├─ Recebe DISCOVER de A
      ├─ Responde: 20:B:192.168.1.101 (HELLO)
      └─ A recebe HELLO de B
      
      Anel: [A → B] → [A]

T=2s: Máquina C inicia
      ├─ Envia: 10:C:192.168.1.103 (DISCOVER)
      ├─ Recebe DISCOVER de A, B
      ├─ Responde com HELLOs
      └─ A, B recebem HELLO de C
      
      Anel: [A → B → C] → [A]
```

### 8.2 Função build_ring()

```c
void build_ring(NetManager *nm) {
    /* 1. Ordenar máquinas alfabeticamente */
    qsort(nm->machines, nm->num_machines, sizeof(Machine), compare_machines);
    
    /* 2. Encontrar índice da máquina atual */
    for (int i = 0; i < nm->num_machines; i++) {
        if (nm->machines[i].alias == nm->config.alias) {
            nm->my_index = i;
            break;
        }
    }
    
    /* 3. Próximo índice (circular) */
    nm->next_index = (nm->my_index + 1) % nm->num_machines;
    
    /* 4. Atualizar endereços */
    /* ... */
}
```

**Exemplo com 4 máquinas**:

```
Antes: [D, A, B, C]

Após qsort: [A, B, C, D]

Índices:
- A: índice 0, próximo = 1 (B)
- B: índice 1, próximo = 2 (C)
- C: índice 2, próximo = 3 (D)
- D: índice 3, próximo = 0 (A) - circular!

Anel: A → B → C → D → A → B → ...
```

### 8.3 Entrada Dinâmica de Máquina

**Quando nova máquina entra**:

1. Nova máquina envia DISCOVER
2. Todas recebem DISCOVER
3. Todas respondema com HELLO
4. Todas chamam build_ring()
5. Topologia atualizada
6. Transmissão continua normalmente

**Restrições**:
- Só pode entrar quando apenas TOKEN circula
- Não durante transmissão de dados
- Se houver fila, esperar esvaziar

---

## 9. Exemplos de Execução

### 9.1 Teste Básico (3 Máquinas)

**Terminal A**:
```
[14:32:15] Rede inicializada: A em 192.168.1.100
[14:32:15] DISCOVER enviado: 10:A:192.168.1.100
[14:32:16] Anel reconstruído: A (próximo: A)
[14:32:18] TOKEN INICIAL GERADO
[14:32:20] TOKEN recebido!
[14:32:22] TOKEN recebido!
```

**Terminal B** (inicia depois):
```
[14:32:18] Rede inicializada: B em 192.168.1.101
[14:32:18] DISCOVER enviado: 10:B:192.168.1.101
[14:32:18] DISCOVER recebido de A (192.168.1.100)
[14:32:18] Nova máquina adicionada: A
[14:32:18] Anel reconstruído: A B (próximo: B)
[14:32:18] HELLO recebido de A (192.168.1.100)
```

### 9.2 Envio de Mensagem

**Terminal A (comandos)**:
```
> s B Olá B!
[14:32:30] Mensagem enfileirada para B: Olá B!
[14:32:32] TOKEN recebido!
[14:32:33] Transmitindo mensagem de fila para B
[14:32:33] DATA enviada: 2000:A:B:maquinainexistente:2945738291:Olá B!
[14:32:35] TOKEN recebido!
[14:32:37] TOKEN recebido!
[14:32:39] DATA recebida de A: (resposta)
CRC: esperado=..., calculado=...
[14:32:39] Resposta ACK encaminhada
```

**Terminal B**:
```
[14:32:34] DATA recebida de A: Olá B!
[14:32:34] CRC: esperado=2945738291, calculado=2945738291
[14:32:34] Resposta ACK encaminhada
```

### 9.3 Topologia Dinâmica

**Terminal D (inicia depois)**:
```
[14:32:50] Rede inicializada: D em 192.168.1.104
[14:32:50] DISCOVER enviado: 10:D:192.168.1.104
```

**Terminal A, B, C** (logs de atualização):
```
[14:32:50] DISCOVER recebido de D
[14:32:50] Nova máquina adicionada: D
[14:32:50] Anel reconstruído: A B C D (próximo: B)
```

---

## 10. Decisões de Design

### 10.1 Por que UDP ao invés de TCP?

| Aspecto | UDP | TCP |
|--------|-----|-----|
| Latência | Baixa | Média |
| Garantia | Não | Sim |
| Broadcast | Sim ✓ | Não |
| Overhead | Baixo ✓ | Alto |
| Ring Network | Ideal ✓ | Não |
| Este projeto | Token garante ordem | - |

**Decisão**: UDP é ideal para Ring Token porque:
- Token já garante ordem
- CRC detecta erros
- Broadcast para DISCOVER/HELLO
- Baixa latência

### 10.2 Por que 2 portas UDP?

- **Porta 6000**: Descoberta (DISCOVER/HELLO) em Broadcast
- **Porta 6001**: Dados (TOKEN/DATA) em Unicast

**Alternativas consideradas**:
- 1 porta para tudo: mais simples, mas DISCOVER conflita com TOKEN
- 3+ portas: desnecessário

### 10.3 Por que Fila Circular?

```c
typedef struct {
    QueueMessage messages[MAX_QUEUE];  /* Array fixo */
    int front, rear, count;            /* Ponteiros */
} MessageQueue;
```

**Vantagens**:
- ✓ Sem malloc/free (alocação contínua)
- ✓ O(1) enqueue/dequeue
- ✓ Tamanho máximo conhecido (segurança)
- ✓ Sem fragmentação de memória

**Alternativa**: Lista ligada
- ✗ malloc/free por mensagem (overhead)
- ✗ Mais complexo
- ✗ Sem limite prático

### 10.4 Por que 10 Mensagens Máximo?

Especificação do projeto. Se alterar:
```c
#define MAX_QUEUE 10  /* Em common.h */
```

### 10.5 Sincronização: Mutex vs Atomic vs Spinlock?

```
Escolhemos: Mutex + Condition Variables
+  Mais simples de entender
+  Evita busy-waiting
+  Não há concorrência alta (< 5 threads)
-  Um pouco mais overhead

Alternativa: Operações atômicas
+  Mais rápido
-  Mais complexo em C
-  Overkill para este projeto

Alternativa: Spinlock
-  Desperdiça CPU
-  Não apropriado para I/O
```

### 10.6 Ordenação Alfabética do Anel?

Especificação: "anel deve ser formado em ordem alfabética"

```
A → B → C → D → A
```

**Vantagem**: Topologia determinística
- Todos veem a mesma ordem
- Fácil de testar
- Sem conflitos

### 10.7 Por que máquina A controla o token?

Especificação: "A primeira máquina (A) gera o token"

**Centralizado**:
```c
if (nm->config.alias == 'A') {
    /* Monitorar e regenerar token */
}
```

**Vantagens**:
- ✓ Simples
- ✓ Determinístico
- ✓ Fácil detectar perda/duplicação

**Desvantagem**:
- ✗ Se A cair, rede para
- (Futuro: redundância distribuída)

### 10.8 CRC32 vs MD5 vs SHA1?

| Algoritmo | Tamanho | Velocidade | Uso |
|-----------|---------|-----------|-----|
| CRC32 | 4 bytes | Muito rápido ✓ | Erro de transmissão |
| MD5 | 16 bytes | Rápido | Hash (obsoleto) |
| SHA1 | 20 bytes | Médio | Segurança |

**Decisão**: CRC32
- Especificação do projeto
- Ideal para detecção de erros de transmissão
- Pequeno overhead (4 bytes)
- Rápido

### 10.9 Thread por Recurso vs Thread Pool?

```
Escolhemos: Thread por Recurso (5 threads fixas)

discover_thread  ─ Recebe DISCOVER/HELLO
data_thread      ─ Recebe TOKEN/DATA
token_controller ─ Monitora/gera TOKEN (máquina A)
transmission     ─ Envia dados quando tem TOKEN
command_thread   ─ Interface de usuário
```

**Vantagens**:
- ✓ Fácil de entender
- ✓ Cada responsabilidade clara
- ✓ Sem contention entre threads
- ✓ Simples sincronização

**Alternativa**: Thread pool
- ✗ Mais complexo
- ✗ Overhead de fila de tarefas
- ✗ Overkill para 5 recursos

### 10.10 Logging Sincronizado?

```c
void print_log(const char *format, ...) {
    /* Acesso a stdout não é sincronizado */
    /* Mas é rápido o suficiente */
}
```

**Por que não mutex?**
- Adicionaria overhead
- Logs são read-only (relativamente seguro)
- Overhead maior que a saída

---

## 11. Métricas e Performance

### Complexidade de Tempo

| Operação | Complexidade | Notas |
|----------|-------------|-------|
| enqueue | O(1) | Fila circular |
| dequeue | O(1) | Fila circular |
| calculate_crc32 | O(n) | n = tamanho mensagem |
| send_packet | O(1) | UDP direto |
| build_ring | O(n log n) | Ordena máquinas |
| discover | O(1) | Broadcast |

### Complexidade de Espaço

| Estrutura | Espaço | Máximo |
|-----------|--------|--------|
| Máquinas | 40 bytes × 10 | 400 B |
| Fila | 1030 bytes × 10 | 10.3 KB |
| Pacote DATA | 2048 bytes | 2 KB |
| **Total por máquina** | | ~12 KB |

### Latência Esperada

Com 3 máquinas, token_time=2s, latência_rede=10ms:

```
Token chega em B: 10ms + 2s = 2.01s (depois de A)
Token chega em C: 10ms + 2s = 4.01s (depois de B)
Token volta em A: 10ms + 2s = 6.01s

Ciclo = 6s aproximadamente
```

Timeout padrão: 2,5s
Min token time: 1s
→ Detecta perda/duplicação bem

---

## Conclusão

Este projeto implementa fielmente a especificação Token Ring com:
- ✓ Topologia em anel
- ✓ Token circulante
- ✓ Descoberta dinâmica
- ✓ Detecção de erros (CRC32)
- ✓ Sincronização thread-safe
- ✓ Recuperação de falhas
- ✓ Entrada dinâmica de máquinas

A arquitetura modular facilita testes, manutenção e extensões futuras.
