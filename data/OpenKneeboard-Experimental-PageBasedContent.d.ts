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

declare namespace OpenKneeboard {
    export interface Page {
        guid: string,
        pixelSize: { width: number, height: number },
        extraData?: any,
    }

    export interface PageChangedEvent extends CustomEvent {
        detail: { page: Page };
    }

    export interface PagesChangedEvent extends CustomEvent {
        detail: { pages: [Page] };
    }

    export interface PeerMessageEvent<Message> extends CustomEvent {
        detail: Message;
    }

    interface APIEventsMap {
        pageChanged: PageChangedEvent,
        pagesChanged: PagesChangedEvent,
        peerMessage: PeerMessageEvent<any>,
    }

    export interface API {
        GetPages(): Promise<{
            havePages: boolean,
            pages?: [Page],
        }>;

        SetPages(pages: [Page]): Promise<any>;

        SendMessageToPeers(message: any): Promise<any>;

        RequestPageChange(guid: string): Promise<any>
    }
}