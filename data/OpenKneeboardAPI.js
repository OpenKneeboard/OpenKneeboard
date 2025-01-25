/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

class OpenKneeboardAPI extends EventTarget {
    constructor() {
        const runtimeData = OpenKneeboardNative.initializationData;
        console.log(`Initializing OpenKneeboard API (${runtimeData.Version.HumanReadable})`);
        super();

        this.#runtimeData = runtimeData;

        if (runtimeData.PeerInfo) {
            const info = runtimeData.PeerInfo.ThisInstance;
            console.log(`OpenKneeboard: This browser instance is for view {${info.ViewGUID}} - "${info.ViewName}"`);
        }

        if (runtimeData.AvailableExperimentalFeatures) {
            var message = "OpenKneeboard: the following experimental features are available in this build:"
            for (const feature of runtimeData.AvailableExperimentalFeatures) {
                message += `\n🧪 '${feature.Name}' version ${feature.Version}`
            }
            console.log(message);
        }

        if (!runtimeData.Version.IsGitHubActionsBuild) {
            console.log("OpenKneeboard RuntimeData", runtimeData);
        }

        for (const [vhost, path] of Object.entries(runtimeData.VirtualHosts)) {
            if (window.location.href.startsWith('https://' + vhost + '/')) {
                console.log(`OpenKneeboard: ℹ️ this plugin is being served from the virtual host '${window.location.href}', which is mapped to '${path}'`);
            }
            break;
        }
    }

    SetPreferredPixelSize(width, height) {
        return this.#AsyncRequest("SetPreferredPixelSize", width, height);
    }

    async GetVersion() {
        return this.#runtimeData.Version;
    }

    EnableExperimentalFeature(name, version) {
        return this.EnableExperimentalFeatures([{ name, version }]);
    }

    async EnableExperimentalFeatures(features) {
        const ret = await this.#AsyncRequest("EnableExperimentalFeatures", features);
        if (!(ret.details?.features)) {
            return ret;
        }
        var message = "OpenKneeboard: the following experimental features have been enabled by scripts in this page:";
        for (const feature of ret.details.features) {
            message += `\n🧪 ✅ '${feature.Name}' version ${feature.Version}`;
        }
        console.log(message);

        return ret;
    }

    #runtimeData;

    #AsyncRequest(name, ...args) {
        return OpenKneeboardNative.asyncRequest(name, ...args);
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

    #RequestPageChange(guid) {
        return this.#AsyncRequest("OpenKneeboard.RequestPageChange", guid);
    }

    #SendMessageToPeers(message) {
        return this.#AsyncRequest("OpenKneeboard.SendMessageToPeers", { message });
    }

    #ActivateAPI(api) {
        switch (api) {
            case "SetCursorEventsMode":
                this.SetCursorEventsMode = this.#SetCursorEventsMode;
                return;
            case "PageBasedContentWithRequestPageChange":
                this.RequestPageChange = this.#RequestPageChange;
            // fallthrough
            case "PageBasedContent":
                this.SetPages = this.#SetPages;
                this.GetPages = this.#GetPages;
                this.SendMessageToPeers = this.#SendMessageToPeers;
                return;
        }
    }

    #OnNativeMessage(message, ...args) {
        console.log("native message:", { message, args });
        /*
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
            case "okb/asyncResponse":
        }
                */
    }
}