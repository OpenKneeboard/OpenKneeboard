import {
  DeveloperToolsSettingsPageNative,
  CrashKind,
  CrashLocation
} from "../generated/DeveloperToolsSettingsPageNative";
import * as React from "react";
import * as nx from "./NotXAML";
import NativeRequest from "./NativeRequest";
import {useState} from "react";

function FolderPicker({value, defaultValue, onChange, placeholder}: {
  defaultValue?: string,
  value?: string,
  onChange?: (value: string) => void,
  placeholder: string
}) {
  return (
    <nx.StackPanel orientation={"Horizontal"}>
      <nx.TextBox
        value={value}
        defaultValue={defaultValue}
        placeholder={placeholder}
        style={{flexGrow: 1}}
        onChange={(value) => {
          if (onChange) {
            onChange(value);
          }
        }}
      />
      <nx.Button
        onClick={async () => {
          try {
            const path = await NativeRequest.Send("ShowFolderPicker", {}) as string;
            if (onChange) {
              onChange(path);
            }
          } catch (e) {
          }
        }}>
        <nx.FontIcon glyph={"OpenFolderHorizontal"}/>
      </nx.Button>
    </nx.StackPanel>
  );
}

function propertyReactState<O, P extends keyof O>(o: O, prop: P) {
  const initialValue = o[prop];
  let [state, setReactState] = useState(initialValue);
  let setState = (nextState: typeof state) => {
    o[prop] = nextState;
    setReactState(nextState);
  };
  return [state, setState] as const;
}

export default function DeveloperToolsSettingsPage({id, initData}: { id: string, initData: any }) {
  const [native, _] = useState(new DeveloperToolsSettingsPageNative(id, initData));
  const [sourcePath, setSourcePath] = propertyReactState(native, "AppWebViewSourcePath");
  const [autoUpdateVersion, setAutoUpdateVersion] = propertyReactState(native, "AutoUpdateFakeCurrentVersion")
  const [isHKCUPluginHandler, setIsHKCUPluginHandler] = propertyReactState(native, "IsPluginFileTypeInHKCU")

  const [crashKind, setCrashKind] = useState(Object.values(CrashKind).at(0));
  const [crashLocation, setCrashLocation] = useState(Object.values(CrashLocation).at(0));

  return (
    <nx.Page>
      <nx.Title>Developer Tools</nx.Title>
      <nx.Caption>Debug information</nx.Caption>
      <nx.StackPanel orientation={"Vertical"}>
        <nx.StackPanel orientation={"Horizontal"}>
          <nx.Button
            onClick={() => native.CopyDebugMessagesToClipboard()}>
            <nx.FontIcon glyph={"Copy"}/>
            Copy debug log
          </nx.Button>
          <nx.Button
            onClick={() => native.CopyAPIEventsToClipboard()}>
            <nx.FontIcon glyph={"Copy"}/>
            Copy API events
          </nx.Button>
        </nx.StackPanel>
        <nx.Caption>Load app webview components from</nx.Caption>
        <FolderPicker
          value={sourcePath}
          placeholder={native.DefaultAppWebViewSourcePath}
          onChange={(path) => setSourcePath(path)}
        />
        <nx.Caption>Current version for auto-update check</nx.Caption>
        <nx.TextBox
          value={autoUpdateVersion}
          placeholder={native.ActualCurrentVersion}
          onChange={(value) => setAutoUpdateVersion(value)}
        />
        <nx.Caption>
          Use this executable for OpenKneeboardPlugin files via <span
          className={"code"}>HKEY_CURRENT_USER</span>
        </nx.Caption>
        <nx.ToggleSwitch
          isChecked={isHKCUPluginHandler}
          checkedContent={"HKCU override is active"}
          uncheckedContent={"Using another executable or HKLM"}
          onChange={(v) => setIsHKCUPluginHandler(v)}
        />
        <nx.StackPanel orientation={"Horizontal"}>
          <select
            value={crashKind}
            onChange={(e) => {
              setCrashKind(e.target.value as typeof crashKind);
            }}>
            <option value={CrashKind.Fatal}>call fatal()</option>
            <option value={CrashKind.Throw}>throw</option>
            <option value={CrashKind.ThrowFromNoexcept}>throw from noexcept</option>
            <option value={CrashKind.Terminate}>call std::terminate()</option>
          </select>
          <select
            value={crashLocation}
            onChange={(e) => {
              setCrashLocation(e.target.value as typeof crashLocation);
            }}>
            <option value={CrashLocation.UIThread}>from UI thread</option>
            <option value={CrashLocation.MUITask}>from Microsoft.UI task</option>
            <option value={CrashLocation.WindowsSystemTask}>from Windows.System task</option>
          </select>
          <nx.Button
            onClick={() => native.TriggerCrash(crashKind, crashLocation)}
          >💥 Crash</nx.Button>
        </nx.StackPanel>
      </nx.StackPanel>
    </nx.Page>
  );
}