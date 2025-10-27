# Τύχη: Zufall durch Zerfall

Diese Repository stellt den Quellcode des Jugend forscht 2026 Projekts von Piro B. und Benedikt M. zur Verfügung.

## Aufbau
Der Quellcode dieses Projekts ist hauptsächlich in `server/src/I2B.h` zu finden. `server/src/server.cpp` beinhält einen HTTP-Server, mit
welchem man Bytes abfragen kann. Der server kann durch `sudo make run` in dem `server` Verzeichnis auf einem Raspberry Pi gestartet werden
kann.

`include/` enthält C++-Bibliotheken.
