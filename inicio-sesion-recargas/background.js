browser.browserAction.onClicked.addListener(async (tab) => {
    if (!tab.id || !tab.url) return;

    // Asegurarnos de estar en la página correcta
    if (!tab.url.startsWith("https://web.movivendor.com/login")) {
        console.warn("No es la página de login");
        return;
    }

    try {
        await browser.tabs.executeScript(tab.id, {
            file: "autocompletar-numero.js"
        });
        console.log("Content script inyectado manualmente");
    } catch (err) {
        console.error("Error inyectando content script:", err);
    }
});
