{
  description = "app_ui — QML view with C++ backend, depends on app_core";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder/tutorial-v2";
    app_core.url = "path:../app-core";
  };

  outputs = inputs@{ logos-module-builder, ... }:
    logos-module-builder.lib.mkLogosQmlModule {
      src = ./.;
      configFile = ./metadata.json;
      flakeInputs = inputs;
    };
}
