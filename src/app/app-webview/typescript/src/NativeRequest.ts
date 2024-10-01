const NativeRequest = new class {
  constructor() {
    window.chrome.webview.addEventListener('message', this.#OnMessage.bind(this));
  }
  Send(message: string, params: any) {
    let promise = Promise.withResolvers();
    const promiseID = crypto.randomUUID();
    window.chrome.webview.postMessage({message, params, promiseID});
    this.#inFlight[promiseID] = promise;
    return promise.promise;
  }

  #inFlight : Record<string, PromiseWithResolvers<any>> = {};

  #OnMessage(event: any) {
    if (event.data.message != "ResolvePromise") {
      return;
    }
    if (!(event.data.promiseID in this.#inFlight)) {
      return;
    }
    const promise = this.#inFlight[event.data.promiseID];
    delete this.#inFlight[event.data.promiseID];
    if (event.data.success) {
      promise.resolve(event.data.result);
    } else {
      promise.reject(event.data.error);
    }
  }
}

export default NativeRequest;