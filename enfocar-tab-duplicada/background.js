console.log('[ETD] Loaded Add-on');

TARGET_URLS = [];

function stringPathsToUrls(paths) {
    res = [];
    for (const p of paths) {
        try {
            u = new URL(p);
            res.push(u)
        } catch {
            try {
                u = new URL('https://' + p);
                res.push(u)
            } catch {
                console.log(`[ETD] Failed to parse ${p}`)
            }
        }
    }
    return res;
}

function loadTargetUrls() {
    browser.storage.local.get('hosts').then(result => {
        if (result.hosts && Array.isArray(result.hosts)) {
            TARGET_URLS = stringPathsToUrls(result.hosts);
        }
        console.log('[ETD] Loaded target hosts')
    });
}

// Watch for hosts changes
browser.storage.onChanged.addListener((changes, area) => {
    if (area === "local" && changes.hosts) {
        TARGET_URLS = stringPathsToUrls(changes.hosts.newValue || []);
        console.log('[ETD] Updated target hosts');
    }
});

loadTargetUrls();

// Helper: returns matching host if URL is in target hosts
function idxTargetUrl(url) {
    console.log(TARGET_URLS);
    try {
        const u = new URL(url);

        for (let i = 0; i < TARGET_URLS.length; ++i) {
            const t = TARGET_URLS[i];
            if (t.host !== u.host)
                continue;

            if (t.pathname === "/" || t.pathname === "")
                return i;

            if (u.pathname.startsWith(t.pathname))
                return i;

            // Not true ==> continue checking the next TARGET_URL
        }
    } catch (e) {
        console.log(`[ETD] Invalid url ${url}`)
    }
    return -1;
}

browser.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
    if (!changeInfo.url) return;

    console.log(`[ETD] Tab updated with URL: ${changeInfo.url}`);

    const idx = idxTargetUrl(changeInfo.url);
    if (idx === -1) return;

    console.log('[ETD] Target detected');


    browser.tabs.query({}).then(tabs => {
        const existing = tabs.find(t => {
            try {
                return t.id !== tabId && idxTargetUrl(t.url) === idx;
            } catch (e) { return false; }
        });


        if (existing) {
            console.log('[EDT] Tab already exists. Focusing it');
            browser.tabs.remove(tabId);
            browser.tabs.update(existing.id, {active: true});
            browser.windows.update(existing.windowId, {focused: true});
        } else {
            console.log('[EOT] Tab not exists. Letting the new one open');
        }
    });
});
