class OpenKneeboardSimHubHooks {
  constructor() {
    console.log("OpenKneeboardSimHubHooks::Constructor()");
    this.#hookWindowSimhub();
  }

  #simhub;
  #simhub_server;
  #simhub_server_notify;

  #hookWindowSimhub() {
    Object.defineProperty(
      window,
      'simhub',
      {
        get: () => {
          return this.#simhub;
        },
        set: (value) => {
          this.#simhub = value;
          this.#hookSimhubServer();
        }
      });
  }

  #hookSimhubServer() {
    Object.defineProperty(
      this.#simhub,
      'server',
      {
        get: () => {
          return this.#simhub_server;
        },
        set: (value) => {
          this.#simhub_server = value;
          this.#hookSimHubServerNotifyMainTemplateLoaded();
        },
      });
  }

  #hookSimHubServerNotifyMainTemplateLoaded() {
    Object.defineProperty(
      this.#simhub_server,
      'notifyMainTemplateLoaded',
      {
        get: () => {
          if (this.#simhub_server_notify) {
            return this.#simhubNotifyMainTemplateLoaded.bind(this);
          }
          return undefined;
        },
        set: (value) => {
          this.#simhub_server_notify = value;
        },
      });
  }

  #simhubNotifyMainTemplateLoaded() {
    this.#onNotifyMainTemplateLoaded();
    return this.#simhub_server_notify();
  }

  async #onNotifyMainTemplateLoaded() {
    console.log("OpenKneeboard: SimHub notifyMainTemplateLoaded hook");
    const width = $(".maincontainer").width();
    const height = $(".maincontainer").height();

    try {
      const result = await window.OpenKneeboard.SetPreferredPixelSize(width, height);
      console.log("OpenKneeboard: resized to fit SimHub template", result);
    } catch (error) {
      console.log("OpenKneeboard: failed to resize", error);
    }
  }
}

window.OpenKneeboard.SimHubHooks = new OpenKneeboardSimHubHooks();