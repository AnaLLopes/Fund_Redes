import java.net.*;
import java.util.*;
import java.util.concurrent.atomic.*;

/**
 * Nó principal da rede em anel Token Ring UDP.
 *
 * Responsabilidades:
 *   1. Descoberta de máquinas via DISCOVER/HELLO (broadcast na porta 6000).
 *   2. Construção e reconfiguração da topologia do anel (ordem alfabética).
 *   3. Circulação do token e transmissão de dados (porta 5000).
 *   4. Detecção de token perdido (timeout) e token duplicado (tempo mínimo).
 *   5. Controle de erro via CRC32 e módulo de inserção de falhas.
 *   6. Interface de usuário via linha de comando.
 *
 * Threads:
 *   - DiscListener  : escuta DISCOVER/HELLO na porta 6000 (tempo todo).
 *   - RingListener  : escuta TOKEN/DATA na porta 5000 (tempo todo).
 *   - TokenMonitor  : monitora timeout do token (somente na máquina controladora).
 *   - Thread main   : loop de entrada do usuário.
 */
public class RingNode {

    //                         Constantes                                
    static final int    DISC_PORT         = 6000;
    static final int    RING_PORT         = 5000;
    static final int    DISC_DURATION_MS  = 5000;  // Tempo de descoberta inicial (ms)
    static final String BROADCAST_DEST    = "BROADCAST";

    //Estado da máquina                  
    final Config cfg;
    String       myIP;

    // ─── Topologia ────────────────────────────────────────────────────
    final List<MachineInfo> ring = new ArrayList<>();
    volatile MachineInfo successor;
    volatile boolean     amController = false; // Esta máquina gera/controla o token?

    // ─── Sockets ──────────────────────────────────────────────────────
    DatagramSocket discSock;  // Porta 6000 — descoberta
    DatagramSocket ringSock;  // Porta 5000 — anel (token + dados)

    // ─── Fila e módulos ───────────────────────────────────────────────
    final MessageQueue  mq     = new MessageQueue();
    final ErrorInserter errInj;

    // ─── Controle do token ────────────────────────────────────────────
    final AtomicLong    lastTokenAt = new AtomicLong(0); // Timestamp da última passagem
    final AtomicBoolean dropToken   = new AtomicBoolean(false); // Remover próximo token?
    final AtomicBoolean acceptRecoveredToken = new AtomicBoolean(false); // Ignorar a próxima checagem de duplicidade após timeout
    volatile boolean    sentData    = false; // Aguardando retorno de pacote de dados?

    // ─── Ciclo de vida ────────────────────────────────────────────────
    volatile boolean running   = true;
    volatile boolean discPhase = true; // Ainda na fase de descoberta?

    //                                                                   

    public RingNode(Config cfg) {
        this.cfg    = cfg;
        this.errInj = new ErrorInserter(cfg.errorProbability);
    }

    //                         INICIALIZAÇÃO                             

    public void start() throws Exception {
        myIP = detectIP();
        log("Iniciando nó '" + cfg.nickname + "' em " + myIP + " | " + cfg);

        // Abre socket de descoberta (porta 6000, broadcast)
        discSock = new DatagramSocket(DISC_PORT);
        discSock.setBroadcast(true);
        discSock.setSoTimeout(300);

        // // Abre socket do anel (porta 5000)
        // ringSock = new DatagramSocket(RING_PORT);
        // ringSock.setSoTimeout(300);

        // Registra a si mesmo no anel
        ring.add(new MachineInfo(cfg.nickname, myIP));

        doDiscovery();  // Fase 1 — Descoberta

        discSock.close(); // Não precisa mais do socket de descoberta

        // Abre socket do anel (porta 5000)
        ringSock = new DatagramSocket(RING_PORT);
        ringSock.setSoTimeout(300);

        buildRing();    // Fase 2 — Construção do anel
        operate();      // Fase 3 — Operação normal
    }

    //                        FASE DE DESCOBERTA                         

    void doDiscovery() throws Exception {
        log("   FASE DE DESCOBERTA (" + DISC_DURATION_MS / 1000 + "s)           ");

        // Thread de escuta de DISCOVER/HELLO roda durante toda a execução
        Thread dl = new Thread(this::discListener, "DiscListener");
        dl.setDaemon(true);
        dl.start();

        // Envia DISCOVER a cada segundo durante DISC_DURATION_MS
        long end = System.currentTimeMillis() + DISC_DURATION_MS;
        while (System.currentTimeMillis() < end) {
            bcastDiscover();
            Thread.sleep(1000);
        }

        discPhase = false;
        dl.interrupt(); // Encerra a thread de escuta de descoberta
        log("   Descoberta concluída — " + ring.size() + " máquina(s) encontrada(s)    ");
    }

    void bcastDiscover() throws Exception {
        String pkt = Packet.discover(cfg.nickname, myIP);
        broadcast(pkt, DISC_PORT);
        log("[DISCOVER  ->] " + pkt);
    }

    void bcastHello() throws Exception {
        String pkt = Packet.hello(cfg.nickname, myIP);
        broadcast(pkt, DISC_PORT);
        log("[HELLO  ->] " + pkt);
    }

    /** Thread que escuta pacotes de descoberta na porta 6000. */
    void discListener() {
        byte[] buf = new byte[512];
        while (running) {
            try {
                DatagramPacket dp = new DatagramPacket(buf, buf.length);
                discSock.receive(dp);
                String raw = new String(dp.getData(), 0, dp.getLength()).trim();
                onDiscPacket(raw);
            } catch (SocketTimeoutException ignored) {
            } catch (SocketException e) {
                if (running && discSock != null && !discSock.isClosed()) {
                    log("[DISC] Erro na escuta: " + e.getMessage());
                }
                break;
            } catch (Exception e) {
                if (running) log("[DISC] Erro na escuta: " + e.getMessage());
            }
        }
    }

    /** Processa pacote DISCOVER ou HELLO recebido. */
    synchronized void onDiscPacket(String raw) throws Exception {
        int type = Packet.getType(raw);
        if (type != Packet.TYPE_DISCOVER && type != Packet.TYPE_HELLO) return;

        MachineInfo mi = Packet.parseDiscovery(raw);
        // Ignora mensagens próprias
        if (mi == null || mi.nickname.equals(cfg.nickname)) return;

        if (type == Packet.TYPE_DISCOVER) {
            log("[DISCOVER  <-] " + mi + "  -> respondendo com HELLO");
            bcastHello();
            boolean added = addToRing(mi);
            if (added && !discPhase) {
                log("[ANEL] Nova máquina detectada durante operação  -> reconfigurando...");
                buildRing();
            }
        } else { // HELLO
            log("[HELLO  <-] " + mi);
            boolean added = addToRing(mi);
            if (added && !discPhase) {
                log("[ANEL] Nova máquina detectada durante operação  -> reconfigurando...");
                buildRing();
            }
        }
    }

    /**
     * Adiciona máquina à lista do anel se ainda não estiver lá.
     * @return true se foi de fato adicionada.
     */
    boolean addToRing(MachineInfo mi) {
        for (MachineInfo m : ring)
            if (m.nickname.equals(mi.nickname)) return false;
        ring.add(mi);
        return true;
    }

    //                         TOPOLOGIA DO ANEL                         

    /**
     * Ordena as máquinas alfabeticamente, determina o sucessor e
     * decide se esta máquina é a controladora do token.
     * Imprime a topologia resultante.
     */
    void buildRing() {
        // Ordena por apelido (ordem alfabética define a direção do anel)
        ring.sort(Comparator.comparing(m -> m.nickname));

        int myIdx = -1;
        for (int i = 0; i < ring.size(); i++)
            if (ring.get(i).nickname.equals(cfg.nickname)) { myIdx = i; break; }

        if (myIdx < 0) { log("[ANEL] ERRO: minha entrada não encontrada no anel!"); return; }

        // Sucessor circular: último conecta ao primeiro
        successor    = ring.get((myIdx + 1) % ring.size());
        amController = (myIdx == 0); // Primeira máquina (A) controla o token

        // Impressão da topologia
        StringBuilder sb = new StringBuilder();
        sb.append("\n                                         ");
        sb.append("\n            TOPOLOGIA DO ANEL            ");
        sb.append("\n                                         \n   ");
        for (int i = 0; i < ring.size(); i++) {
            String n = ring.get(i).nickname;
            sb.append(n.equals(cfg.nickname) ? "[" + n + "]" : n);
            if (i < ring.size() - 1) sb.append("  -> ");
        }
        sb.append("  -> ").append(ring.get(0).nickname);
        sb.append("\n   Meu sucessor   : ").append(successor);
        sb.append("\n   Controlador    : ").append(ring.get(0).nickname);
        if (amController) sb.append("  <- (EU)");
        sb.append("\n                                         ");
        System.out.println(sb);
    }

    //                        OPERAÇÃO DO ANEL                           

    void operate() throws Exception {
        // Thread de escuta do anel (token + dados)
        Thread rl = new Thread(this::ringListener, "RingListener");
        rl.setDaemon(true);
        rl.start();

        // Somente a primeira máquina (controladora) gera o token e o monitora
        if (amController) {
            Thread tm = new Thread(this::tokenMonitor, "TokenMonitor");
            tm.setDaemon(true);
            tm.start();

            Thread startupToken = new Thread(() -> {
                try {
                    Thread.sleep(500); // Aguarda outros nós ficarem prontos
                    log("★★★ Gerando token inicial ★★★");
                    sendToken();
                } catch (InterruptedException ignored) {
                } catch (Exception e) {
                    log("[TOKEN] Erro ao gerar token inicial: " + e.getMessage());
                }
            }, "StartupToken");
            startupToken.setDaemon(true);
            startupToken.start();
        }

        // Thread principal vira o loop de entrada do usuário
        userInputLoop();
    }

    // ─── Escuta do anel ───────────────────────────────────────────────

    void ringListener() {
        byte[] buf = new byte[4096];
        while (running) {
            try {
                DatagramPacket dp = new DatagramPacket(buf, buf.length);
                ringSock.receive(dp);
                String raw = new String(dp.getData(), 0, dp.getLength()).trim();
                onRingPacket(raw);
            } catch (SocketTimeoutException ignored) {
            } catch (SocketException e) {
                if (running && ringSock != null && !ringSock.isClosed()) {
                    log("[RING] Erro na escuta: " + e.getMessage());
                }
                break;
            } catch (Exception e) {
                if (running) log("[RING] Erro na escuta: " + e.getMessage());
            }
        }
    }

    /** Despacha o pacote recebido no anel para o handler correto. */
    synchronized void onRingPacket(String raw) {
        try {
            int type = Packet.getType(raw);
            switch (type) {
                case Packet.TYPE_TOKEN: onToken();      break;
                case Packet.TYPE_DATA:  onData(raw);    break;
                default:
                    log("[RING] Pacote desconhecido ignorado: " + raw);
            }
        } catch (Exception e) {
            log("[RING] Erro ao processar pacote: " + e.getMessage());
            e.printStackTrace();
        }
    }

    // ─── Tratamento do TOKEN ──────────────────────────────────────────

    void onToken() throws Exception {
        long now = System.currentTimeMillis();

        // Caso especial: estava aguardando retorno de dados mas chegou outro token.
        // Isso indica que o pacote de dados foi perdido no anel (ex: máquina intermédia caiu).
        if (sentData) {
            log("⚠ [TOKEN] Token recebido com dados pendentes! Dados perdidos  -> descartando mensagem.");
            mq.dequeue();
            sentData = false;
        }

        // ── Controle do token (somente na controladora) ──────────────
        if (amController) {
            long elapsed = now - lastTokenAt.get();

            if (acceptRecoveredToken.getAndSet(false)) {
                lastTokenAt.set(now);
                log("[TOKEN] ◆ Recebido (token recuperado após timeout)");
            } else if (lastTokenAt.get() > 0 && elapsed < cfg.minTimeBetweenTokens * 1000L) {
                // Token duplicado? Chegou rápido demais.
                log("⚠ [TOKEN] TOKEN DUPLICADO detectado! elapsed=" + elapsed
                    + "ms < " + (cfg.minTimeBetweenTokens * 1000L) + "ms  -> REMOVENDO");
                return; // Não encaminha — descarta o token extra
            } else {
                lastTokenAt.set(now);
                log("[TOKEN] ◆ Recebido (elapsed=" + elapsed + "ms desde o último)");
            }
        } else {
            log("[TOKEN] ◆ Recebido");
        }

        // Remoção manual de token?
        if (dropToken.getAndSet(false)) {
            log("[TOKEN] Token removido manualmente pelo operador.");
            return;
        }

        // Delay configurado (simula tempo de processamento/transmissão)
        Thread.sleep(cfg.tokenDataTime * 1000L);

        // Tem mensagem na fila? Envia. Caso contrário, repassa o token.
        if (!mq.isEmpty()) {
            Message msg = mq.peek();
            log("[TOKEN] Tenho mensagem para enviar: " + msg);
            sendDataPkt(msg);
            sentData = true;
        } else {
            sendToken();
        }
    }

    // ─── Tratamento de DADOS ──────────────────────────────────────────

    void onData(String raw) throws Exception {
        Packet.DataPkt dp = Packet.parseData(raw);
        if (dp == null) {
            log("[DATA] Falha no parse do pacote: " + raw);
            return;
        }

        log("[DATA] ◆ " + dp);

        // Sou a origem? O pacote completou a volta no anel.
        if (dp.origin.equals(cfg.nickname)) {
            onDataReturn(dp);
            return;
        }

        boolean isBroadcast = BROADCAST_DEST.equals(dp.destination);
        boolean isForMe     = dp.destination.equals(cfg.nickname);

        // ── Sou o destino (unicast ou broadcast) ──────────────────────
        if (isForMe || isBroadcast) {
            boolean crcOk = CRCUtil.verify(dp.message, dp.crc);

            if (isBroadcast) {
                // Exibe a mensagem, mas mantém "maquinainexistente" conforme especificação.
                // O módulo de inserção de falhas não altera o controle para broadcast.
                printReceivedMsg(dp.origin, dp.message, crcOk, true);
                // dp.control permanece CTRL_INEXISTENTE
            } else {
                // Unicast: calcula ACK ou NAK com base no CRC
                if (crcOk) {
                    dp.control = Packet.CTRL_ACK;
                    printReceivedMsg(dp.origin, dp.message, true, false);
                } else {
                    dp.control = Packet.CTRL_NAK;
                    log("[DATA] ✗ CRC INVALIDO -> enviando NAK para " + dp.origin);
                }
            }
        }
        // Caso contrário: não é meu pacote — apenas encaminha sem alterar

        // Delay e encaminhamento ao sucessor
        Thread.sleep(cfg.tokenDataTime * 1000L);
        sendToSuccessor(dp.serialize());
    }

    /** Trata o retorno de um pacote de dados à máquina origem. */
    void onDataReturn(Packet.DataPkt dp) throws Exception {
        log("[DATA] ◆ Pacote retornou à origem (controle=" + dp.control + ")");

        // Retorno inesperado: não estávamos aguardando dados.
        // Isso pode acontecer se o token monitor gerou um novo token antes do retorno.
        // Não enviamos outro token pois já há um circulando.
        if (!sentData) {
            log("[DATA] Retorno inesperado — ignorando (token já circulando).");
            return;
        }
        sentData = false;

        switch (dp.control) {
            case Packet.CTRL_ACK:
                log("✓ [DATA] ACK — Mensagem entregue com sucesso a '" + dp.destination + "'.");
                mq.dequeue(); // Remove da fila: entregue
                break;

            case Packet.CTRL_NAK:
                log("✗ [DATA] NAK — Erro na entrega. Mensagem será retransmitida no próximo token.");
                // Não remove da fila: a mensagem será reenviada na próxima passagem do token
                break;

            case Packet.CTRL_INEXISTENTE:
                log("✗ [DATA] Destino '" + dp.destination + "' não encontrado na rede. Descartando mensagem.");
                mq.dequeue(); // Remove da fila: destino inexistente
                break;

            default:
                log("[DATA] Campo controle desconhecido: " + dp.control + "  -> descartando.");
                mq.dequeue();
        }

        // Libera o token para o próximo nó do anel
        Thread.sleep(cfg.tokenDataTime * 1000L);
        sendToken();
    }

    // ─── Monitor do Token ─────────────────────────────────────────────

    /**
     * Roda apenas na máquina controladora.
     * Detecta:
     *   - Token perdido (timeout): gera novo token.
     *   - Token duplicado (chegou rápido demais): tratado em onToken().
     */
    void tokenMonitor() {
        while (running) {
            try {
                Thread.sleep(200); // Polling a cada 200ms
                long now     = System.currentTimeMillis();
                long elapsed = now - lastTokenAt.get();

                if (lastTokenAt.get() > 0 && elapsed > cfg.tokenTimeout * 1000L) {
                    // Adquire lock para evitar gerar dois tokens ao mesmo tempo
                    synchronized (this) {
                        now     = System.currentTimeMillis();
                        elapsed = now - lastTokenAt.get();
                        if (elapsed > cfg.tokenTimeout * 1000L) {
                            log("⚠ [TOKEN MONITOR] TIMEOUT! " + elapsed + "ms sem token."
                                + " Token perdido  -> gerando novo token.");
                            lastTokenAt.set(now);
                            acceptRecoveredToken.set(true);
                            sentData = false; // Qualquer envio pendente é considerado perdido
                            sendToken();
                        }
                    }
                }
            } catch (InterruptedException e) {
                break;
            } catch (Exception e) {
                log("[TOKEN MONITOR] Erro: " + e.getMessage());
            }
        }
    }

    //                        ENVIO DE PACOTES                           

    void sendToken() throws Exception {
        log("[TOKEN]  -> Enviando token para " + successor);
        sendToSuccessor(Packet.token());
    }

    void sendDataPkt(Message msg) throws Exception {
        long   crc     = CRCUtil.calculate(msg.content); // CRC da mensagem original
        String body    = errInj.apply(msg.content);       // Possível corrupção
        String pktStr  = Packet.data(cfg.nickname, msg.destination,
                                     Packet.CTRL_INEXISTENTE, crc, body);
        log("[DATA]  -> Enviando para " + successor + " : " + pktStr);
        sendToSuccessor(pktStr);
    }

    void sendToSuccessor(String raw) throws Exception {
        byte[]        data = raw.getBytes();
        InetAddress   addr = InetAddress.getByName(successor.ip);
        DatagramPacket dp  = new DatagramPacket(data, data.length, addr, RING_PORT);
        ringSock.send(dp);
    }

    /** Envia broadcast UDP para 255.255.255.255 na porta indicada. */
    void broadcast(String msg, int port) throws Exception {
        byte[] data = msg.getBytes();
        try (DatagramSocket s = new DatagramSocket()) {
            s.setBroadcast(true);
            s.send(new DatagramPacket(data, data.length,
                   InetAddress.getByName("255.255.255.255"), port));
        }
    }

    //                      INTERFACE DO USUARIO                         

    void userInputLoop() {
        Scanner sc = new Scanner(System.in);
        printHelp();

        while (running) {
            System.out.print(cfg.nickname + "> ");
            System.out.flush();
            if (!sc.hasNextLine()) break;
            String line = sc.nextLine().trim();
            if (line.isEmpty()) continue;
            try { processCmd(line); }
            catch (Exception e) { log("Erro no comando: " + e.getMessage()); }
        }

        running = false;
        log("Encerrando nó " + cfg.nickname + ".");
    }

    synchronized void processCmd(String line) throws Exception {
        String[] parts = line.split(" ", 3);
        String   cmd   = parts[0].toLowerCase();

        switch (cmd) {

            // ── Enviar mensagem ──────────────────────────────────────
            case "msg":
                if (parts.length < 3) {
                    log("Uso: msg <DESTINO|BROADCAST> <mensagem>");
                    return;
                }
                String dest = parts[1].toUpperCase();
                if (mq.enqueue(dest, parts[2])) {
                    log("✓ Mensagem enfileirada para '" + dest + "': \"" + parts[2] + "\"");
                    log("  Fila atual: " + mq.status());
                } else {
                    log("✗ Fila cheia! (máximo " + MessageQueue.MAX_CAPACITY + " mensagens)");
                }
                break;

            // ── Injetar token ────────────────────────────────────────
            case "token+":
                log("[CMD] Injetando token manualmente na rede...");
                sendToken();
                break;

            // ── Remover token ────────────────────────────────────────
            case "token-":
                dropToken.set(true);
                log("[CMD] O próximo token que chegar será removido.");
                break;

            // ── Status ───────────────────────────────────────────────
            case "status":
                printStatus();
                break;

            // ── Fila ─────────────────────────────────────────────────
            case "fila":
                log("Fila: " + mq.status());
                break;

            // ── Topologia ────────────────────────────────────────────
            case "anel":
                buildRing();
                break;

            // ── Ajuda ────────────────────────────────────────────────
            case "help": case "?":
                printHelp();
                break;

            // ── Sair ─────────────────────────────────────────────────
            case "quit": case "exit":
                running = false;
                break;

            default:
                log("Comando desconhecido: '" + cmd + "'. Digite 'help' para ajuda.");
        }
    }

    //                         EXIBIÇÃO                                 

    void printHelp() {
        System.out.println();
        System.out.println("                                                    ");
        System.out.println("                    COMANDOS                        ");
        System.out.println("                                                    ");
        System.out.println("   msg <DESTINO> <mensagem>  Enviar mensagem        ");
        System.out.println("   msg BROADCAST <mensagem>  Enviar para todos      ");
        System.out.println("   token+                    Injetar token na rede  ");
        System.out.println("   token-                    Remover próximo token  ");
        System.out.println("   status                    Status desta máquina   ");
        System.out.println("   fila                      Ver fila de mensagens  ");
        System.out.println("   anel                      Ver topologia do anel  ");
        System.out.println("   quit                      Encerrar               ");
        System.out.println("                                                    ");
        System.out.println();
    }

    void printStatus() {
        System.out.println();
        System.out.println("                                          ");
        System.out.println("                STATUS                    ");
        System.out.println("                                          ");
        System.out.printf ("   Máquina      : %s (%s)%n", cfg.nickname, myIP);
        System.out.printf ("   Sucessor     : %s%n", successor);
        System.out.printf ("   Controladora : %s%n", amController ? "SIM (esta máquina)" : "NÃO");
        System.out.printf ("   Fila         : %s%n", mq.status());
        System.out.printf ("   Aguard. dados: %s%n", sentData ? "SIM" : "NÃO");
        System.out.printf ("   Rem. token   : %s%n", dropToken.get() ? "SIM" : "NÃO");
        System.out.println("                                          ");
        System.out.println();
    }

    void printReceivedMsg(String from, String message, boolean crcOk, boolean isBcast) {
        String type = isBcast ? "BROADCAST" : "MENSAGEM";
        System.out.println();
        System.out.println("                                              ");
        System.out.printf ("     📨 %s recebida%n", type);
        System.out.printf ("     De      : %s%n", from);
        System.out.printf ("     Mensagem: %s%n", message);
        System.out.printf ("     CRC     : %s%n", crcOk ? "OK ✓" :
                          "ERRO ✗" + (isBcast ? " (ignorado p/ broadcast)" : ""));
        System.out.println("                                              ");
        System.out.println();
    }

    //              UTILITARIOS                               

    /**
     * Detecta o endereço IPv4 não-loopback desta máquina.
     * Itera pelas interfaces de rede disponíveis.
     */
    String detectIP() {
        try {
            Enumeration<NetworkInterface> nifs = NetworkInterface.getNetworkInterfaces();
            while (nifs.hasMoreElements()) {
                NetworkInterface ni = nifs.nextElement();
                if (ni.isLoopback() || !ni.isUp()) continue;
                Enumeration<InetAddress> addrs = ni.getInetAddresses();
                while (addrs.hasMoreElements()) {
                    InetAddress a = addrs.nextElement();
                    if (a instanceof Inet4Address && !a.isLoopbackAddress())
                        return a.getHostAddress();
                }
            }
        } catch (Exception ignored) {}
        try { return InetAddress.getLocalHost().getHostAddress(); }
        catch (Exception e) { return "127.0.0.1"; }
    }

    void log(String msg) {
        System.out.println("[" + cfg.nickname + "] " + msg);
    }
}
