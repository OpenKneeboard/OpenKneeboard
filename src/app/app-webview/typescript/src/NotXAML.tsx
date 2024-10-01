import * as React from "react";

export function Page({children}: { children: React.ReactNode }) {
  return <div className="nxPage">{children}</div>;
}

export function Title({children}: { children: React.ReactNode }) {
  return (
    <h1 className="nxTitle">{children}</h1>
  );
}

export function StackPanel({orientation, children}: {
  orientation: "Horizontal" | "Vertical",
  children: React.ReactNode
}) {
  return <div className={`nxStackPanel nxStackPanel_${orientation}`}>{children}</div>;
}

export function Caption({children}: { children: React.ReactNode }) {
  return <div className="nxCaption">{children}</div>;
}

export function Button(props: React.JSX.IntrinsicAttributes & React.ClassAttributes<HTMLButtonElement> & React.ButtonHTMLAttributes<HTMLButtonElement>) {
  let {className, ...rest} = props;
  className = ["nxButton", className].join(' ');
  return <button className={className} {...rest} />;
}

interface TextBoxProps extends Omit<React.HTMLProps<HTMLInputElement>, "onChange"> {
  onChange: (value: string) => void;
}

export function TextBox({onChange, ...rest}: TextBoxProps) {
  return (
    <input
      type="text"
      className="nxTextBox"
      onChange={(e) => {
        if (onChange) {
          onChange(e.target.value);
        }
      }}
      {...rest}
    />
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
}

export function ToggleSwitch({isChecked, onChange, checkedContent, uncheckedContent}: ToggleSwitchProps) {
  return (
    <div
      className={`nxToggleSwitch nxToggleSwitch_${isChecked ? '' : 'un'}checked`}
      onClick={(e) => {
        if (onChange) {
          onChange(!isChecked);
        }
      }}>
      <div className={"nxToggleSwitch_control"}>
        <div className={"nxToggleSwitch_slider"}/>
      </div>
      <div className={"nxToggleSwitch_content"}>
        {isChecked ? checkedContent : uncheckedContent}
      </div>
    </div>
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
export function FontIcon({glyph}:{glyph: Glyph}) {
  return (
    <span className={"nxFontIcon"}>{GlyphMapping[glyph]}</span>
  );
}