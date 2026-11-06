param([string]$Scenario='Smoke',[string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',[switch]$KeepSaves)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$projectRoot=Join-Path $repoRoot 'Botanicus'
$taskRoot=Join-Path $projectRoot 'Saved\Diagnostics\CashRegisterJuice4A2'
$profileRoot=Join-Path $taskRoot 'isolated_user'
$saveRoot=Join-Path $profileRoot 'Saved\SaveGames'
$configRoot=Join-Path $profileRoot 'Saved\Config\WindowsEditor'
New-Item -ItemType Directory -Force -Path $saveRoot,$configRoot | Out-Null
if(!$KeepSaves){Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Saved\SaveGames') -File -Filter '*.sav' | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $saveRoot $_.Name)}}
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
$scriptPath=Join-Path $taskRoot ($Scenario+'.py')
if($Scenario -eq 'ProfileCheckout'){
    # Build before starting the editor; never compile during the measured blocks.
    foreach($target in @('BotanicusEditor','Botanicus')){
        & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') $target Win64 Development "-Project=$projectRoot\Botanicus.uproject" -WaitMutex -NoHotReloadFromIDE *> (Join-Path $taskRoot ('build_'+$target+'.log'))
        if($LASTEXITCODE -ne 0){throw "Build failed: $target"}
    }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot ($Scenario+'.py')) -Destination $scriptPath
$bootstrapPath=Join-Path $taskRoot 'StartCash.py'
$pythonPath=$scriptPath.Replace('\','/')
$errorPath=(Join-Path $taskRoot 'startup_error.txt').Replace('\','/')
$bootstrap=@"
import unreal,traceback
from pathlib import Path
try:
    state={'__file__':r'$pythonPath','__name__':'__main__'}
    exec(compile(Path(r'$pythonPath').read_text(encoding='utf-8-sig'),r'$pythonPath','exec'),state)
except BaseException:
    Path(r'$errorPath').write_text(traceback.format_exc(),encoding='utf-8');unreal.SystemLibrary.quit_editor()
"@
[IO.File]::WriteAllText($bootstrapPath,$bootstrap,[Text.UTF8Encoding]::new($false))
$started=[DateTime]::UtcNow
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') (Join-Path $projectRoot 'Botanicus.uproject') "-ExecCmds=sg.EffectsQuality 3,py $($bootstrapPath.Replace('\','/'))" "-UserDir=$($profileRoot.Replace('\','/'))" -BotanicusCashValidation -BotanicusJuiceValidation -BotanicusEquipmentValidation -BotanicusNoctifloreValidation -BotanicusPlacementValidation -unattended -RenderOffscreen -windowed -ResX=1280 -ResY=720 -nop4 "-abslog=$taskRoot\$Scenario.log" *> (Join-Path $taskRoot ($Scenario+'_console.log'))
if($LASTEXITCODE -ne 0){throw "Unreal failed: $Scenario"}
$resultName=switch($Scenario){'Smoke' {'smoke.json'} 'SmokeClient' {'smoke_client.json'} 'ValidateCheckout' {'validation_checkout.json'} 'ValidateDisconnect' {'validation_disconnect.json'} 'CaptureCheckout' {'capture_checkout.json'} 'ProfileCheckout' {'profile_checkout.json'}}
if($resultName){
    $resultPath=Join-Path $taskRoot $resultName
    if(!(Test-Path -LiteralPath $resultPath) -or (Get-Item -LiteralPath $resultPath).LastWriteTimeUtc -lt $started){throw "No fresh result: $Scenario"}
    $result=Get-Content -Raw -LiteralPath $resultPath | ConvertFrom-Json
    if(!$result.passed -or $result.errors.Count -gt 0){throw "Validation failed: $resultPath"}
    Write-Output "Passed: $resultPath"
}
