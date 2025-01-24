var OpenKneeboardNative = new class {
  #initializationData;

  GetInitializationData() {
    native function OKBNative_GetInitializationData();

    if (!this.#initializationData) {
      this.#initializationData = JSON.parse(OKBNative_GetInitializationData());
    }
    return this.#initializationData;
  }
};
