# Guia Rapida: 2 PCs por Switch

Esta guia asume este escenario:

- Esta PC sera el servidor.
- Solo hay otra PC cliente conectada al mismo switch.
- El proyecto se compila y ejecuta dentro de WSL Ubuntu.

## 1. Preparar la red fisica

En ambas PCs:

1. Conecta cada equipo al switch con RJ45.
2. Espera 10-15 segundos.
3. Abre PowerShell y revisa el nombre del adaptador cableado:

```powershell
Get-NetAdapter -Physical | Sort-Object Name | Format-Table Name,Status,LinkSpeed,InterfaceDescription -AutoSize
```

4. Si Wi-Fi sigue activo, desactivalo temporalmente para que no se use otra ruta:

```powershell
Disable-NetAdapter -Name "Wi-Fi" -Confirm:$false
```

## 2. Poner IP estatica

Importante:

- Si abres PowerShell como Administrador, normalmente inicia en `C:\Windows\System32`.
- Por eso `.\scripts\...` falla si no te moviste antes al repo.
- Usa uno de estos dos enfoques:

Opcion A: moverte primero al repo

```powershell
Set-Location "C:\Users\rjbar\OneDrive\Documents\GitHub\PRY01-OS1"
```

Opcion B: ejecutar con ruta absoluta, sin importar en que carpeta estes

```powershell
$repo = "C:\Users\rjbar\OneDrive\Documents\GitHub\PRY01-OS1"
```

Servidor, en PowerShell como Administrador:

```powershell
$repo = "C:\Users\rjbar\OneDrive\Documents\GitHub\PRY01-OS1"
& "$repo\scripts\set_switch_ip_windows.ps1" -AdapterAlias "Ethernet" -IpAddress 192.168.50.10
```

Cliente, en PowerShell como Administrador:

```powershell
$repo = "C:\RUTA\AL\REPO\PRY01-OS1"
& "$repo\scripts\set_switch_ip_windows.ps1" -AdapterAlias "Ethernet 2" -IpAddress 192.168.50.11
```

Verifica en ambas PCs:

```powershell
ipconfig
```

Esperado:

- Servidor: `Ethernet` con `192.168.50.10`
- Cliente: `Ethernet` con `192.168.50.11`
- WSL sigue en `172.x.x.x`

## 3. Configurar Windows para exponer WSL y abrir firewall

En esta PC servidor, abre PowerShell como Administrador:

```powershell
$repo = "C:\Users\rjbar\OneDrive\Documents\GitHub\PRY01-OS1"
& "$repo\scripts\configure_server_forwarding.ps1" -Distro "Ubuntu" -AdapterAlias "Ethernet" -ListenAddress 192.168.50.10 -Port 8080
```

Eso hace dos cosas:

- Redirige `192.168.50.10:8080` hacia la IP interna de WSL.
- Abre el puerto `8080/TCP` en el firewall de Windows.
- Habilita respuesta a `ping` en IPv4.
- Intenta mover el perfil de la red cableada a `Private`.

Importante:

- Los clientes deben usar la IP de Windows (`192.168.50.10`).
- No uses la IP interna de WSL (`172.x.x.x`) en la otra PC.

## 4. Validar conectividad basica

Desde el cliente, en PowerShell:

```powershell
ping 192.168.50.10
```

Si esto falla incluso despues del paso 3, revisa:

- Que el adaptador correcto sea `Ethernet` o `Ethernet 2` segun la PC.
- Que ambos tengan IP en `192.168.50.x/24`.
- Que el switch y los cables esten bien conectados.

## 5. Levantar el servidor

En Ubuntu/WSL de esta PC:

```bash
cd /mnt/c/Users/rjbar/OneDrive/Documents/GitHub/PRY01-OS1
./scripts/start_server_wsl.sh 8080
```

Dejalo corriendo.

## 6. Verificar el puerto desde el cliente

En PowerShell del cliente:

```powershell
Test-NetConnection 192.168.50.10 -Port 8080
```

Esperado:

- `TcpTestSucceeded : True`

## 7. Conectar el cliente

En Ubuntu/WSL de la otra PC:

```bash
cd /mnt/c/RUTA/AL/REPO/PRY01-OS1
make
./bin/chat_client Ana 192.168.50.10 8080
```

Usa cualquier nombre distinto para el usuario.

## 7.1 Si PowerShell si conecta pero Ubuntu dice `No route to host`

Este caso ya fue reproducido con WSL2 en modo NAT:

- En Windows, `ping 192.168.50.10` y `Test-NetConnection 192.168.50.10 -Port 8080` pueden dar bien.
- En Ubuntu/WSL, el cliente todavia puede fallar con `connect: No route to host`.

La correccion recomendada para la PC cliente es mover WSL a `mirrored networking`.

En PowerShell del cliente:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
@"
[wsl2]
networkingMode=mirrored
dnsTunneling=true
firewall=true
"@ | Set-Content -Path "$env:USERPROFILE\.wslconfig"
wsl --shutdown
```

Luego vuelve a abrir Ubuntu y verifica:

```bash
hostname -I
ip route
ping -c 3 192.168.50.10
```

Esperado:

- WSL ya no queda solo en `172.x.x.x`.
- Debe aparecer una ruta directa a `192.168.50.0/24`.
- El `ping` al servidor debe responder.

Despues prueba de nuevo:

```bash
cd /mnt/c/Users/Roberto/Documents/GitHub/PRY01-OS1
./bin/chat_client Ana 192.168.50.10 8080
```

No apliques este cambio en la PC servidor si ya te esta funcionando con `portproxy`; aqui solo hace falta en la PC cliente.

## 7.2 Si `mirrored mode` no se activa

Si despues de crear `%UserProfile%\.wslconfig` y ejecutar `wsl --shutdown` sigues viendo algo como esto en Ubuntu:

```bash
hostname -I
172.x.x.x

ip route
default via 172.x.x.1 dev eth0
```

entonces WSL sigue en NAT y la configuracion fue ignorada.

La salida mas comun en ese caso es:

- En Windows, `ping 192.168.50.10` funciona.
- En Ubuntu, `ping 192.168.50.10` da `Destination Host Unreachable`.
- El cliente muestra `connect: No route to host`.

### Fallback recomendado para la PC cliente: usar WSL 1

WSL 1 comparte la pila de red de Windows, asi que si Windows cliente ya llega a `192.168.50.10:8080`, el cliente de chat en WSL 1 normalmente tambien puede llegar.

Antes de cambiar de version, revisa la version actual:

```powershell
wsl -l -v
```

Luego convierte solo Ubuntu del cliente a WSL 1:

```powershell
wsl --set-version Ubuntu 1
```

Notas:

- Segun Microsoft, cambiar entre WSL 1 y WSL 2 puede tardar y puede fallar en algunos casos.
- En este proyecto el repo esta en `C:\Users\Roberto\Documents\GitHub\PRY01-OS1`, asi que los archivos del proyecto no dependen del disco interno de la distro.
- Haz este cambio solo en la PC cliente.

Cuando termine, vuelve a abrir Ubuntu y valida:

```bash
hostname -I
ip route
ping -c 3 192.168.50.10
```

Luego prueba el cliente:

```bash
cd /mnt/c/Users/Roberto/Documents/GitHub/PRY01-OS1
./bin/chat_client Ana 192.168.50.10 8080
```

## 8. Prueba minima funcional

En el cliente:

```text
/list
/help
/quit
```

Si quieres validar el codigo sin depender de la red fisica, en esta misma PC puedes hacer una prueba local:

```bash
cd /mnt/c/Users/rjbar/OneDrive/Documents/GitHub/PRY01-OS1
./scripts/test_local_loopback_wsl.sh 5050
```

## 9. Fallas comunes

- `No route to host`: la otra PC no esta en la misma subred o sigue saliendo por Wi-Fi.
- `Connection refused`: el servidor no esta corriendo o el `portproxy` apunta a una IP vieja de WSL.
- `TcpTestSucceeded : False`: firewall o `portproxy` mal configurado.
- `IP_ALREADY_CONNECTED`: pasa solo si intentas conectar dos clientes desde la misma IP; con 2 PCs distintas no deberia aparecer.
- `.\scripts\... is not recognized`: estas parado en otra carpeta, usualmente `C:\Windows\System32`; usa `Set-Location` al repo o ejecuta el script con ruta absoluta.
- `No existe un adaptador llamado 'Ethernet'`: usa el nombre exacto que devolvio `Get-NetAdapter -Physical`, por ejemplo `Ethernet 2`.
- `La ejecucion de scripts esta deshabilitada`: primero corre `Set-ExecutionPolicy -Scope Process Bypass`.
