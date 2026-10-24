param([string]$Port,[string]$Weights="",[switch]$Advice,[string]$Device="cpu")
$ErrorActionPreference='Stop'
$visionPython=Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
if(-not (Test-Path -LiteralPath $visionPython)){throw 'Create .venv and install requirements-viewer.txt first; see README.md'}

$visionArgs=@((Join-Path $PSScriptRoot 'start_viewer.py'))
if($Port){$visionArgs+=@('--port',$Port)}
if($Weights){$visionArgs+=@('--weights',$Weights,'--device',$Device)}
if($Advice){$visionArgs+='--send-advice'}
& $visionPython @visionArgs
