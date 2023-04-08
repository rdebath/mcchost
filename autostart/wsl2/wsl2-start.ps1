wsl.exe '$HOME/bin/mcchost-server'
wsl.exe sudo /etc/init.d/ssh start
$wsl_ip = (wsl hostname -I).trim()
Write-Host "WSL Machine IP: ""$wsl_ip"""
netsh interface portproxy set v4tov4 listenaddress=0.0.0.0 listenport=25565 connectport=25565 connectaddress=$wsl_ip
netsh interface portproxy set v4tov4 listenaddress=0.0.0.0 listenport=22 connectport=22 connectaddress=$wsl_ip
