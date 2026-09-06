$ErrorActionPreference = 'Stop'

# Do not build and start a second emulator over an already working instance.
try {
    $running = Invoke-WebRequest 'http://127.0.0.1:8000/api/v1/storage/status' `
        -UseBasicParsing -TimeoutSec 2
    if ($running.StatusCode -eq 200) {
        Write-Host 'Energy Monitor QEMU is already running.' -ForegroundColor Yellow
        Write-Host 'Dashboard:  http://127.0.0.1:8000/' -ForegroundColor Cyan
        Write-Host 'Statistics: http://127.0.0.1:8000/statistics' -ForegroundColor Cyan
        exit 0
    }
} catch {
    # No responsive instance: continue with build and startup.
}

. 'C:\Espressif\tools\Microsoft.v6.0.PowerShell_profile.ps1'

$env:IDF_CCACHE_ENABLE = '0'
$env:Path = 'C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin;' +
    'C:\Espressif\tools\ninja\1.12.1;' +
    'C:\Espressif\tools\cmake\4.0.3\bin;' +
    'C:\Program Files\Git\mingw64\bin;' + $env:Path
$env:GIT_CONFIG_COUNT = '2'
$env:GIT_CONFIG_KEY_0 = 'safe.directory'
$env:GIT_CONFIG_VALUE_0 = 'C:/Espressif/v6.0/esp-idf'
$env:GIT_CONFIG_KEY_1 = 'safe.directory'
$env:GIT_CONFIG_VALUE_1 = 'C:/Espressif/v6.0/esp-idf/components/openthread/openthread'

Write-Host '=== Energy Monitor: build ===' -ForegroundColor Green
& 'C:\Espressif\tools\ninja\1.12.1\ninja.exe' -C '.\build' all
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host '=== Energy Monitor: QEMU flash image ===' -ForegroundColor Green
& 'C:\Espressif\tools\python\v6.0\venv\Scripts\python.exe' -m esptool `
    --chip esp32 merge-bin `
    -o '.\build\qemu_flash.bin' `
    --flash-mode dio --flash-freq 40m --flash-size 2MB --pad-to-size 2MB `
    0x1000 '.\build\bootloader\bootloader.bin' `
    0x8000 '.\build\partition_table\partition-table.bin' `
    0x10000 '.\build\energy_monitor.bin'
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host '=== UART QEMU (Ctrl+A, X aby zakończyć) ===' -ForegroundColor Cyan
$sdImage = '.\build\sd_image.bin'
if (-not (Test-Path -LiteralPath $sdImage)) {
    Write-Host '=== Creating persistent 4 GB emulated SD card ===' -ForegroundColor Green
    $sdStream = [System.IO.File]::Open($sdImage, [System.IO.FileMode]::CreateNew)
    try { $sdStream.SetLength(4GB) } finally { $sdStream.Dispose() }
}
& 'C:\Espressif\tools\tools\qemu-xtensa\esp_develop_9.2.2_20250817\qemu\bin\qemu-system-xtensa.exe' `
    -M esp32 -m 4M -accel 'tcg,tb-size=32' `
    -drive 'file=build/qemu_flash.bin,if=mtd,format=raw' `
    -drive 'file=build/sd_image.bin,if=sd,format=raw' `
    -nic 'user,model=open_eth,hostfwd=tcp::8000-:80' `
    -nographic -serial mon:stdio
