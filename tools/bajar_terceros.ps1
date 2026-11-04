# ============================================================================
#  bajar_terceros.ps1 - las librerias OPCIONALES de terceros de Whisk3D-Core (no van en git: las baja este script)
#    - Open Image Denoise 2.5.1 (Intel, Apache 2.0): el denoiser que anda en CUALQUIER PC (CPU; GPU si hay)
#    - OptiX 7.2 (NVIDIA): solo los HEADERS (repo publico NVIDIA/optix-dev). La version 7.2 anda con los drivers
#      R455 en adelante (y todos los nuevos). La libreria la trae el DRIVER de NVIDIA (nvoptix.dll): sin placa
#      NVIDIA, Whisk3D no la usa.
#    - NRD 4.18.0 (NVIDIA, codigo fuente): el denoiser de TIEMPO REAL (lo usa el backend Vulkan)
#  Nada de esto es obligatorio: Whisk3D compila y corre sin ninguno (los carga al arrancar si estan).
#  Uso (desde la raiz del repo):  powershell -ExecutionPolicy Bypass -File tools\bajar_terceros.ps1
# ============================================================================
$ErrorActionPreference = 'Stop'
$raiz = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$dst = Join-Path $raiz 'libs\Whisk3DCore\thirdparty'
$tmp = Join-Path $env:TEMP 'whisk3d_terceros'
New-Item -ItemType Directory -Force $tmp | Out-Null

function Bajar($url, $zip) {
    $f = Join-Path $tmp $zip
    # (curl.exe viene con Windows 10+: Invoke-WebRequest se corta con los zips grandes)
    if (-not (Test-Path $f)) { Write-Host "bajando $url"; & curl.exe -sSL --retry 3 -o $f $url; if ($LASTEXITCODE -ne 0) { throw "no pude bajar $url" } }
    return $f
}
function Extraer($zip, $carpeta) {
    $d = Join-Path $dst $carpeta
    if (Test-Path $d) { Remove-Item -Recurse -Force $d }
    $x = Join-Path $tmp ($carpeta + '_x')
    if (Test-Path $x) { Remove-Item -Recurse -Force $x }
    Expand-Archive -Force $zip $x
    # (los zips traen una carpeta adentro: se sube un nivel)
    $hijos = @(Get-ChildItem $x)
    if ($hijos.Count -eq 1 -and $hijos[0].PSIsContainer) { Move-Item $hijos[0].FullName $d } else { Move-Item $x $d }
    Write-Host "  -> $d"
}

Extraer (Bajar 'https://github.com/RenderKit/oidn/releases/download/v2.5.1/oidn-2.5.1.x64.windows.zip' 'oidn.zip') 'oidn'
Extraer (Bajar 'https://github.com/NVIDIA/optix-dev/archive/refs/tags/v7.2.0.zip' 'optix.zip') 'optix'
Extraer (Bajar 'https://github.com/NVIDIA-RTX/NRD/archive/refs/tags/v4.18.0.zip' 'nrd.zip') 'nrd'
Write-Host 'listo'
