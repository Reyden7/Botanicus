param([ValidateSet('BuildHologram','ValidatePlacement','CapturePlacement','ProfilePlacement','RegressionGrowth','RegressionEquipment')][string]$Scenario='ValidatePlacement',[string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference='Stop'
$repoRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$projectRoot=Join-Path $repoRoot 'Botanicus';$taskRoot=Join-Path $projectRoot 'Saved\Diagnostics\PlacementJuice4A1';$profileRoot=Join-Path $taskRoot 'isolated_user'
$saveRoot=Join-Path $profileRoot 'Saved\SaveGames';$configRoot=Join-Path $profileRoot 'Saved\Config\WindowsEditor'
New-Item -ItemType Directory -Force -Path $saveRoot,$configRoot | Out-Null
Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Saved\SaveGames') -File -Filter '*.sav' | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $saveRoot $_.Name)}
$ini=@'
[/Script/UnrealEd.LevelEditorPlaySettings]
PlayNetMode=PIE_ListenServer
RunUnderOneProcess=True
PlayNumberOfClients=4
EnableGameSound=True
CreateAudioDeviceForEveryPlayer=True
ClientWindowWidth=960
ClientWindowHeight=540
'@
[IO.File]::WriteAllText((Join-Path $configRoot 'EditorPerProjectUserSettings.ini'),$ini)
$scriptPath=Join-Path $taskRoot ($Scenario+'.py');Copy-Item -LiteralPath (Join-Path $PSScriptRoot ($Scenario+'.py')) -Destination $scriptPath
$bootstrapPath=Join-Path $taskRoot 'StartPlacement.py';$pythonPath=$scriptPath.Replace('\','/');$errorPath=(Join-Path $taskRoot 'startup_error.txt').Replace('\','/')
$bootstrap=@"
import unreal,traceback
from pathlib import Path
try:
    placement_state={'__file__':r'$pythonPath','__name__':'__main__'}
    exec(compile(Path(r'$pythonPath').read_text(encoding='utf-8'),r'$pythonPath','exec'),placement_state)
except BaseException:
    Path(r'$errorPath').write_text(traceback.format_exc(),encoding='utf-8');unreal.SystemLibrary.quit_editor()
"@
[IO.File]::WriteAllText($bootstrapPath,$bootstrap,[Text.UTF8Encoding]::new($false));$started=[DateTime]::UtcNow
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') (Join-Path $projectRoot 'Botanicus.uproject') "-ExecCmds=sg.EffectsQuality 3,py $($bootstrapPath.Replace('\','/'))" "-UserDir=$($profileRoot.Replace('\','/'))" -BotanicusJuiceValidation -BotanicusEquipmentValidation -BotanicusNoctifloreValidation -BotanicusPlacementValidation -unattended -RenderOffscreen -windowed -ResX=1280 -ResY=720 -nop4 "-abslog=$taskRoot\$Scenario.log" *> (Join-Path $taskRoot ($Scenario+'_console.log'))
if($LASTEXITCODE -ne 0){throw "Unreal failed: $Scenario"}
$name=switch($Scenario){'BuildHologram' {'hologram_asset.json'} 'ValidatePlacement' {'validation_placement.json'} 'CapturePlacement' {'capture_placement.json'} 'ProfilePlacement' {'profile_placement.json'} 'RegressionGrowth' {'validation_growth.json'} 'RegressionEquipment' {'validation_equipment.json'}}
$resultPath=Join-Path $taskRoot $name
if(!(Test-Path -LiteralPath $resultPath) -or (Get-Item -LiteralPath $resultPath).LastWriteTimeUtc -lt $started){throw 'No fresh JSON result. See startup_error.txt'}
$r=Get-Content -Raw -LiteralPath $resultPath | ConvertFrom-Json
if(!$r.passed -or $r.errors.Count -gt 0){throw "Failed: $resultPath"}
Write-Output "Passed: $resultPath"
