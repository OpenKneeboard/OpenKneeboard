class OpenKneeboardAPIError extends Error {
    constructor(message, apiMethodName) {
        super(message)
        this.apiMethodName = apiMethodName;
    }

    apiMethodName;
}

class OpenKneeboardAPI extends EventTarget {
    constructor(runtimeData) {
        console.log(`Initializing OpenKneeboard API (${runtimeData.Version.HumanReadable})`);
        super();

        this.#runtimeData = runtimeData;
        this.#inFlightRequests = {};
        this.#nextAsyncCallID = 0;

        document.addEventListener('DOMContentLoaded', this.OnDOMContentLoaded.bind(this));
        window.chrome.webview.addEventListener('message', this.#OnNativeMessage.bind(this));

        if (runtimeData.PeerInfo) {
            const info = runtimeData.PeerInfo.ThisInstance;
            console.log(`OpenKneeboard: This browser instance is for view {${info.ViewGUID}} - "${info.ViewName}"`);
        }

        if (runtimeData.AvailableExperimentalFeatures) {
            var message = "OpenKneeboard: the following experimental features are available in this build:"
            for (const feature of runtimeData.AvailableExperimentalFeatures) {
                message += `\n- '${feature.Name}' version ${feature.Version}`
            }
            console.log(message);
        }

        if (!runtimeData.Version.IsGitHubActionsBuild) {
            console.log("OpenKneeboard RuntimeData", runtimeData);
        }
    }

    OnDOMContentLoaded() {
        document.body.classList.add("OpenKneeboard", "OpenKneeboard_WebView2")
    }

    SetPreferredPixelSize(width, height) {
        return this.#AsyncRequest(
            "OpenKneeboard.SetPreferredPixelSize",
            { width, height },
        );
    }

    GetVersion() {
        return new Promise(
            (resolve, reject) => { resolve(this.#runtimeData.Version); });
    }

    EnableExperimentalFeature(name, version) {
        return this.EnableExperimentalFeatures([{ name, version }]);
    }

    EnableExperimentalFeatures(features) {
        return this.#AsyncRequest("OpenKneeboard.EnableExperimentalFeatures", { features });
    }

    /** WARNING: NOT AVAILABLE IN RELEASED BUILDS.
     * 
     * This function is only available as a tool during development; it **WILL** fail
     * in release builds.
     */
    GetAvailableExperimentalFeatures() {
        console.log("WARNING: OpenKneeboard.GetAvailableExperimentalFeatures() **WILL** fail in tagged releases. It is only for use during development, not to check feature availability.");
        return this.#AsyncRequest("OpenKneeboard.GetAvailableExperimentalFeatures", {});
    }

    #runtimeData;
    #inFlightRequests;
    #nextAsyncCallID;

    #AsyncRequest(messageName, messageData) {
        const callID = this.#nextAsyncCallID++;
        return new Promise((resolve, reject) => {
            this.#inFlightRequests[callID] = { resolve, reject, messageName };
            window.chrome.webview.postMessage({ callID, messageName, messageData });
        });
    }

    #SetCursorEventsMode(mode) {
        return this.#AsyncRequest("OpenKneeboard.SetCursorEventsMode", { mode });
    }

    #GetPages() {
        return this.#AsyncRequest("OpenKneeboard.GetPages", {});
    }

    #SetPages(pages) {
        return this.#AsyncRequest("OpenKneeboard.SetPages", { pages });
    }

    #SendMessageToPeers(message) {
        return this.#AsyncRequest("OpenKneeboard.SendMessageToPeers", { message });
    }

    #ActivateAPI(api) {
        switch (api) {
            case "SetCursorEventsMode":
                this.SetCursorEventsMode = this.#SetCursorEventsMode;
                return;
            case "PageBasedContent":
                this.SetPages = this.#SetPages;
                this.GetPages = this.#GetPages;
                this.SendMessageToPeers = this.#SendMessageToPeers;
                return;
        }
    }

    #OnNativeMessage(event) {
        const message = event.data;
        if (!("OpenKneeboard_WebView2_MessageType" in message)) {
            return;
        }

        switch (message.OpenKneeboard_WebView2_MessageType) {
            case "console.log":
                console.log(...message.logArgs);
                return;
            case "ActivateAPI":
                this.#ActivateAPI(message.api);
                return;
            case "Event":
                this.dispatchEvent(new CustomEvent(message.eventType, message.eventOptions));
                return;
            case "AsyncResponse":
                if (!('callID' in message)) {
                    return;
                }
                const callID = message.callID;
                if (callID in this.#inFlightRequests) {
                    const call = this.#inFlightRequests[callID];
                    try {
                        if (message.result) {
                            call.resolve(message.result);
                        } else {
                            console.log(`⚠️ OpenKneeboard API error: '${call.messageName}' => '${message.error}'`);
                            call.reject(new OpenKneeboardAPIError(message.error, call.messageName));
                        }
                    } finally {
                        delete this.#inFlightRequests[callID];
                    }
                }
        }
    }
}