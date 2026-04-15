param(
    [string]$Distro = "Ubuntu",
    [string]$ListenAddress = "192.168.50.10",
    [int]$Port = 8080,
    [string]$FirewallRuleName = "PRY01 Chat Server 8080"
)

$isAdmin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator
    )

if (-not $isAdmin) {
    throw "Ejecuta este script en PowerShell como Administrador."
}

$service = Get-Service iphlpsvc -ErrorAction Stop
if ($service.Status -ne "Running") {
    Start-Service iphlpsvc
}

$wslIp = (wsl.exe -d $Distro bash -lc "hostname -I | awk '{print \$1}'").Trim()
if ([string]::IsNullOrWhiteSpace($wslIp)) {
    throw "No pude obtener la IP de WSL para la distro '$Distro'."
}

netsh interface portproxy delete v4tov4 listenaddress=$ListenAddress listenport=$Port | Out-Null
netsh interface portproxy add v4tov4 `
    listenaddress=$ListenAddress `
    listenport=$Port `
    connectaddress=$wslIp `
    connectport=$Port | Out-Null

$rule = Get-NetFirewallRule -DisplayName $FirewallRuleName -ErrorAction SilentlyContinue
if (-not $rule) {
    New-NetFirewallRule `
        -DisplayName $FirewallRuleName `
        -Direction Inbound `
        -Action Allow `
        -Protocol TCP `
        -LocalPort $Port `
        -Profile Any | Out-Null
}

Write-Host "Portproxy configurado: ${ListenAddress}:$Port -> ${wslIp}:$Port"
Write-Host "Firewall listo con regla '$FirewallRuleName'"
Write-Host "Verificacion:"
netsh interface portproxy show all
