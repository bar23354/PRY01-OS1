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

$adapter = Get-NetAdapter -Name $AdapterAlias -ErrorAction SilentlyContinue
if (-not $adapter) {
    $available = Get-NetAdapter -Physical | Sort-Object Name |
        Select-Object -ExpandProperty Name
    $availableText = if ($available) {
        $available -join ", "
    }
    else {
        "(sin adaptadores fisicos detectados)"
    }

    throw "No existe un adaptador llamado '$AdapterAlias'. Disponibles: $availableText"
}

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

Start-Sleep -Milliseconds 500

$current = Get-NetIPAddress -InterfaceAlias $AdapterAlias -AddressFamily IPv4 `
    -ErrorAction SilentlyContinue |
    Where-Object { $_.IPAddress -eq $IpAddress -and $_.PrefixLength -eq $PrefixLength }

if (-not $current) {
    New-NetIPAddress -InterfaceAlias $AdapterAlias `
        -IPAddress $IpAddress `
        -PrefixLength $PrefixLength `
        -AddressFamily IPv4 `
        -ErrorAction Stop | Out-Null
}

Write-Host "IP estatica configurada en $AdapterAlias -> $IpAddress/$PrefixLength"
