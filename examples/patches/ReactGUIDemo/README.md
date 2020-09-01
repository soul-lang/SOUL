## React GUI Demo

**Please note! This is an early alpha demo of this React functionality!
The implementation will probably change quite a bit as we explore it!**

This example shows the use of a React-based GUI in a SOUL patch.

We're using the [Blueprint](https://github.com/nick-thompson/blueprint) library to implement this, so it's not quite a full [React](https://reactjs.org/) or [React native](https://reactnative.dev/) implementation, but has the same architecture, and is much lighter-weight than it would be to embed a full web-browser.

The example's javascript source files are in the `react/src` folder, and a pre-built version of the `react/build/main.js` file is included in the repo to allow you to see what the example looks like without needing to install all of the react dependencies required to build it.

In order to actually build `main.js` from the source files yourself, and to build your own patches with a React GUI, you'll need to get hold of the Blueprint repo and use `npm` to clutter your machine with the thousands of random dependencies that all Javascript projects inevitably drag along. For more instructions on how to take that journey, see the [Blueprint Setup Guide](https://nick-thompson.github.io/blueprint/#/Getting_Started).

#### Providing a React GUI for a patch

To tell a host that your patch has a React GUI, it's as simple as adding a manifest entry that points to the built javascript file, e.g.

        "view":  "react/build/main.js"

The `soul play` command tool will do its best to open the file. It will also watch for changes, so that if `npm start` is running in your javascript folder and the `soul play` tool is running, it will live-rebuild the GUI as you modify the code.

#### Javascript API

As the API becomes more stable, we'll release a proper document about the javascript functions that are provided to let your GUI code interact with the patch and host. But for the moment, adventurous explorers can see the implementations of the function bindings in the [soul_patch_Blueprint.h](../../../include/soul/patch/helper_classes/soul_patch_Blueprint.h) file.
