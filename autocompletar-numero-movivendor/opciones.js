
// Load the saved value
browser.storage.local.get('puntoDeVenta').then(data => {
    if (data.puntoDeVenta) {
        document.getElementById('puntoDeVenta').value = data.puntoDeVenta;
    }
});

document.getElementById('botonGuardar').addEventListener('click', () => {
  const intPuntoDeVenta = parseInt(document.getElementById('puntoDeVenta').value, 10);
  
  if (isNaN(intPuntoDeVenta)) {
    alert("Favor de ingresar un número válido.");
    return;
  }

  browser.storage.local.set({ puntoDeVenta: intPuntoDeVenta.toString() });
});