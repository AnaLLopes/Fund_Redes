import java.util.LinkedList;

/**
 * Fila de mensagens a serem enviadas por esta máquina.
 * Capacidade máxima: MAX_CAPACITY mensagens.
 * Thread-safe — todos os métodos são sincronizados.
 */
public class MessageQueue {
    public static final int MAX_CAPACITY = 10;

    private final LinkedList<Message> queue = new LinkedList<>();

    /** Adiciona mensagem à fila. Retorna false se a fila estiver cheia. */
    public synchronized boolean enqueue(String destination, String content) {
        if (queue.size() >= MAX_CAPACITY) return false;
        queue.addLast(new Message(destination, content));
        return true;
    }

    /** Consulta a primeira mensagem sem removê-la. */
    public synchronized Message peek() { return queue.peek(); }

    /** Remove e retorna a primeira mensagem. */
    public synchronized Message dequeue() { return queue.poll(); }

    public synchronized boolean isEmpty() { return queue.isEmpty(); }
    public synchronized int     size()    { return queue.size(); }

    /** Retorna uma string descritiva do estado da fila. */
    public synchronized String status() {
        if (queue.isEmpty()) return "vazia";
        StringBuilder sb = new StringBuilder(queue.size() + " mensagem(ns): ");
        for (Message m : queue) sb.append("[").append(m).append("] ");
        return sb.toString().trim();
    }
}
