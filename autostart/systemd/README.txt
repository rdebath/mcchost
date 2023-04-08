
The "unit file" mcchost.service sets up systemd to restart mcchost
in a similar fashion to inittab.

# Install it
sudo cp mcchost.service /etc/systemd/system/
sudo systemctl daemon-reload

# Enable now:
sudo systemctl start mcchost.service

# Enable boot:
sudo systemctl enable mcchost.service
