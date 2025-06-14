import paho.mqtt.client as mqtt
import sqlite3
from datetime import datetime
import json
import re

# Crear/conectar a la base de datos
conn = sqlite3.connect('ACS_control.db', check_same_thread=False)
cursor = conn.cursor()

# Crear la tabla dispositivos
cursor.execute('''
CREATE TABLE IF NOT EXISTS "dispositivos" (
    "mac" TEXT PRIMARY KEY NOT NULL,
    "dispositivo" TEXT NOT NULL,
    "descripcion" TEXT,
    "fecha_registro" TEXT DEFAULT CURRENT_TIMESTAMP,
    "ubicacion" TEXT,
    "activo" INTEGER DEFAULT 1  -- 1=activo, 0=inactivo
);
''')

# Crear la tabla temperaturas
cursor.execute('''
CREATE TABLE IF NOT EXISTS "temperaturas" (
    "id" INTEGER PRIMARY KEY AUTOINCREMENT,
    "mac" TEXT NOT NULL,
    "timestamp" TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "temperatura" REAL NOT NULL,
    "humedad" REAL,
    "bateria" REAL,
    FOREIGN KEY ("mac") REFERENCES "dispositivos" ("mac") ON DELETE CASCADE
);
''')

# Índice para consultas rápidas por dispositivo y fecha
cursor.execute('''
CREATE INDEX IF NOT EXISTS "idx_temperaturas_mac_timestamp" 
ON "temperaturas" ("mac", "timestamp");
''')

# Trigger para borrado lógico (evita perder datos históricos)
cursor.execute('''
CREATE TRIGGER IF NOT EXISTS "logical_delete_dispositivo"
BEFORE DELETE ON "dispositivos"
BEGIN
    UPDATE "dispositivos" SET activo = 0 WHERE mac = OLD.mac;
    SELECT RAISE(IGNORE);  -- Cancela el borrado físico
END;
''')

print("Tablas creadas exitosamente.")


# Función para obtener la hora local en formato ISO 8601
def hora_iso():
    return datetime.now().strftime('%Y-%m-%d %H:%M:%S')


def validar_mac(mac):
    """Valida formato MAC (AA:BB:CC:DD:EE:FF)"""
    return bool(re.match(r'^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$', mac))

# Función para insertar datos desde un JSON


def insertar_medicion(data):
    try:
        # Validación básica
        if not all(k in data for k in ['mac', 'temperatura']):
            raise ValueError("Faltan campos obligatorios")

        if not validar_mac(data['mac']):
            raise ValueError(f"MAC inválida: {data['mac']}")

        # Insertar medición
        cursor.execute('''
        INSERT INTO temperaturas 
        (mac, temperatura, humedad, bateria) 
        VALUES (?, ?, ?, ?)
        ''', (
            data['mac'],
            data['temperatura'],
            data.get('humedad'),
            data.get('bateria')  # Opcional para futuro
        ))
        conn.commit()

        print(f"[OK] {data['mac']} | Temp: {data['temperatura']}°C | "
              f"Hum: {data.get('humedad', 'N/A')}% | "
              f"Bat: {data.get('bateria', 'N/A')}V")

    except ValueError as ve:
        print(f"[Validation Error] {ve}")
    except sqlite3.IntegrityError:
        print(f"[SQL Error] Violación de integridad")
    except Exception as e:
        print(f"[Unexpected Error] {type(e).__name__}: {e}")

# Callbacks MQTT


def on_connect(client, _, flags, rc, properties):
    print(f"Conectado al broker (código {rc})")
    client.subscribe("ACS_Control/+/Temperatura")


def on_message(_, __, msg):
    try:
        topic_parts = msg.topic.split('/')
        dispositivo_id = topic_parts[1]

        data = json.loads(msg.payload.decode())
        # Usar ID del tópico si no hay nombre
        data.setdefault('dispositivo', dispositivo_id)

        insertar_medicion(data)

    except json.JSONDecodeError:
        print(f"[ERROR] JSON inválido: {msg.payload}")
    except Exception as e:
        print(f"[ERROR] Procesando mensaje: {e}")


# Configuración MQTT
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

try:
    mqtt_client.connect("192.168.3.1", 7983, 60)
except Exception as e:
    print(f"[FATAL] Error conexión MQTT: {e}")
    exit(1)


def main():
    try:
        print("Iniciando servicio... (Ctrl+C para detener)")
        mqtt_client.loop_forever()

    except KeyboardInterrupt:
        print("\nDeteniendo servicio...")
    finally:
        mqtt_client.disconnect()
        conn.close()
        print("Recursos liberados correctamente")


if __name__ == "__main__":
    main()
