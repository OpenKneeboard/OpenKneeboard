class OpenKneeboardSimHubHooks {
  constructor() {
    console.log("OpenKneeboardSimHubHooks::Constructor()");
    this.hookWindowSimhub();
  }

  hookWindowSimhub() {
    Object.defineProperty(
      window,
      'simhub',
      {
        get: () => {
          return this.simhub;
        },
        set: (value) => {
          this.simhub = value;
          this.hookSimhubServer();
        }
      });
  }

  hookSimhubServer() {
    Object.defineProperty(
      this.simhub,
      'server',
      {
        get: () => {
          return this.simhub_server;
        },
        set: (value) => {
          this.simhub_server = value;
          this.hookSimHubServerNotifyMainTemplateLoaded();
        },
      });
  }

  hookSimHubServerNotifyMainTemplateLoaded() {
    Object.defineProperty(
      this.simhub_server,
      'notifyMainTemplateLoaded',
      {
        get: () => {
          if (this.simhub_server_notify) {
            return this.simhubNotifyMainTemplateLoaded.bind(this);
          }
          return undefined;
        },
        set: (value) => {
          this.simhub_server_notify = value;
        },
      });
  }

  simhubNotifyMainTemplateLoaded() {
    this.simhub_server_notify();
    console.log("OpenKneeboard: SimHub notifyMainTemplateLoaded hook");
    const width = $(".maincontainer").width();
    const height = $(".maincontainer").height();

    window.OpenKneeboard.SetPreferredPixelSize(width, height);
  }
}