console.log("v4");

async function obtenerPuntoVenta() {
  // Wait for storage values to load
  const data = await browser.storage.local.get(["puntoDeVenta"]);

  if (typeof data.puntoDeVenta === "undefined") {
      return "55";
  }
  
  return data.puntoDeVenta;
}

obtenerPuntoVenta().then(strPuntoDeVenta => {
    const autocompletarNumero = () => {
        const puntoDeVenta = document.getElementById("in-puntoVenta");
        const botonLogIn = document.getElementById("logIn");
        
        if (!puntoDeVenta || !botonLogIn)
            return;
        
        puntoDeVenta.value = strPuntoDeVenta;

        if (strPuntoDeVenta.length !== 10)
            return;
        
        setTimeout(() => {
            botonLogIn.click();
        }, 500);        
    };

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", autocompletarNumero);
    } else {
        autocompletarNumero();
    }
});