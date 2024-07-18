console.log("OpenKneeboard integration attached");

class OpenKneeboard {
    constructor(runtimeData) {
        console.log("OpenKneeboard::Constructor()", runtimeData);

        this.#runtimeData = runtimeData;
        this.#inflightRequests = {};
        this.#nextAsyncCallID = 0;

        document.addEventListener('DOMContentLoaded', this.OnDOMContentLoaded.bind(this));
        window.chrome.webview.addEventListener('message', this.#OnNativeMessage.bind(this));
    }

    OnDOMContentLoaded() {
        document.body.classList.add("OpenKneeboard", "OpenKneeboard_WebView2")
    }

    SetPreferredPixelSize(width, height) {
        return this.#AsyncRequest(
            "OpenKneeboard/SetPreferredPixelSize",
            { width, height },
        );
    }

    GetVersion() {
        return this.#runtimeData.Version;
    }

    #runtimeData;
    #inflightRequests;
    #nextAsyncCallID;

    #AsyncRequest(messageName, messageData) {
        const callID = this.#nextAsyncCallID++;
        return new Promise((resolve, reject) => {
            this.#inflightRequests[callID] = { resolve, reject };
            window.chrome.webview.postMessage({ callID, messageName, messageData });
        });
    }

    #OnNativeMessage(event) {
        const message = event.data;
        if (!('callID' in message)) {
            return;
        }
        const callID = message.callID;
        if (callID in this.#inflightRequests) {
            try {
                if (message.result) {
                    this.#inflightRequests[callID].resolve(message.result);
                } else {
                    this.#inflightRequests[callID].reject(message.error);
                }
            } finally {
                delete this.#inflightRequests[callID];
            }
        }
    }
}