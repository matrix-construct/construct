self: { config, pkgs, lib, ... }:

let
  cfg = config.services.matrix-construct;
in {
  options.services.matrix-construct = with lib; {
    enable = mkEnableOption "the construct server";

    useScreen = mkOption {
      type = types.bool;
      default = true;
      example = false;
      description = ''
        Run construct in screen for stdio access.
      '';
    };

    setupUnbound = mkOption {
      type = types.bool;
      default = true;
      example = false;
      description = ''
        Setup default unbound forwardAddresses.
      '';
    };

    extraArgs = mkOption {
      type = with types; listOf str;
      default = [];
      example = [ "-6" "--debug" ];
      description = ''
        Extra flags to pass to construct.
      '';
    };

    package = mkOption {
      type = types.package;
      default = self.packages.${pkgs.system}.matrix-construct;
      defaultText = "pkgs.matrix-construct";
      description = ''
        Guix package to use.
      '';
    };

    server = mkOption {
      type = types.str;
      default = null;
      example = "matrix.example.org";
      description = ''
        Server configuration to run construct with.
      '';
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ cfg.package ] ++ lib.optional cfg.useScreen pkgs.screen;

    systemd.services.matrix-construct = {
      description = "Matrix Construct";
      wantedBy = [ "multi-user.target" ];

      ## bin/construct host.tld [servername]
      ## Connect to screen
      ##  Wait for init, then press ctrl-c
      ##  Create listener with `net listen matrix * 8448 privkey.pem cert.pem chain.pem`
      ##   ..I used /var/lib/acme/xa0.uk/key.pem /(...)/xa0.uk/fullchain.pem /(...)/xa0.uk/fullchain.pem`
      ##  Route and test with https://matrix.org/federationtester/api/report?server_name=host.tld
      ##  Restart, or reload with `mod reload web_root`
      ## Exit screen
      script = '' cd $STATE_DIRECTORY && exec ''
        + (if cfg.useScreen then '' ${pkgs.screen}/bin/screen -D -m '' else "")
        + '' ${cfg.package}/bin/construct ${cfg.server} ${lib.concatStringsSep " " cfg.extraArgs} '';

      serviceConfig = {
        Restart = "on-failure";
        ConfigurationDirectory = "construct";
        RuntimeDirectory = "construct";
        StateDirectory = "construct"; # Todo: bootstrap
        LogsDirectory = "construct";
        StandardOutput = "syslog";
        StandardError = "syslog";
        TimeoutStopSec = "120";
        KillSignal = "SIGQUIT";
      };
    };

    services.unbound.forwardAddresses = lib.mkIf cfg.setupUnbound [ "4.2.2.1" "4.2.2.2" "4.2.2.3" "4.2.2.4" "4.2.2.5" "4.2.2.6" ];
  };
}
