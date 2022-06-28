# Magic URIs

OpenKneeboard recognizes several magic URIs in PDF documents; these were added to support the 'Quick Start' PDF, but can be
used in hyperlinks in any PDF document.

Currently the following are supported:

* `openkneeboard:///Settings/Games`: Open Settings -> Games
* `openkneeboard:///Settings/Input`: Open Settings -> Input
* `openkneeboard:///Settings/Tabs`: Open Settings -> Tabs

Others can be added to `MainWindow::LaunchOpenKneeboardURI()` if needed.
