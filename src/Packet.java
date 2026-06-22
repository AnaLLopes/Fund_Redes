/**
 * Define o formato de todos os pacotes do protocolo Token Ring UDP.
 *
 * Tipos de pacotes:
 *   DISCOVER  →  "10:<apelido>:<ip>"        (broadcast na porta 6000)
 *   HELLO     →  "20:<apelido>:<ip>"        (broadcast na porta 6000)
 *   TOKEN     →  "1000"                     (unicast na porta 6000)
 *   DATA      →  "2000:<origem>:<destino>:<controle>:<crc>:<mensagem>"  (porta 6000)
 *
 * Valores do campo controle em pacotes DATA:
 *   "maquinainexistente"  → origem envia, destino não foi encontrado no anel
 *   "ACK"                 → destino recebeu sem erro
 *   "NAK"                 → destino detectou erro de CRC
 */
public class Packet {

    // ─── Tipos ────────────────────────────────────────────────────────
    public static final int TYPE_DISCOVER = 10;
    public static final int TYPE_HELLO    = 20;
    public static final int TYPE_TOKEN    = 1000;
    public static final int TYPE_DATA     = 2000;

    // ─── Valores do campo controle ────────────────────────────────────
    public static final String CTRL_INEXISTENTE = "maquinainexistente";
    public static final String CTRL_ACK         = "ACK";
    public static final String CTRL_NAK         = "NAK";

    // ─── Fábricas ─────────────────────────────────────────────────────

    public static String discover(String nick, String ip) {
        return "10:" + nick + ":" + ip;
    }

    public static String hello(String nick, String ip) {
        return "20:" + nick + ":" + ip;
    }

    public static String token() {
        return "1000";
    }

    /** Cria string de pacote DATA conforme especificação. */
    public static String data(String origin, String dest, String ctrl, long crc, String msg) {
        return "2000:" + origin + ":" + dest + ":" + ctrl + ":" + crc + ":" + msg;
    }

    // ─── Identificação ────────────────────────────────────────────────

    /**
     * Extrai o tipo numérico de um pacote bruto.
     * Retorna -1 se o formato for inválido.
     */
    public static int getType(String raw) {
        if (raw == null || raw.isEmpty()) return -1;
        String prefix = raw.contains(":") ? raw.substring(0, raw.indexOf(':')) : raw.trim();
        try { return Integer.parseInt(prefix.trim()); }
        catch (NumberFormatException e) { return -1; }
    }

    // ─── Parsers ──────────────────────────────────────────────────────

    /**
     * Faz parse de pacotes DISCOVER ou HELLO.
     * Formato: "10:nick:ip" ou "20:nick:ip"
     */
    public static MachineInfo parseDiscovery(String raw) {
        String[] p = raw.split(":", 3);
        return (p.length >= 3) ? new MachineInfo(p[1], p[2]) : null;
    }

    /**
     * Faz parse de um pacote DATA.
     * Formato: "2000:origem:destino:controle:crc:mensagem"
     */
    public static DataPkt parseData(String raw) {
        // Divide em no máximo 6 partes (mensagem pode conter ':')
        String[] p = raw.split(":", 6);
        if (p.length < 6) return null;
        DataPkt dp = new DataPkt();
        dp.origin      = p[1].trim();
        dp.destination = p[2].trim();
        dp.control     = p[3].trim();
        try { dp.crc = Long.parseLong(p[4].trim()); }
        catch (NumberFormatException e) { return null; }
        dp.message = p[5];   // Preserva a mensagem integralmente (pode ter ':')
        return dp;
    }

    // ─── Classe auxiliar para pacotes DATA ────────────────────────────

    public static class DataPkt {
        public String origin;
        public String destination;
        public String control;
        public String message;
        public long   crc;

        /** Serializa de volta para o formato de transmissão. */
        public String serialize() {
            return "2000:" + origin + ":" + destination + ":" + control + ":" + crc + ":" + message;
        }

        @Override
        public String toString() {
            return "DATA[" + origin + "→" + destination + " | " + control
                 + " | crc=" + crc + " | \"" + message + "\"]";
        }
    }
}
