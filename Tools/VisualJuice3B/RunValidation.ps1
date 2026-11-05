param(
    [ValidateSet('ValidateGrowth','ValidateReturn','ProfilePlantJuice','CaptureMotion','Regression3A')]
    [string]$Scenario='ValidateGrowth',
    [string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8'
)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$projectRoot=Join-Path $repoRoot 'Botanicus'
$taskRoot=Join-Path $projectRoot 'Saved\Diagnostics\VisualJuice3B'
$profileRoot=Join-Path $taskRoot 'isolated_user'
$saveRoot=Join-Path $profileRoot 'Saved\SaveGames'
$configRoot=Join-Path $profileRoot 'Saved\Config\WindowsEditor'
New-Item -ItemType Directory -Force -Path $saveRoot,$configRoot | Out-Null
# Only write the diagnostic profile; never pass the real user directory to PIE.
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
$source=Join-Path $PSScriptRoot ($Scenario+'.py')
$scriptPath=Join-Path $taskRoot ($Scenario+'.py')
Copy-Item -LiteralPath $source -Destination $scriptPath
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logPath=Join-Path $taskRoot ($Scenario+'.log')
$consolePath=Join-Path $taskRoot ($Scenario+'_console.log')
& $editor (Join-Path $projectRoot 'Botanicus.uproject') "-ExecCmds=sg.EffectsQuality 3,py $($scriptPath.Replace('\','/'))" "-UserDir=$($profileRoot.Replace('\','/'))" -BotanicusJuiceValidation -unattended -RenderOffscreen -windowed -ResX=1280 -ResY=720 -nop4 "-abslog=$logPath" *> $consolePath
if($LASTEXITCODE -ne 0) {throw "Unreal exited with code $LASTEXITCODE. See $logPath"}
$resultName=switch($Scenario) {
    'ValidateGrowth' {'validation_growth.json'}
    'ValidateReturn' {'validation_return.json'}
    'ProfilePlantJuice' {'profile_3b.json'}
    'CaptureMotion' {'capture_motion.json'}
    'Regression3A' {'pie_result.json'}
}
$result=Get-Content -Raw -LiteralPath (Join-Path $taskRoot $resultName) | ConvertFrom-Json
if($result.errors.Count -gt 0 -or ($result.PSObject.Properties.Name -contains 'passed' -and !$result.passed)) {throw "Diagnostic failed. See $resultName and $logPath"}
if($Scenario -eq 'CaptureMotion' -and (!$result.transforms_and_collision_unchanged -or !$result.low_quality_disables_motion)) {throw 'Motion or quality checks failed.'}
Write-Output "Passed. Results and captures: $taskRoot"
