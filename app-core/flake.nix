{
  description = "app_core — Logos core module wrapping storage and delivery for the status UI";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder/tutorial-v2";
    storage_module.url = "github:logos-co/logos-storage-module";
    delivery_module.url = "github:logos-co/logos-delivery-module";
  };

  outputs = inputs@{ logos-module-builder, ... }:
    logos-module-builder.lib.mkLogosModule {
      src = ./.;
      configFile = ./metadata.json;
      flakeInputs = inputs;
    };
}
