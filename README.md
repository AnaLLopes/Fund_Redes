# Rede em Anel — Token Ring UDP

Implementação de uma rede local em anel com token, usando UDP como transporte.

---

## Compilação

```bash
chmod +x compile.sh
./compile.sh
```

Ou manualmente:
```bash
mkdir -p out
javac -encoding UTF-8 -d out/ src/*.java
```

---

## Execução

Rodar uma instância por máquina física:

```bash
java -cp out/ Main config_A.txt   # Máquina A
java -cp out/ Main config_B.txt   # Máquina B
java -cp out/ Main config_C.txt   # Máquina C
```

---

## Arquivo de Configuração

```
<apelido>
<tempo_token_e_dados_segundos>
<probabilidade_erro_percentual>
<timeout_token_segundos>
<tempo_minimo_entre_tokens_segundos>
```

Exemplo (`config_B.txt`):
```
B
2
20
10
3
```

**Observação sobre o timeout:** O `timeout_token` deve ser maior que
`N × tempo_token_e_dados`, onde N é o número de máquinas no anel,
pois cada nó introduz um delay ao repassar o token/dados.

---

## Comandos Disponíveis (durante execução)

| Comando                    | Descrição                                  |
|----------------------------|--------------------------------------------|
| `msg <DESTINO> <texto>`    | Enfileira mensagem unicast para `DESTINO`  |
| `msg BROADCAST <texto>`    | Enfileira mensagem para todas as máquinas  |
| `token+`                   | Injeta um token na rede manualmente        |
| `token-`                   | Remove o próximo token que chegar          |
| `status`                   | Exibe status desta máquina                 |
| `fila`                     | Exibe a fila de mensagens pendentes        |
| `anel`                     | Exibe a topologia atual do anel            |
| `quit` / `exit`            | Encerra a aplicação                        |

---

## Protocolo — Formato dos Pacotes

| Tipo     | Formato                                                    |
|----------|------------------------------------------------------------|
| DISCOVER | `10:<apelido>:<ip>`                                        |
| HELLO    | `20:<apelido>:<ip>`                                        |
| TOKEN    | `1000`                                                     |
| DATA     | `2000:<origem>:<destino>:<controle>:<crc>:<mensagem>`      |

Valores do campo `<controle>`:
- `maquinainexistente` — enviado pela origem; destino não respondeu
- `ACK` — destino recebeu sem erro de CRC
- `NAK` — destino detectou erro de CRC

---

## Topologia

O anel é formado em **ordem alfabética** dos apelidos. O último nó conecta ao primeiro (circular). A primeira máquina (A) gera o token inicial e monitora timeouts.

**Exemplo com 4 máquinas:**
```
[A] → B → C → D → A
```

---

## Arquitetura

### Classes

| Classe          | Responsabilidade                                         |
|-----------------|----------------------------------------------------------|
| `Main`          | Ponto de entrada; lê config e inicia o nó               |
| `Config`        | Lê e armazena configurações do arquivo                   |
| `RingNode`      | Orquestra toda a lógica do nó (discovery, anel, UI)      |
| `Packet`        | Fábricas e parsers para todos os tipos de pacote         |
| `MessageQueue`  | Fila thread-safe de mensagens pendentes (máx. 10)        |
| `CRCUtil`       | Cálculo e verificação de CRC32                           |
| `ErrorInserter` | Módulo de inserção aleatória de erros                    |
| `MachineInfo`   | Representa um nó (apelido + IP)                          |
| `Message`       | Representa uma mensagem na fila (destino + conteúdo)     |

### Threads

| Thread          | Função                                                    |
|-----------------|-----------------------------------------------------------|
| `DiscListener`  | Escuta DISCOVER/HELLO na porta 6000 (tempo todo)          |
| `RingListener`  | Escuta TOKEN/DATA na porta 6000 (tempo todo)              |
| `TokenMonitor`  | Monitora timeout do token (somente na controladora)       |
| Thread principal| Loop de entrada de comandos do usuário                    |

### Sincronização

Os métodos `onRingPacket`, `onDiscPacket` e `processCmd` são todos
`synchronized` na instância de `RingNode`, garantindo que apenas uma
operação de rede ocorra por vez. Variáveis compartilhadas de controle
(`sentData`, `amController`) são `volatile`; contadores de tempo e
flags de remoção usam `AtomicLong` e `AtomicBoolean`.

---

## Fluxo de Transmissão

```
1. A recebe o TOKEN
2. A tem mensagem na fila → envia pacote DATA(origem=A, dest=B, ctrl=maquinainexistente, crc=X)
3. Pacote passa por C (não é o destino → encaminha)
4. Pacote chega em B (destino):
     - Verifica CRC
     - Se OK  → ctrl = ACK;  exibe mensagem
     - Se ERR → ctrl = NAK
     - Encaminha ao próximo
5. Pacote retorna a A (origem):
     - ACK  → remove da fila, envia TOKEN
     - NAK  → mantém na fila (retransmite no próximo token), envia TOKEN
     - maquinainexistente → remove da fila (destino offline), envia TOKEN
```

---

## Portas UDP

| Porta | Uso                                         |
|-------|---------------------------------------------|
| 6000  | Descoberta (DISCOVER / HELLO) — broadcast   |
| 6000  | Anel (TOKEN / DATA) — unicast ao sucessor   |
