"""
pre_patch_yield.py - Pre-Build-Patch fuer ESP8266 Arduino Core 3.1.2

Problem:
  ESP8266 SDK 2.2.2-dev deaktiviert kurzzeitig Interrupts beim WiFi-AP-Betrieb
  (Beacon-Uebertragung, RF-Kalibrierung). Wird in diesem Fenster yield() aufgerufen,
  prüft __yield() ob Interrupts aktiviert sind:
    ETS_INTR_ENABLED() == false → panic() → Bootloop

  Der Crash tritt unabhaengig von ESP8266WebServer oder ESPAsyncWebServer auf,
  immer wenn yield()/delay() nach softAP() waehrend deaktivierten Interrupts laeuft.

Fix:
  panic() aus core_esp8266_main.cpp entfernen.
  __yield() kehrt zurueck wenn Interrupts deaktiviert – kein Datenverlust,
  da der SDK die Interrupts nach der RF-Operation sofort re-aktiviert.
"""
Import("env")
import os, glob, re

MARKER = "PATCHED_NO_YIELD_PANIC_SDK222"

pio_home = os.path.expanduser("~/.platformio")
build_dir = env.subst("$BUILD_DIR")

search_paths = [
    os.path.join(pio_home, "packages",
                 "framework-arduinoespressif8266",
                 "cores", "esp8266", "core_esp8266_main.cpp"),
]
for p in glob.glob(os.path.join(pio_home, "packages",
                                "framework-arduinoespressif8266*",
                                "cores", "esp8266", "core_esp8266_main.cpp")):
    if p not in search_paths:
        search_paths.append(p)

core_file = None
for p in search_paths:
    if os.path.exists(p):
        core_file = p
        break

if not core_file:
    print("[pre_patch_yield] WARNING: core_esp8266_main.cpp nicht gefunden")
else:
    with open(core_file, "r") as f:
        content = f.read()

    if MARKER in content:
        print("[pre_patch_yield] Bereits gepatcht – OK")
    else:
        # Exakter String-Match fuer framework-arduinoespressif8266 3.1.2
        OLD_EXACT = (
            "void IRAM_ATTR __yield() {\n"
            "    if (ETS_INTR_ENABLED()) {\n"
            "        esp_yield();\n"
            "    }\n"
            "    else {\n"
            "        panic();\n"
            "    }\n"
            "}"
        )

        # Regex-Fallback fuer Whitespace-Varianten
        PAT = re.compile(
            r"void\s+IRAM_ATTR\s+__yield\s*\(\s*\)\s*\{"
            r"\s*if\s*\(\s*ETS_INTR_ENABLED\s*\(\s*\)\s*\)\s*\{"
            r"\s*esp_yield\s*\(\s*\)\s*;\s*\}"
            r"\s*else\s*\{\s*panic\s*\(\s*\)\s*;\s*\}"
            r"\s*\}",
            re.DOTALL
        )

        NEW = (
            "// " + MARKER + "\n"
            "void IRAM_ATTR __yield() {\n"
            "    if (ETS_INTR_ENABLED()) {\n"
            "        esp_yield();\n"
            "    }\n"
            "    // Kein panic(): SDK 2.2.2-dev deaktiviert Interrupts kurz\n"
            "    // waehrend WiFi-AP-Beacon. Stillschweigend zurueckkehren.\n"
            "}"
        )

        patched = None
        method = None
        if OLD_EXACT in content:
            patched = content.replace(OLD_EXACT, NEW, 1)
            method = "exact"
        elif PAT.search(content):
            patched = PAT.sub(NEW, content, count=1)
            method = "regex"

        if patched:
            with open(core_file, "w") as f:
                f.write(patched)
            os.utime(core_file, None)

            obj = os.path.join(build_dir, "FrameworkArduino",
                               "core_esp8266_main.cpp.o")
            if os.path.exists(obj):
                os.remove(obj)
                print("[pre_patch_yield] Build-Cache invalidiert: " + obj)

            print("[pre_patch_yield] ERFOLG (" + method + "): " + core_file)
            print("[pre_patch_yield] panic() entfernt – WiFi-AP-Bootloop behoben")
        else:
            print("[pre_patch_yield] FEHLER: Muster nicht gefunden in " + core_file)
