using System.Text;
using System.Text.Json;
using tdotnetbridge.ClientLibrary;

namespace vicr123_accounts_fido;

[QObject]
public class SecurityKey
{
    [ExportToQt]
    public string Name { get; set; }
    
    [ExportToQt]
    public ulong UserId { get; set; }
    
    [ExportToQt]
    public byte[] PublicKey { get; set; }
    
    [ExportToQt]
    public uint Counter { get; set; }
    
    [ExportToQt]
    public string CredType { get; set; }
    
    public DateTime RegistrationDate { get; set; }
    
    public Guid AaGuid { get; set; }
    
    [ExportToQt]
    public byte[] CredentialId { get; set; }
    
    [ExportToQt]
    public byte[] UserHandle { get; set; }

    [ExportToQt]
    public string ToJson()
    {
        return Encoding.UTF8.GetString(JsonSerializer.SerializeToUtf8Bytes(this));
    }
}