import java.io.*;

/**
 * Lê e armazena as configurações do arquivo de inicialização.
 * Formato esperado:
 *   <apelido>
 *   <tempo_token_e_dados>
 *   <probabilidade_erro>
 *   <timeout_token>
 *   <tempo_minimo_entre_tokens>
 */
public class Config {
    public String nickname;           // Apelido desta máquina (A, B, C, ...)
    public int    tokenDataTime;      // Delay em segundos ao receber token/dados
    public int    errorProbability;   // Probabilidade de inserir erro (0-100%)
    public double tokenTimeout;       // Segundos até considerar token perdido
    public int    minTimeBetweenTokens; // Segundos mínimos entre passagens do token

    public Config(String nickname, int tokenDataTime, int errorProbability, double tokenTimeout, int minTimeBetweenTokens) {
        this.nickname = nickname;
        this.tokenDataTime = tokenDataTime;
        this.errorProbability = errorProbability;
        this.tokenTimeout = tokenTimeout;
        this.minTimeBetweenTokens = minTimeBetweenTokens;
    }

    public Config(){}

    public static Config readFromFile(String filename) throws IOException {
        try (BufferedReader br = new BufferedReader(new FileReader(filename))) {
            Config c = new Config();
            c.nickname               = br.readLine().trim();
            c.tokenDataTime          = Integer.parseInt(br.readLine().trim());
            c.errorProbability       = Integer.parseInt(br.readLine().trim());
            c.tokenTimeout           = Double.parseDouble(br.readLine().trim().replace(',', '.'));
            c.minTimeBetweenTokens   = Integer.parseInt(br.readLine().trim());
            return c;
        }
    }

    @Override
    public String toString() {
        return String.format(
            "[apelido=%s delay=%ds errProb=%d%% timeout=%.1fs minEntreTokens=%ds]",
            nickname, tokenDataTime, errorProbability, tokenTimeout, minTimeBetweenTokens
        );
    }
}
