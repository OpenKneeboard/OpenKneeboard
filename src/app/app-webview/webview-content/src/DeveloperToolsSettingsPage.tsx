import {
  DeveloperToolsSettingsPageNative,
  CrashKind,
  CrashLocation
} from "../generated/DeveloperToolsSettingsPageNative";
import * as React from "react";
import * as nx from "./NotXAML";
import NativeRequest from "./NativeRequest";
import { ReactNode, useEffect, useState } from "react";
import NativeClass from "./NativeClass";

function FolderPicker({ header, value, defaultValue, onChange, placeholder }: {
  header?: ReactNode;
  defaultValue?: string,
  value?: string,
  onChange?: (value: string) => void,
  placeholder: string
}) {
  return (
    <nx.WithOptionalHeader header={header}>
      <nx.StackPanel orientation={"Horizontal"}>
        <nx.TextBox
          value={value}
          defaultValue={defaultValue}
          placeholder={placeholder}
          style={{ flexGrow: 1 }}
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
          <nx.FontIcon glyph={"OpenFolderHorizontal"} />
        </nx.Button>
      </nx.StackPanel>
    </nx.WithOptionalHeader>
  );
}

function propertyReactState<O extends {}, P extends keyof O>(o: O, p: P) {
  let [reactState, setReactState] = useState<O[P]>(o[p]);

  let setState = (nextState: O[P]) => {
    o[p] = nextState;
    setReactState(nextState);
  };

  return [reactState, setState] as const;
}

interface NativeLoader<T extends NativeClass> {
  load(instanceID: string): Promise<T>;
}

function nativeState<T extends NativeClass>(loader: NativeLoader<T>, instanceID: string): T | undefined {
  const [native, setNative] = useState<T | undefined>(undefined);

  useEffect(() => {
    const load = async () => setNative(await loader.load(instanceID));

    if (!native) {
      // noinspection JSIgnoredPromiseFromCall
      load();
    }
  });

  return native;
}


function DeveloperToolsSettingsPage({ native }: { native: DeveloperToolsSettingsPageNative }) {
  const [sourcePath, setSourcePath] = propertyReactState(native, "AppWebViewSourcePath");
  const [autoUpdateVersion, setAutoUpdateVersion] = propertyReactState(native, "AutoUpdateFakeCurrentVersion")
  const [isHKCUPluginHandler, setIsHKCUPluginHandler] = propertyReactState(native, "IsPluginFileTypeInHKCU")

  const [crashKind, setCrashKind] = useState(Object.values(CrashKind)[0]);
  const [crashLocation, setCrashLocation] = useState(Object.values(CrashLocation)[0]);

  return (
    <nx.Page>
      <nx.Title>Developer Tools</nx.Title>
      <nx.StackPanel orientation={"Vertical"}>
        <nx.WithHeader header={"Debug information"}>
          <nx.StackPanel orientation={"Horizontal"}>
            <nx.Button
              onClick={() => native.CopyDebugMessagesToClipboard()}>
              <nx.FontIcon glyph={"Copy"} />
              Copy debug log
            </nx.Button>
            <nx.Button
              onClick={() => native.CopyAPIEventsToClipboard()}>
              <nx.FontIcon glyph={"Copy"} />
              Copy API events
            </nx.Button>
          </nx.StackPanel>
        </nx.WithHeader>
        <FolderPicker
          header={"Load app webview components from"}
          value={sourcePath}
          placeholder={native.DefaultAppWebViewSourcePath}
          onChange={(path) => setSourcePath(path)}
        />
        <nx.TextBox
          header={"Current version for auto-update check"}
          value={autoUpdateVersion}
          placeholder={native.ActualCurrentVersion}
          onChange={(value) => setAutoUpdateVersion(value)}
        />
        <nx.ToggleSwitch
          header={<React.Fragment>
            Use this executable for OpenKneeboardPlugin files via <span
              className={"code"}>HKEY_CURRENT_USER</span>
          </React.Fragment>}
          isChecked={isHKCUPluginHandler}
          checkedContent={"HKCU override is active"}
          uncheckedContent={"Using another executable or HKLM"}
          onChange={(v) => setIsHKCUPluginHandler(v)}
        />
        <nx.WithHeader header={"Trigger crash"}>
          <nx.StackPanel orientation={"Horizontal"}>
            <nx.ComboBox
              value={crashKind}
              onChange={(value) => {
                setCrashKind(value);
              }}>
              <nx.ComboBoxItem value={CrashKind.Fatal}>Call Fatal</nx.ComboBoxItem>
              <nx.ComboBoxItem value={CrashKind.Throw}>Throw</nx.ComboBoxItem>
              <nx.ComboBoxItem value={CrashKind.ThrowFromFireAndForget}>Throw from OpenKneeboard::fire_and_forget</nx.ComboBoxItem>
              <nx.ComboBoxItem value={CrashKind.ThrowFromNoexcept}>Throw from noexcept</nx.ComboBoxItem>
              <nx.ComboBoxItem value={CrashKind.ThrowWithTaskFatal}>Throw with this_task::fatal_on_uncaught_exception()</nx.ComboBoxItem>
              <nx.ComboBoxItem value={CrashKind.Terminate}>Call std::terminate</nx.ComboBoxItem>
              <nx.ComboBoxItem value={CrashKind.TaskWithoutThread}>Task with expired owner thread</nx.ComboBoxItem>
              <nx.ComboBoxItem value={CrashKind.TaskWithoutAwait}>Task without `co_await`</nx.ComboBoxItem>
              <nx.ComboBoxItem value={CrashKind.TaskWithDoubleAwait}>Task that is awaited twice</nx.ComboBoxItem>
            </nx.ComboBox>
            <nx.ComboBox
              value={crashLocation}
              onChange={(value) => {
                setCrashLocation(value as typeof crashLocation);
              }}>
              <nx.ComboBoxItem value={CrashLocation.UIThread}>in UI thread</nx.ComboBoxItem>
              <nx.ComboBoxItem value={CrashLocation.MUITask}>in Microsoft.UI task</nx.ComboBoxItem>
              <nx.ComboBoxItem value={CrashLocation.WindowsSystemTask}>in Windows.System task</nx.ComboBoxItem>
            </nx.ComboBox>
            <nx.Button
              onClick={() => {
                if (!confirm("Are you sure you want to crash the app?")) {
                  return;
                }
                native.TriggerCrash(crashKind, crashLocation)
              }}
            >💥 Crash
            </nx.Button>
          </nx.StackPanel>
        </nx.WithHeader>
      </nx.StackPanel>
    </nx.Page>
  )
    ;
}

export default function ({ instanceID }: { instanceID: string }) {
  const native = nativeState(DeveloperToolsSettingsPageNative, instanceID);
  if (native) {
    return (<DeveloperToolsSettingsPage native={native} />);
  } else {
    return <div className={"nativeLoading"}>Loading native data...</div>
  }
}
