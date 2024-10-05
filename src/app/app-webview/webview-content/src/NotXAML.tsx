import * as React from "react";
import { ReactNode } from "react";

export function Page({ children }: { children: React.ReactNode }) {
  return <div className="nxPage">{children}</div>;
}

export function Title({ children }: { children: React.ReactNode }) {
  return (
    <h1 className="nxTitle">{children}</h1>
  );
}

interface StackPanelProps {
  orientation: "Horizontal" | "Vertical";
  children: React.ReactNode;
  spacing?: number;
}

export function StackPanel({ orientation, children, spacing }: StackPanelProps) {
  let style: React.CSSProperties = {};
  if (spacing) {
    style.gap = `${spacing}px`;
  }

  return (
    <div
      className={`nxStackPanel nxStackPanel_${orientation}`}
      style={style}>
      {children}
    </div>);
}

export function Caption({ children }: { children: React.ReactNode }) {
  return <div className="nxCaption">{children}</div>;
}

export function Button(props: React.JSX.IntrinsicAttributes & React.ClassAttributes<HTMLButtonElement> & React.ButtonHTMLAttributes<HTMLButtonElement>) {
  let { className, ...rest } = props;
  className = ["nxButton", className].join(' ');
  return <button className={className} {...rest} />;
}

interface TextBoxProps extends Omit<React.HTMLProps<HTMLInputElement>, "onChange"> {
  onChange: (value: string) => void;
  header?: ReactNode;
}

export function TextBox({ onChange, className, header, ...rest }: TextBoxProps) {
  className = [className, "nxTextBox nxInput nxWidget"].join(' ');
  return (
    <WithOptionalHeader header={header}>
      <input
        type="text"
        className={className}
        onChange={(e) => {
          if (onChange) {
            onChange(e.target.value);
          }
        }}
        {...rest}
      /></WithOptionalHeader>
  );
}

interface ComboBoxItemProps<T extends string> {
  value: T;
  children: string;
}

export function ComboBoxItem<T extends string>({ value, children }: ComboBoxItemProps<T>) {
  return (
    <option value={value}>{children}</option>
  );
}

interface ComboBoxProps<T extends string> extends Omit<React.HTMLProps<HTMLSelectElement>, "onChange" | "value" | "initialValue" | "children"> {
  onChange: (value: T) => void;
  value?: T;
  initialValue?: T;
  children:
  React.ReactElement<ComboBoxItemProps<T>> |
  React.ReactElement<ComboBoxItemProps<T>>[];
}

export function ComboBox<T extends string>({ onChange, className, children, ...rest }: ComboBoxProps<T>) {
  className = [className, "nxComboBox nxButton nxWidget"].join(' ');
  return (
    <select
      className={className}
      onChange={(e) => {
        if (onChange) {
          onChange(e.target.value as T);
        }
      }}
      {...rest}>
      {children}
    </select>
  );
}

/* Using 'Checked' instead of MUXC-style 'On' to:
 * - be consistent with HTML/JS
 * - avoid 'OnContent' sounding like an event handler
 */
interface ToggleSwitchProps {
  isChecked: boolean,
  onChange?: (checked: boolean) => void,
  checkedContent?: React.ReactNode,
  uncheckedContent?: React.ReactNode,
  header?: React.ReactNode,
}

export function ToggleSwitch(props: ToggleSwitchProps) {
  const { isChecked, onChange, checkedContent, uncheckedContent, header } = props;
  return (
    <WithOptionalHeader header={header}>
      <div
        tabIndex={0}
        className={`nxToggleSwitch nxToggleSwitch_${isChecked ? '' : 'un'}checked`}
        onClick={() => {
          if (onChange) {
            onChange(!isChecked);
          }
        }}
        onKeyUp={(e) => {
          if (onChange && e.code == 'Space') {
            onChange(!isChecked);
          }
        }}
      >
        <div className={"nxToggleSwitch_control"}>
          <div className={"nxToggleSwitch_slider"} />
        </div>
        <div className={"nxToggleSwitch_content"}>
          {isChecked ? checkedContent : uncheckedContent}
        </div>
      </div>
    </WithOptionalHeader>
  );
}

// See:
// - Windows 10: https://learn.microsoft.com/en-us/windows/apps/design/style/segoe-ui-symbol-font
// - Windows 11: https://learn.microsoft.com/en-us/windows/apps/design/style/segoe-fluent-icons-font
//
// Most (all?) Window 10 glyphs are also in the Windows 11 font; use those :)
//
// Recommended flow:
// 1. Find something you want in the Windows 10 list
// 2. Check it also exists at the same code point and is reasonable in the Windows 11 list
// 3. Use the name from the Windows 10 list if they differ
const GlyphMapping = {
  Copy: "\ue8c8",
  OpenFolderHorizontal: "\ued25",
}
export type Glyph = keyof typeof GlyphMapping;

export function FontIcon({ glyph }: { glyph: Glyph }) {
  return (
    <span className={"nxFontIcon"}>{GlyphMapping[glyph]}</span>
  );
}

interface WithOptionalHeaderProps {
  header?: ReactNode;
  children: ReactNode;
}

export function WithOptionalHeader({ header, children }: WithOptionalHeaderProps) {
  if (!header) {
    return children;
  }
  return (
    <StackPanel orientation={"Vertical"} spacing={8}>
      <Caption>{header}</Caption>
      <div>{children}</div>
    </StackPanel>
  );
}

interface WithHeaderProps extends WithOptionalHeaderProps {
  header: ReactNode;
}

export function WithHeader(props: WithHeaderProps) {
  return WithOptionalHeader(props);
}