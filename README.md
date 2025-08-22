# Setup

Add these lines to `/etc/nixos/configuration.nix`:
```nix
 users.users.<yourusernamehere> = {
    isNormalUser = true;
    extraGroups = [
      "dialout"
    ];
  };
  services.udev.packages = with pkgs; [ platformio-core.udev ];
  services.udev.extraRules = ''
    SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0660", GROUP="dialout"
    SUBSYSTEM=="usb", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0660", GROUP="dialout"
  '';
```
Execute `nix-shell`.
Execute `code .`
Have fun.
