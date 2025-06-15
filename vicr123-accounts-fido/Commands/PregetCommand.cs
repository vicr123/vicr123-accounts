using System.Buffers.Text;
using System.Collections;
using System.Text.Json;
using Fido2NetLib;
using Fido2NetLib.Objects;

namespace vicr123_accounts_fido.Commands;

public static class PregetCommand
{
    public static async Task Invoke(string rpName, string rpId)
    {
        var json = await JsonDocument.ParseAsync(Console.OpenStandardInput());
        
        var existingCreds =
            json.RootElement.GetProperty("existingCreds").Deserialize<IList<string>>().Select(x => JsonSerializer.Deserialize<SecurityKey>(Convert.FromBase64String(x))).ToList();
        
        var fido2 = new Fido2(new Fido2Configuration
        {
            ServerDomain = rpId,
            ServerName = rpName
        });

        var options =
            fido2.GetAssertionOptions(existingCreds.Select(x => new PublicKeyCredentialDescriptor(x.CredentialId)), UserVerificationRequirement.Preferred, new AuthenticationExtensionsClientInputs()
            {
                UserVerificationMethod = true
            });

        var optionsJson = options.ToJson();
        await Console.Out.WriteAsync(optionsJson);
    }
}