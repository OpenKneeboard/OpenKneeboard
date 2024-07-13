console.log("OpenKneeboard integration attached");

class OpenKneeboard {
    constructor(runtimeData) {
        console.log("OpenKneeboard::Constructor()", runtimeData);
        document.addEventListener('DOMContentLoaded', this.OnDOMContentLoaded.bind(this));
        this.#runtimeData = runtimeData;
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

    GetVersion() {
        return this.#runtimeData.Version;
    }

    #runtimeData;
}