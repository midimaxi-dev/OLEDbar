<#
.SYNOPSIS
    Signs one file with Microsoft Artifact Signing and verifies the result.
.DESCRIPTION
    Used as the Inno Setup sign tool by package.ps1 -Sign, so Setup and the uninstaller it writes are signed.
    Requires the ArtifactSigning PowerShell module (installed by azure/artifact-signing-action), an Azure CLI
    sign-in, and the SIGNING_ENDPOINT, SIGNING_ACCOUNT and SIGNING_PROFILE environment variables.
#>
param(
    [Parameter(Mandatory)][string]$Path
)
$ErrorActionPreference = 'Stop'

foreach ($name in 'SIGNING_ENDPOINT', 'SIGNING_ACCOUNT', 'SIGNING_PROFILE') {
    if (-not [Environment]::GetEnvironmentVariable($name)) { throw "$name is not set" }
}
Import-Module ArtifactSigning

$params = @{
    Endpoint                            = $env:SIGNING_ENDPOINT
    CodeSigningAccountName              = $env:SIGNING_ACCOUNT
    CertificateProfileName              = $env:SIGNING_PROFILE
    Files                               = $Path
    FileDigest                          = 'SHA256'
    TimestampRfc3161                    = 'http://timestamp.acs.microsoft.com'
    TimestampDigest                     = 'SHA256'
    ExcludeEnvironmentCredential        = $true
    ExcludeWorkloadIdentityCredential   = $true
    ExcludeManagedIdentityCredential    = $true
    ExcludeSharedTokenCacheCredential   = $true
    ExcludeVisualStudioCredential       = $true
    ExcludeVisualStudioCodeCredential   = $true
    ExcludeAzureCliCredential           = $false
    ExcludeAzurePowerShellCredential    = $true
    ExcludeAzureDeveloperCliCredential  = $true
    ExcludeInteractiveBrowserCredential = $true
}
Invoke-ArtifactSigning @params

$signature = Get-AuthenticodeSignature -LiteralPath $Path
if ($signature.Status -ne 'Valid' -or -not $signature.TimeStamperCertificate) {
    throw "Signing failed for $Path ($($signature.Status)): $($signature.StatusMessage)"
}
Write-Host "Signed $Path"
