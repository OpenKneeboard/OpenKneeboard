import NativeRequest from "./NativeRequest";

export default class NativeClass {
  constructor(id: string) {
    this.#instanceID = id;
    this.#class = new.target.name;
  }

  readonly #instanceID: string;
  readonly #class: string;

  protected NativePropertyChanged(name: string, value: any): void {
    window.chrome.webview.postMessage({
      message: "NativePropertyChanged",
      class: this.#class,
      instanceID: this.#instanceID,
      propertyName: name,
      propertyValue: value,
    });
  }

  protected InvokeNativeMethod(name: string, args: any[]): void {
    window.chrome.webview.postMessage({
      message: "InvokeNativeMethod",
      class: this.#class,
      instanceID: this.#instanceID,
      methodName: name,
      methodArguments: args,
    })
  }

  protected static async GetInstanceJSONData(klass: string, instanceID: string) {
    return await NativeRequest.Send("NativeObjectToJSON", {class: klass, instanceID});
  }
}