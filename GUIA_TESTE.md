# Rede em Anel - Guia de Teste Rápido

## 🚀 Início Rápido (3 máquinas)

### Terminal 1 - Máquina A
```bash
cd /home/ana/Fund_Redes
./ring_network config_A.txt
```

### Terminal 2 - Máquina B (abra novo terminal)
```bash
cd /home/ana/Fund_Redes
./ring_network config_B.txt
```

### Terminal 3 - Máquina C (abra novo terminal)
```bash
cd /home/ana/Fund_Redes
./ring_network config_C.txt
```

## 📝 Operações Básicas

### Visualizar Anel
Em qualquer máquina, digite:
```
r
```
Resultado esperado:
```
Anel atual (3 máquinas): A B C 
Máquina atual: B (índice 1), próximo: C (índice 2)
```

### Ver Status do Token
```
t
```
Resultado esperado:
```
Token status: has=0, count=1, total_gerados=0
```

### Enviar Mensagem Simples
De A para B:
```
s B Oi B!
```

Logs esperados em A:
```
[14:35:22] Mensagem enfileirada para B: Oi B!
[14:35:22] Transmitindo mensagem de fila para B
[14:35:22] DATA enviada: 2000:A:B:maquinainexistente:1234567890:Oi B!
```

Logs esperados em B:
```
[14:35:22] DATA recebida de A: Oi B!
[14:35:22] CRC: esperado=1234567890, calculado=1234567890
[14:35:22] Resposta ACK encaminhada
```

## 🧪 Cenários de Teste

### Teste 1: Comunicação Básica
1. Iniciar A, B, C
2. Aguardar estabilização (5 segundos)
3. Em A: `s B Hello B`
4. Ver mensagem recebida em B
5. Em B: `s C Message from B`
6. Ver mensagem recebida em C

### Teste 2: Múltiplas Mensagens
1. Em A enfileirar 3 mensagens:
   ```
   s B Msg 1
   s B Msg 2
   s B Msg 3
   ```
2. A será processadas na ordem
3. B receberá todas

### Teste 3: Topologia Dinâmica
1. Ter A, B, C rodando
2. Abrir novo terminal e iniciar D:
   ```
   cd /home/ana/Fund_Redes
   ./ring_network config_D.txt
   ```
3. Todos enviarão DISCOVER/HELLO
4. Anel será A B C D
5. Em qualquer máquina: `r` mostra novo anel

### Teste 4: Comunicação Cruzada
Em A: `s C Msg from A to C`
- A → B → C → B → A (circula completo)
- C recebe, responde ACK
- B encaminha resposta

### Teste 5: Máquina Inexistente
1. Em A: `s X Message`
2. Máquina X não existe
3. Mensagem retorna com "maquinainexistente"
4. Log mostra erro

## 🔍 Interpretando Logs

### TOKEN recebido
```
[14:35:10] TOKEN recebido!
```
Esta máquina recebeu o token e pode enviar dados.

### DATA enviada
```
[14:35:11] DATA enviada: 2000:A:B:maquinainexistente:1234567890:Oi B!
```
Pacote foi encaminhado para próxima máquina no anel.

### Anel reconstruído
```
[14:35:05] Anel reconstruído: A B C (próximo: C)
```
Nova máquina foi adicionada, topologia atualizada.

### DISCOVER/HELLO
```
[14:35:03] DISCOVER recebido de C (192.168.1.103)
[14:35:03] HELLO recebido de C (192.168.1.103)
```
Máquinas descobrindo uma à outra.

## 🐛 Depuração

### Se não vê DISCOVER/HELLO
- Verifique firewall (bloqueando UDP porta 6000)
- Pode estar em rede local apenas
- Tente no localhost (mesma máquina, antes de debug)

### Se token não circula
- Verifique se máquina A iniciou (A gera primeiro token)
- Veja logs se há erro na porta 6001
- Tente `t` para ver status

### Se mensagem não chega
- Use `r` para verificar anel
- Destino pode não existir
- CRC pode estar diferente (verifique logs)

### Se porta já em uso
No code:
```c
#define DISCOVER_PORT 6000
#define DATA_PORT 6001
```
Mude para portas diferentes se necessário.

## 📊 Esperado por Máquina

### Máquina A (primeira a iniciar)
- Gera token inicial
- Monitora token (detecta perda/duplicação)
- Recebe dados para ela ou encaminha

### Máquina B (segunda)
- Recebe HELLO de A
- Entra no anel como (A → B)
- Recebe token de A quando vazio

### Máquina C (terceira)
- Recebe HELLO de A e B
- Anel se torna (A → B → C)
- Completa anél circular

### Máquina D (opcional)
- Se adicionar depois
- Anel fica (A → B → C → D)
- Quando token volta a A

## ⏱️ Timing

- Token chega em cada máquina a cada N segundos
- N = número de máquinas × token_time
- Com 3 máquinas e token_time=2: ~6s por volta
- Timeout padrão: 2,5s (para detectar perda)

## 🎯 Checklist de Validação

- [ ] 3 máquinas iniciadas com sucesso
- [ ] Todas conseguem sair da fila de descoberta
- [ ] Anel formado em ordem alfabética
- [ ] Token circula continuamente
- [ ] Mensagem simples funciona (A→B)
- [ ] Mesagem circula até origem
- [ ] CRC calcula corretamente
- [ ] 4ª máquina pode se juntar
- [ ] Sistema continua funcionando após entrada de nova máquina

## 📞 Contato

Para problemas, verifique:
1. Logs de cada máquina
2. Status do token (`t`)
3. Topologia (`r`)
4. Mensagens na fila (try `s X test`)
