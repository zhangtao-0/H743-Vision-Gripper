param(
    [string]$IarBuild = 'C:\IAR Systems\Embedded Workbench 9.2\common\bin\iarbuild.exe',
    [string[]]$Configurations = @('H743_VisionGripper','H743_VisionGripper_OV2640','H743_VisionGripper_NoCamera'),
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $taskRoot 'project\EWARM\H743_VisionGripper.ewp'
$evidenceDir = Join-Path $taskRoot 'Docs\整合交付'
if (-not (Test-Path -LiteralPath $IarBuild)) { throw "IAR build utility not found: $IarBuild" }
foreach ($configuration in $Configurations) {
    $logFile = Join-Path $evidenceDir ($configuration + '.log')
    Write-Output "Building $configuration"
    if ($Clean) {
        & $IarBuild $projectFile -clean $configuration *> $logFile
        if ($LASTEXITCODE -ne 0) { throw "Clean failed: $configuration; see $logFile" }
    }
    & $IarBuild $projectFile -build $configuration -log all *> $logFile
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $configuration; see $logFile" }
    Get-Content -LiteralPath $logFile -Tail 5
}
