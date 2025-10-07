// La forma en que el sitio web funciona actualmente es la siguiente:
// - Aparece un input para escribir el puntoDeVenta y hay que presionar logIn.
//   La página procede a comprobar que el puntoDeVenta sea válido y, en caso de serlo,
//   procede al siguiente punto.
// - El input para puntoDeVenta desaparece y en su lugar aparece un input para la contraseña.
//   Dicha contraseña suele aparecer ya rellenada por el navegador, por lo que no hace falta escribirla.
//   Lo que sí falta hacer es volver a presionar logIn. Esperar un tiempo fijo no parece muy efectivo, por lo
//   que en su lugar se usa un MutationObserver para identificar cuando aparece el campo para la contraseña

console.log('autocompletar-numero');

// #####
// Paso 1, obtener el puntoDeVenta almacenado en firefox y escribirlo en el campo puntoDeVenta
// #####
async function obtenerPuntoVenta() {
    // Wait for storage values to load
    const data = await browser.storage.local.get(["puntoDeVenta"]);

    if (typeof data.puntoDeVenta === "undefined") {
        return "55";
    }

    return data.puntoDeVenta;
}

const autocompletarPuntoDeVenta = (strPuntoDeVenta) => {
    const puntoDeVenta = document.getElementById("in-puntoVenta");

    if (!puntoDeVenta)
        return;

    puntoDeVenta.value = strPuntoDeVenta;
};

obtenerPuntoVenta().then(strPuntoDeVenta => {
    const procesoLogin = () => {
        // strPuntoDeVenta como "closure" permite usar esta función como event
        // callback. Esta función anidada existe por esa misma razon
        autocompletarPuntoDeVenta(strPuntoDeVenta);

        registrarObservadorDOM();
    };

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", procesoLogin);
    } else {
        procesoLogin();
    }
});

// #####
// Paso 2. Enfocar el campo de contraseña
// #####
const registrarObservadorDOM = (_, obs) => {
    const observadorDOM = new MutationObserver((_, obs) => {
        const pass = document.getElementById("in-password");
        if (pass) {
            pass.focus();
            obs.disconnect();
        }
    });

    observadorDOM.observe(document.body, {
        childList: true,
        subtree: true
    });
};
