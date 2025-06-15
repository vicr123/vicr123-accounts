using System.CommandLine;
using vicr123_accounts_fido.Commands;

var rootCommand = new RootCommand("FIDO U2F support for vicr123-accounts");

var rpNameOption = new Option<string>("--rpname", "Relying Party Name")
{
    IsRequired = true
};
var rpIdOption = new Option<string>("--rpid", "Relying Party ID")
{
    IsRequired = true
};
var usernameOption = new Option<string>("--username", "Username")
{
    IsRequired = true
};
var userIdOption = new Option<string>("--userid", "User ID")
{
    IsRequired = true
};
var authenticatorAttachmentTypeOption = new Option<string>("--authattachment", () => "", "Authenticator Attachment");

var preregisterCommand = new Command("preregister", "Generate information to register a new FIDO U2F Key")
{
    rpNameOption,
    rpIdOption,
    usernameOption,
    userIdOption,
    authenticatorAttachmentTypeOption
};
preregisterCommand.SetHandler(PreregisterCommand.Invoke, rpNameOption, rpIdOption, usernameOption, userIdOption, authenticatorAttachmentTypeOption);
rootCommand.AddCommand(preregisterCommand);

var registerCommand = new Command("register", "Register a new FIDO U2F Key")
{
    rpNameOption,
    rpIdOption,
    userIdOption
};
registerCommand.SetHandler(RegisterCommand.Invoke, rpNameOption, rpIdOption, userIdOption);
rootCommand.AddCommand(registerCommand);

var preGetCommand = new Command("preget", "Generate information to authenticate an existing FIDO U2F Key")
{
    rpNameOption,
    rpIdOption
};
preGetCommand.SetHandler(PregetCommand.Invoke, rpNameOption, rpIdOption);
rootCommand.AddCommand(preGetCommand);

var getCommand = new Command("get", "Authenticate an existing FIDO U2F Key")
{
    rpNameOption,
    rpIdOption
};
getCommand.SetHandler(GetCommand.Invoke, rpNameOption, rpIdOption);
rootCommand.AddCommand(getCommand);

return await rootCommand.InvokeAsync(args);