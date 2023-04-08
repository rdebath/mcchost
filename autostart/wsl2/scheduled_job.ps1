# WARNING: This requires PS script execution to be "enabled".
# The scheduled task will still be created if not, however, it will fail silently.
$trigger = New-JobTrigger -AtStartup -RandomDelay 00:00:15
Register-ScheduledJob -Trigger $trigger -FilePath C:\usr\wsl2-start.ps1 -Name StartWSL2Server
