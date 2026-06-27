(() => {
    "use strict";

    console.log("[autocompletar] script cargado");

    // =========================
    // Configuración / constantes
    // =========================

    const DEFAULT_PUNTO_VENTA = "55";
    const ID_INPUT_PUNTO_VENTA = "in-puntoVenta";
    const ID_INPUT_PASSWORD = "in-password";

    // =========================
    // Storage
    // =========================

    async function obtenerPuntoDeVenta() {
        try {
            const { puntoDeVenta } = await browser.storage.local.get("puntoDeVenta");
            return puntoDeVenta ?? DEFAULT_PUNTO_VENTA;
        } catch (err) {
            console.warn("[autocompletar] error leyendo storage", err);
            return DEFAULT_PUNTO_VENTA;
        }
    }

    // =========================
    // DOM helpers
    // =========================

    function obtenerInputPuntoDeVenta() {
        return document.getElementById(ID_INPUT_PUNTO_VENTA);
    }

    function obtenerInputPassword() {
        return document.getElementById(ID_INPUT_PASSWORD);
    }

    // =========================
    // Paso 1: Autocompletar punto de venta
    // =========================

    async function autocompletarPuntoDeVenta() {
        const input = obtenerInputPuntoDeVenta();
        if (!input) {
            console.debug("[autocompletar] input puntoDeVenta no encontrado");
            return;
        }

        const puntoDeVenta = await obtenerPuntoDeVenta();
        input.value = puntoDeVenta;

        // El estado de la app no se actualiza en automático
        // El siguiente código le informa de los cambios
        input.dispatchEvent(new InputEvent("input", {
          bubbles: true
        }));

        input.dispatchEvent(new Event("change", {
          bubbles: true
        }));

        console.debug("[autocompletar] puntoDeVenta aplicado:", puntoDeVenta);
    }

    // =========================
    // Paso 2: Enfocar password cuando aparezca
    // =========================

    function observarCampoPassword() {
        const existente = obtenerInputPassword();
        if (existente) {
            existente.focus();
            return;
        }

        const observer = new MutationObserver(() => {
            const pass = obtenerInputPassword();
            if (pass) {
                pass.focus();
                observer.disconnect();
                console.debug("[autocompletar] campo password enfocado");
            }
        });

        observer.observe(document.body, {
            childList: true,
            subtree: true
        });
    }

    // =========================
    // Orquestador principal
    // =========================

    async function ejecutarProcesoLogin() {
        await autocompletarPuntoDeVenta();
        observarCampoPassword();
    }

    // =========================
    // Auto-ejecución al cargar
    // =========================

    function iniciarAutomaticamente() {
        if (document.readyState === "loading") {
            document.addEventListener("DOMContentLoaded", ejecutarProcesoLogin, { once: true });
        } else {
            ejecutarProcesoLogin();
        }
    }

    iniciarAutomaticamente();

})();
