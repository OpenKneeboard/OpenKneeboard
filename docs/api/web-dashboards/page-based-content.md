---
parent: Web Dashboard APIs
---

# Page-Based Web Applications
{: .no_toc }

**THIS IS AN EXPERIMENTAL FEATURE** and may change between any two versions without notice; feedback and questions are very welcome in `#code-talk` [on Discord](https://go.openkneeboard.com/Discord).

This experimental API allows a web page to work like a native OpenKneebaord tab type with multiple pages of varying sizes (like a PDF document), instead of a potentially-scrollable content that must fit a variable-sized browser.

## Table of Contents
{: .no_toc, .text-delta }

1. TOC
{:toc}

## Requirements

This feature requires:
- OpenKneeboard v1.9 or above
- the experimental feature `PageBasedContent` version `2024072001`

## Model

Page-based web applications in OpenKneeboard are essentially a [Single Page Application (SPA)](https://developer.mozilla.org/en-US/docs/Glossary/SPA), with several key differences:

- like a PDF, the document is formed of multiple discrete pages, rather than a continuously-scrollable canvas in a standard web page
- like a PDF, every page in the document can be a different size and aspect ratio, rather than fitting the size of the browser window
- OpenKneeboard-specific APIs must be used
- If the user has multiple views (e.g. 'dual kneeboards'), each view will have it's own browser instance running your page. This is similar to having the same page open in multiple browsers at the same time - there is no implicit synchronization, however, standard techniques like local storage and websockets are available. Additionally, OpenKneeboard provides a basic message-passing API.

Multiple instances of your app - corresponding to multiple OpenKneeboard views - are called 'peers' in this API.

For this API, pages are identified by [GUIDs, a.k.a. UUIDs](https://developer.mozilla.org/en-US/docs/Glossary/UUID), not by page number or index; this is a little more work to get started with, but avoids complicated issues if you choose to remove pages, or insert pages other than at the end.

### GUIDs

- OpenKneeboard accepts GUIDs with or without braces, e.g. `12c5b710-bae4-4c60-b5dc-1a8c32a0797a` and `{12c5b710-bae4-4c60-b5dc-1a8c32a0797a}` are considered equivalent
- For the JS APIs, OpenKneeboard will *always* return GUIDs without braces, e.g. `12c5b710-bae4-4c60-b5dc-1a8c32a0797a`. This is to allow easy interoperability with `Crypto.randomUUID()`

Within a single 'web dashboard' tab:
- a specific page must have the same GUID in all browser instances; this is managed via [the `GetPages()` and `SetPages()` APIs](#get-or-set-the-page-ids) and [`pagesChanged` event](#subscribe-to-the-events)
- each page must have a unique GUID

If your page is added as multiple web dashboard tabs, these instances are isolated, and there is no need for page GUIDs to be unique between multiple web dashboard instances.

In practical terms:

- if you have a fixed set of pages, you can hard-code GUIDs - but they should be unique to your project, i.e. freshly generated at random while you are writing your code
- if you have a variable number of pages (e.g. if you implemented something like the 'endless notebook tabs), you can use `Crypto.randomUUID()` to generate a unique ID for each page - but you must use the `GetPages()` API to re-use the same GUID between multiple browser instances in the same web dashboard
- you *can* also use the `Crypto.randomUUID()` approach if you have a fixed set of pages; hard-coding may be simpler but isn't required

## Flow

1. [Enable the feature](#enabling-the-feature)
2. [Subscribe to the events](#subscribe-to-the-events)
3. [Get or set the page IDs](#get-or-set-the-page-ids)
4. [Adjust displayed content in response to events](#showing-the-correct-content)

## Enabling the feature

As this is an experimental feature, it may change without notice, and it must be explicitly enabled before use:

```js
try {
    await OpenKneeboard.EnableExperimentalFeatures([
        { name: 'PageBasedContent', version: 2024072001 },
    ]);
} catch (ex) {
    FallbackWithoutUsingThisFeature();
}
```

## Subscribe to the events

You should subscribe to the events immediately after enabling the feature - especially before making any `SetPages()` or `GetPages()` API calls.

```js
OpenKneeboard.addEventListener('pageChanged', MyPageChangedCallback);
OpenKneeboard.addEventListener('pagesChanged', MyPagesChangedCallback);
OpenKneeboard.addEventListener('peerMessage', MyPeerMessageCallback);
```

The `pageChanged` event is always required; the other two events will only be required by some implementations.

For details, see:
- [pageChanged](#the-pagechanged-event)
- [pagesChanged](#the-pageschanged-event)
- [peerMessage](#the-peerMessage-event)

## Get or set the page IDs

A page is defined with an object of this form:

```js
{
    guid: string,
    pixelSize: { width: integer, height: integer },
    extraData: any,
}
```

- `guid` must be a valid GUID. Brackets may be included, but they will be stripped in `GetPages()` and related events.
- `width` and `height` must be whole numbers that are greater than or equal to 1
- `extraData` is optional, and can contain any *JSON-encodable* value. It will not be processed by OpenKneeboard, but will be returned verbatim in the various other APIs. This is useful if you want to internally identify pages by something other than a GUID - for example, an image filename

If the user has configured multiple views in OpenKneeboard, there may be multiple browsers running your application, and you must make sure that the page IDs are shared. For this reason, you should *always* call `GetPages()` before `SetPages()`, and use the pre-existing IDs if any. You should *only* call `SetPages()` if `GetPages()` did not return any page IDs - this implies that the current brower instance is the first browser instance for your application.

### GetPages

`OpenKneeboard.GetPages()` returns a `Promise`, which resolves to a structure of this form:

```js
{
    havePages: Boolean,
    ?pages: array<Page>
}
```

The `pages` key is present if `havePages` is `true`; you should check `havePages` before checking the `pages` key.

### SetPages

`SetPages()` takes an array of pages, and returns a `Promise<any>`.

You may call `SetPages()` multiple times, e.g. to append, insert, remove, or replace pages.

### Example: Combining GetPages and SetPages

```js
const existingPages = await OpenKneeboard.GetPages();
if (existingPages.havePages) {
    MyApp_InitializeWithPageIDs(existingPages.pages);
} else {
    const pages = [
        {
            guid: crypto.randomUUID(),
            pixelSize: { width: 1024, height: 768 },
        }
    ];
    await OpenKneeboard.SetPages(pages);
    MyApp_InitializeWithPageIDs(pages);
}
```

Your app may want to initalize pages once it have the IDs, store a mapping between existing DOM elements and page IDs, or any other approach that makes sense for your app.

## Showing the correct content

When the page is changed in a view, OpenKneeboard will send [a `pageChanged` event](#the-pagechanged-event); you should show/hide/otherwise-update what your application displays in response to this event; for example:

```js
async function OnPageChanged(ev) {
    const guid = ev.detail.page.guid;
    for (const pageDiv of document.getElementsByClassName('page')) {
        pageDiv.classList.toggle('hidden', guid == pageDiv.id);
    }
}
OpenKneeboard.addEventListener('pageChanged', OnPageChanged);
```

## The pageChanged event

This event on the `OpenKneeboard` object is a [`CustomEvent`](https://developer.mozilla.org/en-US/docs/Web/API/CustomEvent); the `detail` property of the event contains a `page` element, containing a page with `guid`, `pixelSize`, and optional `extraData`, as provided in a call to `SetPage()` by this instance of your application or a peer instance.

## The pagesChanged event

This event on the `OpenKneeboard` object is a `CustomEvent`; the `detail` property contains an object with a `pages` property, which is an array of page structures, each with `guid`, `pixelSize`, and `extraData`.

You most likely do not need to handle this event unless your application calls `SetPages()` multiple times.

## Communicating between peer instances

OpenKneeboard does not provide any implicit synchronization between peer instances - if any synchronization is required, you must do this in your application code.

Some possible approaches include:
- standard web technologies like websockets and local storage
- [`OpenKneeboard.SendMessageToPeers()`](#sendmessagetopeers) combined with [the `peerMessage` event](#the-peermessage-event)

### SendMessageToPeers

This function sends a message to all peer instances, which they can choose to receive by listening for [the `peerMessage` event](#the-peermessage-event).

```js
// Syntax:
OpenKneeboard.SendMessageToPeers(message: any): Promise<any>;
// Example:
await OpenKneeboard.SendMesageToPeers("hello, world");
```

- The `message` parameter is any JSON-encodable value.
- This is a 'fire-and-forget' function; no indication is given whether or not peers received the message, or encountered any issues

### The peerMessage event

This is a `CustomEvent`; the `detail` property contains an object with a `message` property, containing the message that was passed to `SendMessageToPeers()`, verbatim.

```js
// Example:
OpenKneeboard.addEventListener(
  'peerMessage',
  (ev) => { console.log(ev.detail.message); }
);
```

## A full example

`api-example-PageBasedContent.html` is included in:

- [the `data` subfolder of the OpenKneeboard source tree](https://openkneeboard.com/api/web-dashboards/page-based-content.html#communicating-between-peer-instances)
- the `share\doc\` subfolder of an OpenKneeboard installation