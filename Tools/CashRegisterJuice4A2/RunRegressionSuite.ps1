$ErrorActionPreference='Stop'
$repoRoot=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$diagnostics=Join-Path $repoRoot 'Botanicus\Saved\Diagnostics\CashRegisterJuice4A2'
$results=[System.Collections.Generic.List[object]]::new()
$jobs=@(
    @('CashRegisterJuice4A2','CaptureCheckout'),
    @('CashRegisterJuice4A2','SmokeClient'),
    @('VisualJuice3B','Regression3A'),
    @('PlacementJuice4A1','RegressionGrowth'),
    @('PlacementJuice4A1','RegressionEquipment'),
    @('PlacementJuice4A1','ValidatePlacement'),
    @('VisualJuice3C','ValidateNoctiflores')
)
foreach($job in $jobs){
    $began=[DateTime]::UtcNow
    try{
        Write-Output "Starting $($job[0]) / $($job[1])"
        & (Join-Path $repoRoot "Tools\$($job[0])\RunValidation.ps1") -Scenario $job[1]
        $results.Add([pscustomobject]@{suite=$job[0];scenario=$job[1];passed=$true;started=$began;finished=[DateTime]::UtcNow})
    }catch{
        $results.Add([pscustomobject]@{suite=$job[0];scenario=$job[1];passed=$false;error=$_.Exception.Message;started=$began;finished=[DateTime]::UtcNow})
        Write-Output "FAILED: $($_.Exception.Message)"
    }
    $results | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $diagnostics 'regression_queue.json') -Encoding UTF8
}
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' (Join-Path $repoRoot 'Botanicus\Botanicus.uproject') '-ExecCmds=Automation RunTests Botanicus.Audio' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$diagnostics\AudioAutomation" "-UserDir=$($diagnostics.Replace('\','/'))/isolated_user" -unattended -NullRHI -nop4 "-abslog=$diagnostics\AudioAutomation.log" *> (Join-Path $diagnostics 'AudioAutomation_console.log')
& 'C:\Windows\System32\nvidia-smi.exe' --query-gpu=name,driver_version,utilization.gpu,memory.used,memory.total --format=csv,noheader > (Join-Path $diagnostics 'gpu_before_profile.txt')
Get-Process | Select-Object ProcessName,Id,CPU | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $diagnostics 'processes_before_profile.json') -Encoding UTF8
& (Join-Path $PSScriptRoot 'RunValidation.ps1') -Scenario ProfileCheckout
