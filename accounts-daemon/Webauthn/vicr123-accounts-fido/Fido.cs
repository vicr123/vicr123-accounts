using System.Text;
using System.Text.Json;
using Fido2NetLib;
using Fido2NetLib.Objects;
using tdotnetbridge.ClientLibrary;

namespace vicr123_accounts_fido;

[QObject]
public class Fido
{
    [ExportToQt]
    public Fido()
    {
        
    }
    
    [ExportToQt]
    public async Task<byte[]> Get(string rpName, string rpId, string inPayload)
    {
        var json = JsonDocument.Parse(inPayload);

        var pregetOptions =
            JsonSerializer.Deserialize<AssertionOptions>(json.RootElement.GetProperty("pregetOptions").ToString());
        var response =
            JsonSerializer.Deserialize<AuthenticatorAssertionRawResponse>(json.RootElement.GetProperty("response")
                .ToString());
        var expectOrigins = json.RootElement.GetProperty("expectOrigins");
        var existingCreds =
            json.RootElement.GetProperty("existingCreds").Deserialize<IList<SecurityKey>>();
        
        var fido2 = new Fido2(new Fido2Configuration
        {
            ServerDomain = rpId,
            ServerName = rpName,
            Origins = expectOrigins.EnumerateArray().Select(x => x.GetString()).ToHashSet()
        });

        var cred = existingCreds.Single(x => x.CredentialId.AsSpan().SequenceEqual(response.Id));

        var result = await fido2.MakeAssertionAsync(response, pregetOptions, cred.PublicKey, cred.Counter,
            (args, cancellationToken) =>
            {
                var creds = existingCreds.Where(x => x.UserHandle.AsSpan().SequenceEqual(args.UserHandle));
                return Task.FromResult(creds.Any(x => x.CredentialId.AsSpan().SequenceEqual(args.CredentialId)));
            });

        var newCred = existingCreds.Single(x => x.CredentialId.AsSpan().SequenceEqual(response.Id));
        newCred.Counter = result.Counter;

        var payload = new
        {
            UsedCred = JsonSerializer.SerializeToUtf8Bytes(cred),
            NewCred = JsonSerializer.SerializeToUtf8Bytes(newCred)
        };
        
        return JsonSerializer.SerializeToUtf8Bytes(payload);
    }
    
    [ExportToQt]
    public async Task<string> PreGet(string rpName, string rpId, string payload)
    {
        var json = JsonDocument.Parse(payload);
        
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
        return optionsJson;
    }
    
    [ExportToQt]
    public async Task<string> PreRegisterPasskey(string rpName, string rpId, string username, string userId, string authenticatorAttachment, string payload)
    {
        var json = JsonDocument.Parse(payload);

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
                _ => null
            }
        }, AttestationConveyancePreference.None, new AuthenticationExtensionsClientInputs
        {
            Extensions = true,
            UserVerificationMethod = true
        });

        var optionsJson = options.ToJson();
        return optionsJson;
    }
    
    [ExportToQt]
    public async Task<SecurityKey> RegisterPasskey(string rpName, string rpId, string userId, string preregisterOutput)
    {
        var json = JsonDocument.Parse(preregisterOutput);
        
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

        return key;
    }
}