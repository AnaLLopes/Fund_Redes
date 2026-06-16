import java.util.zip.CRC32;

/**
 * Utilitário para cálculo e verificação de CRC32.
 * Usado para detecção de erros nas mensagens de dados.
 *
 * Fluxo de uso:
 *   - Origem: calcula CRC sobre a mensagem original e inclui no pacote.
 *   - Destino: recalcula CRC sobre a mensagem recebida e compara com o do pacote.
 */
public class CRCUtil {

    /** Calcula o CRC32 de uma string e retorna como long. */
    public static long calculate(String data) {
        CRC32 crc32 = new CRC32();
        crc32.update(data.getBytes());
        return crc32.getValue();
    }

    /** Retorna true se o CRC recalculado bate com o esperado. */
    public static boolean verify(String data, long expected) {
        return calculate(data) == expected;
    }
}
