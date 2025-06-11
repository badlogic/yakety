#!/bin/bash
# WSL SSH Setup Script - Run once to configure SSH in WSL

set -e

echo "=== WSL SSH Setup Script ==="
echo "This script will configure OpenSSH server in WSL"
echo

# Check if running as root
if [ "$EUID" -eq 0 ]; then 
   echo "Please run this script as a normal user, not as root"
   exit 1
fi

# Update packages
echo "Updating package list..."
sudo apt update

# Install OpenSSH server if not already installed
if ! command -v sshd &> /dev/null; then
    echo "Installing OpenSSH server..."
    sudo apt install -y openssh-server
else
    echo "OpenSSH server is already installed"
fi

# Backup original sshd_config
if [ ! -f /etc/ssh/sshd_config.backup ]; then
    echo "Backing up original SSH config..."
    sudo cp /etc/ssh/sshd_config /etc/ssh/sshd_config.backup
fi

# Configure SSH
echo "Configuring SSH server..."
sudo tee /etc/ssh/sshd_config.d/wsl.conf > /dev/null << 'EOF'
# WSL SSH Configuration
Port 22
ListenAddress 0.0.0.0
PubkeyAuthentication yes
PasswordAuthentication yes
PermitRootLogin prohibit-password
X11Forwarding yes
EOF

# Enable SSH to start automatically
echo "Enabling SSH auto-start..."
sudo systemctl enable ssh 2>/dev/null || echo "Note: systemctl not available, SSH may need manual start"

# Create .ssh directory if it doesn't exist
if [ ! -d ~/.ssh ]; then
    echo "Creating .ssh directory..."
    mkdir -p ~/.ssh
    chmod 700 ~/.ssh
fi

# Create authorized_keys file if it doesn't exist
if [ ! -f ~/.ssh/authorized_keys ]; then
    echo "Creating authorized_keys file..."
    touch ~/.ssh/authorized_keys
    chmod 600 ~/.ssh/authorized_keys
fi

# Start SSH service
echo "Starting SSH service..."
sudo service ssh start

# Check SSH status
if sudo service ssh status | grep -q "is running"; then
    echo
    echo "✓ SSH server is running successfully!"
else
    echo
    echo "✗ SSH server failed to start. Check logs with: sudo journalctl -u ssh"
    exit 1
fi

# Get WSL IP
WSL_IP=$(hostname -I | awk '{print $1}')
echo
echo "=== Setup Complete ==="
echo "SSH is configured on port 22 with public key authentication enabled"
echo "WSL IP address: $WSL_IP"
echo
echo "Next steps:"
echo "1. Add your SSH public key to ~/.ssh/authorized_keys"
echo "2. Run the Windows batch script to set up port forwarding"
echo "3. Connect using: ssh $USER@<windows-host-ip>"
echo
echo "To disable password authentication after setting up keys:"
echo "sudo sed -i 's/PasswordAuthentication yes/PasswordAuthentication no/' /etc/ssh/sshd_config.d/wsl.conf"
echo "sudo service ssh restart"