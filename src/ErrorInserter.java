import java.util.Random;

/**
 * Módulo de inserção de falhas.
 *
 * Antes de enviar, a máquina origem aplica este módulo sobre o conteúdo
 * da mensagem. Com probabilidade configurável, um byte aleatório é corrompido
 * (XOR com bit aleatório). O CRC no pacote permanece o original (calculado
 * sobre a mensagem sem erro), de modo que o receptor detecta a divergência
 * e responde com NAK.
 */
public class ErrorInserter {
    private final int    probability; // 0-100 (%)
    private final Random rng = new Random();

    public ErrorInserter(int probability) {
        this.probability = probability;
    }

    /**
     * Aplica (ou não) a corrupção na mensagem.
     * @param message Conteúdo original da mensagem.
     * @return Conteúdo possivelmente corrompido.
     */
    public String apply(String message) {
        if (rng.nextInt(100) < probability) {
            byte[] bytes = message.getBytes();
            if (bytes.length > 0) {
                int byteIdx = rng.nextInt(bytes.length);
                int bitIdx  = rng.nextInt(8);
                bytes[byteIdx] ^= (byte)(1 << bitIdx);
            }
            System.out.println("  ⚠  [ERRO INSERIDO] Bit corrompido artificialmente na mensagem!");
            return new String(bytes);
        }
        return message; // Sem corrupção
    }
}
