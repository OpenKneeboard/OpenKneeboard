# Page Sources and Tabs

An `IPageSource` represents 0-n pages that are handled the same way; this includes rendering, cursor events, navigation etc. An `ITab` is a user-configurable `IPageSource`, and may be composed/delegate to multiple other `IPageSource`.

An `IPageSource` that is not an `ITab` is generally a low-level re-usable building block; for example:

- `ImageFilePageSource` is used by `DCSBriefingTab` and `FolderPageSource`
- `PlainTextPageSource` is used by `DCSRadioTab`, `DCSBriefingTab`, and `PlainTextFilePageSource`
- `FolderPageSource` is used by `FolderTab`, `DCSAircraftTab`, and several others
- `PlainTextFilePageSource` is used by `TextFileTab`

`FolderTab` and `TextFileTab` differ from `FolderPageSource` and `PlainTextFilePageSource` in that they:

- provide a tab glyph and title
- understand JSON settings
- can be chosen by the user in Settings-> Tabs -> Add Tab
