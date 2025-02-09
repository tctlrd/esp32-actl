function handleBoard() {
    var bType = parseInt(document.getElementById("boardtype").value)
    if (bType === 1) { // generic
      document.getElementById("numrlys").value = "1";
      document.getElementById("changenumrly").click();
      document.getElementById("gpiorly"). value = "16";
      document.getElementById("accessdeniedpin").value = "255";
      document.getElementById("beeperpin").value = "13";
      document.getElementById("doorstatpin").value = "14";
      document.getElementById("ledwaitingpin").value = "12";
      document.getElementById("openlockpin").value = "15";
      document.getElementById("wifipin").value = "255";
      document.getElementById("wg0pin").value = "4";
      document.getElementById("wg1pin").value = "5";
    } else if (bType === 2) { // Olimex ESP32-POE2 w/ MOD2-IO
      document.getElementById("numrlys").value = "1";
      document.getElementById("changenumrly").click();
      document.getElementById("gpiorly"). value = "16";
      document.getElementById("accessdeniedpin").value = "255";
      document.getElementById("beeperpin").value = "13";
      document.getElementById("doorstatpin").value = "17";
      document.getElementById("ledwaitingpin").value = "14";
      document.getElementById("openlockpin").value = "39";
      document.getElementById("wifipin").value = "255";
      document.getElementById("wg0pin").value = "33";
      document.getElementById("wg1pin").value = "26";
    }
  }