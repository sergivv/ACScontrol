CREATE TABLE "estados" (
    "mac_dispositivo" TEXT PRIMARY KEY,
    "temp_min" REAL DEFAULT NULL,
    "temp_max" REAL DEFAULT NULL,
    "estacion" TEXT DEFAULT NULL CHECK("estacion" IS NULL OR "estacion" IN ('verano', 'invierno')),
    "estado_caldera" INTEGER DEFAULT NULL CHECK("estado_caldera" IN (0, 1)),
    FOREIGN KEY("mac_dispositivo") REFERENCES "dispositivos"("mac") ON DELETE CASCADE
)