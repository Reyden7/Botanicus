param(
    [ValidateSet('Validate','Profile','CountLegacy','RegressionGrowth','RegressionReturn')][string]$Scenario='Validate',
    [string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8'
)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$projectRoot=Join-Path $repoRoot 'Botanicus'
$taskRoot=Join-Path $projectRoot 'Saved\Diagnostics\EquipmentPresentation'
$profileRoot=Join-Path $taskRoot 'isolated_user'
$saveRoot=Join-Path $profileRoot 'Saved\SaveGames'
$configRoot=Join-Path $profileRoot 'Saved\Config\WindowsEditor'
New-Item -ItemType Directory -Force -Path $saveRoot,$configRoot | Out-Null
Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Saved\SaveGames') -File -Filter '*.sav' | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $saveRoot $_.Name)
}
$ini=@'
[/Script/UnrealEd.LevelEditorPlaySettings]
PlayNetMode=PIE_ListenServer
RunUnderOneProcess=True
PlayNumberOfClients=4
EnableGameSound=True
CreateAudioDeviceForEveryPlayer=True
ClientWindowWidth=640
ClientWindowHeight=360
'@
[IO.File]::WriteAllText((Join-Path $configRoot 'EditorPerProjectUserSettings.ini'),$ini)
$flagPath=Join-Path $taskRoot 'baseline_only.flag'
if($Scenario -eq 'CountLegacy') {[IO.File]::WriteAllText($flagPath,'Count the historical decision path.')}
elseif(Test-Path -LiteralPath $flagPath) {Remove-Item -LiteralPath $flagPath}
$scriptName=switch($Scenario) {'Validate' {'ValidateEquipment.py'} 'Profile' {'ProfileEquipment.py'} 'CountLegacy' {'ProfileEquipment.py'} 'RegressionGrowth' {'RegressionGrowth.py'} 'RegressionReturn' {'RegressionReturn.py'}}
$resultName=switch($Scenario) {'Validate' {'validation_equipment.json'} 'Profile' {'profile_equipment.json'} 'CountLegacy' {'before_native.json'} 'RegressionGrowth' {'validation_growth.json'} 'RegressionReturn' {'validation_return.json'}}
$scriptPath=Join-Path $taskRoot $scriptName
Copy-Item -LiteralPath (Join-Path $PSScriptRoot $scriptName) -Destination $scriptPath
$bootstrapPath=Join-Path $taskRoot 'StartEquipmentValidation.py'
$pythonScriptPath=$scriptPath.Replace('\','/')
$pythonErrorPath=(Join-Path $taskRoot 'startup_error.txt').Replace('\','/')
$bootstrap=@"
import unreal,traceback
from pathlib import Path
try:
    equipment_state={'__file__':r'$pythonScriptPath','__name__':'__main__'}
    exec(compile(Path(r'$pythonScriptPath').read_text(encoding='utf-8'),r'$pythonScriptPath','exec'),equipment_state)
except BaseException:
    Path(r'$pythonErrorPath').write_text(traceback.format_exc(),encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
"@
[IO.File]::WriteAllText($bootstrapPath,$bootstrap,[Text.UTF8Encoding]::new($false))
$started=[DateTime]::UtcNow
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logPath=Join-Path $taskRoot ($Scenario+'.log')
& $editor (Join-Path $projectRoot 'Botanicus.uproject') "-ExecCmds=sg.EffectsQuality 3,py $($bootstrapPath.Replace('\','/'))" "-UserDir=$($profileRoot.Replace('\','/'))" -BotanicusJuiceValidation -BotanicusEquipmentValidation -unattended -RenderOffscreen -windowed -ResX=1280 -ResY=720 -nop4 "-abslog=$logPath" *> (Join-Path $taskRoot ($Scenario+'_console.log'))
if($LASTEXITCODE -ne 0) {throw "Editor exited $LASTEXITCODE. See $logPath"}
$resultPath=Join-Path $taskRoot $resultName
if(!(Test-Path -LiteralPath $resultPath) -or (Get-Item -LiteralPath $resultPath).LastWriteTimeUtc -lt $started) {throw "No fresh result. See $logPath and startup_error.txt"}
$result=Get-Content -Raw -LiteralPath $resultPath | ConvertFrom-Json
if(!$result.passed -or $result.errors.Count -gt 0) {throw "Diagnostic failed: $resultPath"}
if($Scenario -eq 'Profile') {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'SummarizeEquipment.py') -Destination (Join-Path $taskRoot 'SummarizeEquipment.py')
}
Write-Output "Passed. Results: $resultPath"
