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

    export interface API {
        GetPages(): Promise<{
            havePages: boolean,
            pages?: [Page],
        }>;

        SetPages(pages: [Page]): Promise<any>;

        SendMessageToPeers(message: any): Promise<any>;
    }
}