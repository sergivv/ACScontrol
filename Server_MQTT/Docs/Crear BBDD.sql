-- Tabla de dispositivos
CREATE TABLE IF NOT EXISTS "dispositivos" (
    "mac" TEXT PRIMARY KEY NOT NULL,
    "dispositivo" TEXT NOT NULL,
    "descripcion" TEXT,
    "fecha_registro" TEXT DEFAULT CURRENT_TIMESTAMP,
    "ubicacion" TEXT,
    "activo" INTEGER DEFAULT 1  -- 1=activo, 0=inactivo
);

-- Tabla de temperaturas con índice optimizado
CREATE TABLE IF NOT EXISTS "temperaturas" (
    "id" INTEGER PRIMARY KEY AUTOINCREMENT,
    "mac" TEXT NOT NULL,
    "timestamp" TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "temperatura" REAL NOT NULL,
    "humedad" REAL,
    "bateria" REAL,
    FOREIGN KEY ("mac") REFERENCES "dispositivos" ("mac") ON DELETE CASCADE
);

-- Índice para consultas rápidas por dispositivo y fecha
CREATE INDEX IF NOT EXISTS "idx_temperaturas_mac_timestamp" 
ON "temperaturas" ("mac", "timestamp");

-- Trigger para borrado lógico (evita perder datos históricos)
CREATE TRIGGER IF NOT EXISTS "logical_delete_dispositivo"
BEFORE DELETE ON "dispositivos"
BEGIN
    UPDATE "dispositivos" SET activo = 0 WHERE mac = OLD.mac;
    SELECT RAISE(IGNORE);  -- Cancela el borrado físico
END;
