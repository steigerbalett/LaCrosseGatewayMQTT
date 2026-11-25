#include "FirmwareInfo.h"
#include "Logger.h"
#include <time.h>

extern Logger logger;

static String NowString() {
  time_t now = time(nullptr);
  if (now < 1600000000) {
    return String();   // no NTP time yet
  }
  struct tm *ti = localtime(&now);
  char buf[20];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M", ti);
  return String(buf);
}

static String Html(String text) {
  text.replace("&", "&amp;");
  text.replace("<", "&lt;");
  text.replace(">", "&gt;");
  text.replace("'", "&#39;");
  return text;
}

void FirmwareInfo::CheckAtBoot(Settings &settings, const char *version) {
  String stored = settings.Get(F("fwVersion"), "");
  if (stored == version) {
    return;
  }
  if (stored.length() > 0) {
    settings.Add(F("fwPrevVersion"), stored);
  }
  settings.Add(F("fwVersion"), version);
  if (settings.Get(F("fwPending"), "") != "1") {
    // not flashed over the web frontend: USB/serial or a first start
    settings.Add(F("fwFile"), stored.length() > 0 ? F("USB/seriell") : F("unbekannt"));
    settings.Add(F("fwTime"), "");
  }
  settings.Add(F("fwPending"), "0");
  settings.Write();
  logger.println(String(F("Firmware ")) + version + F(" (vorher ") + (stored.length() > 0 ? stored : String(F("unbekannt"))) + F(")"));
}

void FirmwareInfo::RecordFlash(const String &source) {
  Settings *s = new Settings();
  s->Read(&logger);
  String name = source;
  int slash = name.lastIndexOf('/');
  if (slash >= 0 && slash < (int)name.length() - 1) {
    name = name.substring(slash + 1);   // URL: only the file name
  }
  if (name.length() > 60) {
    name = name.substring(0, 60);
  }
  name.replace(";", ",");
  s->Add(F("fwFile"), name.length() > 0 ? name : String(F("unbekannt")));
  s->Add(F("fwTime"), NowString());
  s->Add(F("fwPending"), "1");
  s->Write();
  delete s;
}

String FirmwareInfo::BuildRows(Settings &settings) {
  String file = settings.Get(F("fwFile"), "");
  String when = settings.Get(F("fwTime"), "");
  String prev = settings.Get(F("fwPrevVersion"), "");
  String result;
  result += F("<tr><td>Zuletzt geflasht:</td><td>");
  if (file.length() == 0) {
    result += F("&ndash;");
  } else {
    result += F("<code>"); result += Html(file); result += F("</code>");
    if (when.length() > 0) {
      result += F(" am "); result += when;
    }
  }
  result += F("</td></tr><tr><td>Vorherige Version:</td><td>");
  result += prev.length() > 0 ? Html(prev) : String(F("&ndash;"));
  result += F("</td></tr>");
  return result;
}

String FirmwareInfo::BuildRows() {
  Settings *s = new Settings();
  s->Read(&logger);
  String result = BuildRows(*s);
  delete s;
  return result;
}
