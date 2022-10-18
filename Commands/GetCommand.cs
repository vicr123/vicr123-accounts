using System.Text.Json;
using Fido2NetLib;

namespace vicr123_accounts_fido.Commands;

public static class GetCommand
{
    public static async Task Invoke(string rpName, string rpId)
    {
        var json = await JsonDocument.ParseAsync(Console.OpenStandardInput());

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
        
        await Console.OpenStandardOutput().WriteAsync(JsonSerializer.SerializeToUtf8Bytes(payload));
    }
}