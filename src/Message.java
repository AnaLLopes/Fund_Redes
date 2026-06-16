/**
 * Representa uma mensagem na fila de envio desta máquina.
 * Armazena o apelido do destino e o conteúdo da mensagem.
 */
public class Message {
    public final String destination; // Apelido do destino ou "BROADCAST"
    public final String content;     // Conteúdo da mensagem

    public Message(String destination, String content) {
        this.destination = destination;
        this.content     = content;
    }

    @Override
    public String toString() {
        return "→" + destination + ": \"" + content + "\"";
    }
}
