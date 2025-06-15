using System.Text;
using System.Text.Json;
using Fido2NetLib;

namespace vicr123_accounts_fido.Commands;

public static class RegisterCommand
{
    public static async Task Invoke(string rpName, string rpId, string userId)
    {
        var json = await JsonDocument.ParseAsync(Console.OpenStandardInput());
        
        var preregisterOptions =
            json.RootElement.GetProperty("preregisterOptions").Deserialize<CredentialCreateOptions>();
        var response = json.RootElement.GetProperty("response").Deserialize<AuthenticatorAttestationRawResponse>();
        var expectOrigins = json.RootElement.GetProperty("expectOrigins");
        
        var fido2 = new Fido2(new Fido2Configuration
        {
            ServerDomain = rpId,
            ServerName = rpName,
            Origins = expectOrigins.EnumerateArray().Select(x => x.GetString()).ToHashSet()
        });
        
        //TODO: Ensure credential is unique to this user
        var cred = await fido2.MakeNewCredentialAsync(response, preregisterOptions,
            (args, cancellationToken) => Task.FromResult(true));
        
        //Store in database
        var key = new SecurityKey
        {
            UserId = ulong.Parse(userId),
            CredentialId = cred.Result!.CredentialId,
            UserHandle = cred.Result.User.Id,
            PublicKey = cred.Result!.PublicKey,
            Counter = cred.Result.Counter,
            CredType = cred.Result.CredType,
            RegistrationDate = DateTime.UtcNow,
            AaGuid = cred.Result.Aaguid
        };

        await Console.OpenStandardOutput().WriteAsync(JsonSerializer.SerializeToUtf8Bytes(key));
    }
}