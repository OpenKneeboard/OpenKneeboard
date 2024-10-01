declare class webview extends EventTarget {
  postMessage(data: any): void;
}

declare class chrome {
  webview: webview;
}

interface Window {
  chrome: chrome;
}