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
CREATE TABLE IF NOT EXISTS dispositivos (
    mac BLOB PRIMARY KEY,  -- La dirección MAC será la clave primaria
    ubicacion TEXT NOT NULL,
    descripcion TEXT
    )
''')

# Crear la tabla temperaturas
cursor.execute('''
CREATE TABLE IF NOT EXISTS temperaturas (
    id INTEGER PRIMARY KEY AUTOINCREMENT,  
    mac BLOB NOT NULL,                     -- Relacionado con dispositivos.mac
    timestamp TEXT NOT NULL,
    temperatura REAL NOT NULL,
    humedad REAL DEFAULT NULL,
    FOREIGN KEY(mac) REFERENCES dispositivos(mac) ON DELETE CASCADE
)
''')

# Crear un índice en el campo "mac"
cursor.execute('''
CREATE INDEX IF NOT EXISTS idx_mac ON temperaturas(mac);
''')

print("Tabla creada exitosamente.")

# Función para obtener la hora local en formato ISO 8601


def hora_local():
    return datetime.now().strftime('%Y-%m-%d %H:%M:%S')


def es_mac_valida(mac):
    return bool(re.match(r'^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$', mac))

# Función para insertar datos desde un JSON


def insertar_datos(data_json):
    try:
        datos = json.loads(data_json)
        mac = datos.get("mac")
        temperatura = datos.get("temperatura")
        if not mac or temperatura is None:
            raise ValueError(
                "El JSON no contiene los campos 'mac' o 'temperatura'")

        if not es_mac_valida(mac):
            raise ValueError(f"La dirección MAC '{mac}' no es válida")

        timestamp = hora_local()  # Obtener la hora local

        # Insertar en la base de datos
        cursor.execute('''
        INSERT INTO temperaturas (mac, timestamp, temperatura)
        VALUES (?, ?, ?)
        ''', (mac, timestamp, temperatura))
        conn.commit()
        print(
            f"Datos insertados: MAC={mac}, Temperatura={temperatura}, Timestamp={timestamp}")
    except sqlite3.IntegrityError as ie:
        print(f"Error de integridad en la base de datos: {ie}")
    except sqlite3.OperationalError as oe:
        print(f"Error operativo en la base de datos: {oe}")
    except ValueError as ve:
        print(f"Error en los datos recibidos: {ve}")
    except Exception as e:
        print(f"Error procesando el JSON: {e}")

# The callback for when the client receives a CONNACK response from the server.


def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("ACS_Control/+/Temperatura")

# The callback for when a PUBLISH message is received from the server.


def on_message(client, userdata, msg):
    try:
        json_recibido = msg.payload.decode()
        # Extraer el identificador del dispositivo del tópico
        dispositivo = msg.topic.split('/')[1]
        print(
            f"[{hora_local()}] Mensaje recibido del dispositivo '{dispositivo}': {json_recibido}")
        insertar_datos(json_recibido)
    except Exception as e:
        print(f"Error procesando el mensaje MQTT: {e}")


mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message

mqttc.connect("192.168.3.1", 7983, 60)


def main():
    try:
        mqttc.loop_forever()
    except KeyboardInterrupt:
        print("Interrupción por teclado detectada. Cerrando cliente MQTT...")
    finally:
        mqttc.disconnect()  # Desconecta el cliente MQTT
        conn.close()        # Cierra la conexión SQLite
        print("Cliente MQTT y base de datos cerrados correctamente.")


if __name__ == "__main__":
    main()
