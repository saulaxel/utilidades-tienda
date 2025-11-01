function saveOptions() {
    const lines = document.getElementById("hosts").value
        .split("\n")
        .map(l => l.trim())
        .filter(l => l);

    browser.storage.local.set({hosts: lines}).then(() => {
        const status = document.getElementById("status");
        status.textContent = "Saved!";
        setTimeout(() => { status.textContent = ""; }, 1500);
    });
}

function loadOptions() {
    browser.storage.local.get("hosts").then(result => {
        const hosts = result.hosts || [];
        document.getElementById("hosts").value = hosts.join("\n");
    });
}

document.getElementById("save").addEventListener("click", saveOptions);
document.addEventListener("DOMContentLoaded", loadOptions);
