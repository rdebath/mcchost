Files for strting MCChost using systemd.

# put in system:
sudo cp mcchost* /etc/systemd/system/
sudo systemctl daemon-reload

# Enable now:
sudo systemctl start mcchost.socket
sudo systemctl start mcchost-timer.timer

# Enable boot:
sudo systemctl enable mcchost.socket
sudo systemctl enable mcchost-timer.timer


