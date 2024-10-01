export default class OpenKneeboardNativeClass {
  constructor(id: string) {
    this.#id = id;
    this.#class = new.target.name;
  }

  readonly #id: string;
  readonly #class: string;

  protected NativePropertyChanged(name: string, value: any): void {
    window.chrome.webview.postMessage({
      message: "NativePropertyChanged",
      class: this.#class,
      instanceID: this.#id,
      propertyName: name,
      propertyValue: value,
    });
  }

  protected InvokeNativeMethod(name: string): void {
    window.chrome.webview.postMessage({
      message: "InvokeNativeMethod",
      class: this.#class,
      instanceID: this.#id,
      methodName: name,
    })
  }
}