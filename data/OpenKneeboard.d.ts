/*
 * OpenKneeboard API - ISC License
 *
 * Copyright (C) 2022-2023 Fred Emmott <fred@fredemmott.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * ----------------------------------------------------------------
 * ----- THE ABOVE LICENSE ONLY APPLIES TO THIS SPECIFIC FILE -----
 * ----------------------------------------------------------------
 *
 * The majority of OpenKneeboard is under a different license; check specific
 * files for more information.
 */

declare interface OpenKneeboardAPIError extends Error {
    apiMethodName: string;
}

declare namespace OpenKneeboard {
    export interface VersionComponents {
        Major: number;
        Minor: number;
        Patch: number;
        Build: number;
    }

    export interface Version {
        Components: VersionComponents;
        HumanReadable: string;
        IsGitHubActionsBuild: boolean;
        IsTaggedVersion: boolean;
        IsStableRelease: boolean;
    }

    interface APIEventsMap {
        /* This is empty in the base interface, but extended by experimental
        * features. */
    }

    interface APIEventHandler<T extends keyof APIEventsMap> {
        (event: APIEventsMap[T]): any;
    }

    interface APIVoidHandler {
        (): any;
    }

    export interface API extends EventTarget {

        GetVersion(): Promise<Version>;

        OpenDeveloperToolsWindow(): Promise<any>;
        SetPreferredPixelSize(width: number, height: number): Promise<any>;

        EnableExperimentalFeature(name: string, version: number): Promise<any>;
        EnableExperimentalFeatures(features: [{ name: string, version: number }]): Promise<any>;

        addEventListener<E extends keyof APIEventsMap>(e: E, listener: APIEventHandler<E> | APIVoidHandler, options?: boolean | AddEventListenerOptions): void;
        removeEventListener<E extends keyof APIEventsMap>(e: E, listener: APIEventHandler<E> | APIVoidHandler, options?: boolean | EventListenerOptions): void;
    }
}

declare var OpenKneeboard: OpenKneeboard.API;