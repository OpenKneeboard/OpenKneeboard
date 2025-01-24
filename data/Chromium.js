var OpenKneeboardNative = new class {
  #initializationData;

  sendMessage(name, id, data) {
    if (!Number.isInteger(id)) {
      throw "Native message ID must be an integer";
    }
    native function OKBNative_SendMessage();
    OKBNative_SendMessage(`okbjs/${name}`, id, JSON.stringify(data));
  }

  onMessage(callback) {
    native function OKBNative_OnMessage();
    OKBNative_OnMessage(callback);
  }

  get initializationData() {
    native function OKBNative_GetInitializationData();

    if (!this.#initializationData) {
      this.#initializationData = JSON.parse(OKBNative_GetInitializationData());
    }
    return this.#initializationData;
  }
};
