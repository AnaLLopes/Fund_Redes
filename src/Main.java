/**
 * Ponto de entrada da aplicação Token Ring UDP.
 *
 * Uso:
 *   java Main [arquivo_config]
 *
 * Se nenhum arquivo for informado, usa "config.txt" no diretório atual.
 *
 * Formato do arquivo de configuração:
 *   <apelido>
 *   <tempo_token_e_dados>
 *   <probabilidade_erro>
 *   <timeout_token>
 *   <tempo_minimo_entre_tokens>
 *
 * Exemplo (config_B.txt):
 *   B
 *   2
 *   20
 *   2,5
 *   2
 */
public class Main {
    public static void main(String[] args) {
        String configFile = (args.length > 0) ? args[0] : "config.txt";

        System.out.println("                                            ");
        System.out.println("        REDE EM ANEL — Token Ring UDP       ");
        System.out.println("     Redes de Computadores — UFRGS/PUCRS    ");
        System.out.println("                                            ");
        System.out.println("Arquivo de configuração: " + configFile);

        try {
            Config cfg = Config.readFromFile(configFile);
            System.out.println("Configuração: " + cfg);
            new RingNode(cfg).start();
        } catch (java.io.FileNotFoundException e) {
            System.err.println("ERRO: Arquivo de configuração não encontrado: " + configFile);
            printUsage();
        } catch (Exception e) {
            System.err.println("ERRO FATAL: " + e.getMessage());
            e.printStackTrace();
        }
    }

    static void printUsage() {
        System.out.println("\nUso: java Main <arquivo_config>");
        System.out.println("Exemplo: java Main config_A.txt");
        System.out.println("\nFormato do arquivo de configuração:");
        System.out.println("  <apelido>");
        System.out.println("  <tempo_token_e_dados>");
        System.out.println("  <probabilidade_erro>");
        System.out.println("  <timeout_token>");
        System.out.println("  <tempo_minimo_entre_tokens>");
    }
}
