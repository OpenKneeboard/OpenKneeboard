class OpenKneeboardSimHubHooks {
  constructor() {
    console.log("OpenKneeboardSimHubHooks::Constructor()");
    window.addEventListener(
      "onmainsimhubdashmetadata",
      this.#onMetadata.bind(this));
  }

  async #onMetadata(ev) {
    console.log("OpenKneeboard: SimHub metadata hook");
    const width = ev.detail.Width;
    const height = ev.detail.Height;

    console.log(`OpenKneeboard: Requesting resize to ${width}x${height}`);

    try {
      const result = await window.OpenKneeboard.SetPreferredPixelSize(width, height);
      console.log("OpenKneeboard: resized to fit SimHub template", result);
    } catch (error) {
      console.log("OpenKneeboard: failed to resize", error);
    }
  }
}

window.OpenKneeboard.SimHubHooks = new OpenKneeboardSimHubHooks();