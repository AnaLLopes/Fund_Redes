/**
 * Representa uma máquina (nó) identificada na rede em anel.
 * Armazena o apelido e o endereço IP da máquina.
 */
public class MachineInfo {
    public final String nickname;
    public final String ip;

    public MachineInfo(String nickname, String ip) {
        this.nickname = nickname.trim();
        this.ip       = ip.trim();
    }

    @Override
    public String toString() { return nickname + "@" + ip; }

    @Override
    public boolean equals(Object o) {
        return o instanceof MachineInfo && nickname.equals(((MachineInfo) o).nickname);
    }

    @Override
    public int hashCode() { return nickname.hashCode(); }
}
