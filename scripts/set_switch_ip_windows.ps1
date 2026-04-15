param(
    [string]$AdapterAlias = "Ethernet",
    [string]$IpAddress = "192.168.50.10",
    [int]$PrefixLength = 24
)

$isAdmin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator
    )

if (-not $isAdmin) {
    throw "Ejecuta este script en PowerShell como Administrador."
}

$adapter = Get-NetAdapter -Name $AdapterAlias -ErrorAction Stop
if ($adapter.Status -ne "Up") {
    Write-Warning "El adaptador '$AdapterAlias' no esta en estado Up. Estado actual: $($adapter.Status)"
}

$existing = Get-NetIPAddress -InterfaceAlias $AdapterAlias -AddressFamily IPv4 `
    -ErrorAction SilentlyContinue

foreach ($entry in $existing) {
    if ($entry.PrefixOrigin -ne "WellKnown") {
        Remove-NetIPAddress -InterfaceAlias $AdapterAlias `
            -IPAddress $entry.IPAddress `
            -Confirm:$false `
            -ErrorAction SilentlyContinue
    }
}

New-NetIPAddress -InterfaceAlias $AdapterAlias `
    -IPAddress $IpAddress `
    -PrefixLength $PrefixLength `
    -AddressFamily IPv4 `
    -ErrorAction Stop | Out-Null

Write-Host "IP estatica configurada en $AdapterAlias -> $IpAddress/$PrefixLength"
