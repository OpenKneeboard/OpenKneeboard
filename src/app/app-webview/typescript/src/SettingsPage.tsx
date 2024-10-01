import SystemTheme from "../generated/SystemTheme";

declare var InitData: {
  class: string;
  instanceID: string;
  warnings: string[];
  systemTheme: SystemTheme;
};

// Not using React here so this is as reliable as possible
for (const warning of InitData.warnings) {
  let node = document.createElement("div");
  node.className = "nativeWarning";
  node.textContent = warning;
  document.body.prepend(node);
}

function CreateThemedStylesheet(theme: SystemTheme): void {
  let node = document.createElement("style");
  let css = ':root {\n';
  for (const [key, value] of Object.entries(theme.uiColors)) {
    css += `--system-color-${key}: ${value};\n`;
  }
  // These seem to be unthemed and not useful?
  for (const [key, value] of Object.entries(theme.uiElementColors)) {
    css += `--legacy-windows-ui-element-color-${key} { background-color: ${value}; }\n`;
  }
  css += '}';
  node.textContent = css;

  document.head.prepend(node);
}
CreateThemedStylesheet(InitData.systemTheme);

import * as React from "react";
import * as ReactDOM from "react-dom/client";
import "./DeveloperToolsSettingsPage";
import DeveloperToolsSettingsPage from "./DeveloperToolsSettingsPage"
import NativeRequest from "./NativeRequest";

async function main() {
  const instanceData = await NativeRequest.Send("NativeObjectToJSON", {
    class: InitData.class,
    instanceID: InitData.instanceID,
  });

  const root = ReactDOM.createRoot(document.getElementById("root"));
  switch (InitData.class) {
    case "DeveloperToolsSettingsPage":
      root.render(<DeveloperToolsSettingsPage id={InitData.instanceID} initData={instanceData} />);
      break;
    default:
      root.render(
        <div className={"nativeWarning"}>Invalid class name: {InitData.class}</div>
      );
  }
}

await main();