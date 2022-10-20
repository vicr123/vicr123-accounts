using System.Text;
using System.Text.Json;
using Fido2NetLib;
using Fido2NetLib.Objects;

namespace vicr123_accounts_fido.Commands;

public static class PreregisterCommand
{
    public static async Task Invoke(string rpName, string rpId, string username, string userId, string authenticatorAttachment)
    {
        var json = await JsonDocument.ParseAsync(Console.OpenStandardInput());

        var existingCreds =
            json.RootElement.GetProperty("existingCreds").Deserialize<IList<string>>().Select(x => JsonSerializer.Deserialize<SecurityKey>(Convert.FromBase64String(x))).ToList();
        
        var fido2 = new Fido2(new Fido2Configuration
        {
            ServerDomain = rpId,
            ServerName = rpName
        });
        

        var options = fido2.RequestNewCredential(new Fido2User
        {
            DisplayName = username,
            Id = Encoding.UTF8.GetBytes(userId),
            Name = username
        }, existingCreds.Select(x => new PublicKeyCredentialDescriptor(x.CredentialId)).ToList(), new AuthenticatorSelection
        {
            RequireResidentKey = true,
            UserVerification = UserVerificationRequirement.Preferred,
            AuthenticatorAttachment = authenticatorAttachment switch
            {
                "platform" => AuthenticatorAttachment.Platform,
                "cross-platform" => AuthenticatorAttachment.CrossPlatform,
                _ => throw new ArgumentException()
            }
        }, AttestationConveyancePreference.None, new AuthenticationExtensionsClientInputs
        {
            Extensions = true,
            UserVerificationMethod = true
        });

        var optionsJson = options.ToJson();
        await Console.Out.WriteAsync(optionsJson);
    }
}