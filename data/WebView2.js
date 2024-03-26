console.log("OpenKneeboard integration attached");

class OpenKneeboard {
    constructor() {
        console.log("OpenKneeboard::Constructor()");
        document.addEventListener('DOMContentLoaded', this.OnDOMContentLoaded.bind(this));
    }

    OnDOMContentLoaded() {
        document.body.classList.add("OpenKneeboard", "OpenKneeboard_WebView2")
    }

    SetPreferredPixelSize(width, height) {
        window.chrome.webview.postMessage({
            message: "OpenKneeboard/SetPreferredPixelSize",
            data: { width, height },
        });
    }
}