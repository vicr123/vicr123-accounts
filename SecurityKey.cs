namespace vicr123_accounts_fido;

public class SecurityKey
{
    public string Name { get; set; }
    public ulong UserId { get; set; }
    public byte[] PublicKey { get; set; }
    public uint Counter { get; set; }
    public string CredType { get; set; }
    public DateTime RegistrationDate { get; set; }
    public Guid AaGuid { get; set; }
    public byte[] CredentialId { get; set; }
    public byte[] UserHandle { get; set; }
}