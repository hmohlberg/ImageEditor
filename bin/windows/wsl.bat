cd ..\..

powershell.exe /c wsl -d Debian -e git pull

powershell.exe /c wsl -d Debian -e 'bin/debian/ImageEditor' --config 'samples/misc/config.ini' --file 'samples/images/pm2382o.png'

pause
