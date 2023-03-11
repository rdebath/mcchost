These scripts have been tested on Debian running in WSL2.

WARNING:
    Load of large maps can be horribly slow on windows filesystems.
    I strongly suggest that the "level" subdirectory is never placed under
    the /mnt/c directory.

NOTE:
    All the subdirectories under the mcchost directory can be replaced by
    symbolic links to relocate those directories onto other filesystems.
    The "$HOME/.mcchost" directory can be a link too.

Scripts:

scheduled_job.ps1
    This powershell script creates a scheduled job to start
    C:\usr\wsl2-start.ps1 when windows is booted.
    It needs to be run "as Administrator" and requires PS scripting turned on.

wsl2-start.ps1
    This posershell script starts mcchost-server and sshd inside wsl2.
    It also forwards ports 25565 and 22 to the new IP address of the
    wsl2 virtual machine.

wsl2-start.cmd
    Cmd script to start powershell and run wsl2-start.ps1 without PS
    scripting turned on. Can be called from Task scheduler using "cmd /c"

etc_sudoers_nopass_ssh
    This file needs to be placed in /etc/sudoers.d to allow sshd to start.

etc_sudoers_nopass_all
    If this is placed in /etc/sudoers.d no sudo command asks for a password.


