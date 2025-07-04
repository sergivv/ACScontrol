import paho.mqtt.client as mqtt
import sqlite3
import json
import re
import time
from datetime import datetime
from threading import Thread


class ACSControlServer:
    def __init__(self):
        self.db_conn = sqlite3.connect(
            'ACS_control.db', check_same_thread=False)
        self.setup_database()
        self.mqtt_client = self.setup_mqtt()
        self.last_config_check = {}
        self.running = True

    def setup_database(self):
        """Configura la estructura inicial de la base de datos"""
        cursor = self.db_conn.cursor()

        # Tabla dispositivos
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS "dispositivos" (
            "mac" TEXT PRIMARY KEY NOT NULL,
            "dispositivo" TEXT NOT NULL,
            "descripcion" TEXT,
            "fecha_registro" TEXT DEFAULT CURRENT_TIMESTAMP,
            "ubicacion" TEXT,
            "activo" INTEGER DEFAULT 1  -- 1=activo, 0=inactivo
        )''')

        # Tabla temperaturas
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS "temperaturas" (
            "id" INTEGER PRIMARY KEY AUTOINCREMENT,
            "mac" TEXT NOT NULL,
            "timestamp" TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            "temperatura" REAL NOT NULL,
            "humedad" REAL,
            "bateria" REAL,
            FOREIGN KEY ("mac") REFERENCES "dispositivos" ("mac") ON DELETE CASCADE
        )''')

        # Tabla estados
        cursor.execute('''
        CREATE TABLE IF NOT EXISTS "estados" (
            "mac_dispositivo" TEXT PRIMARY KEY,
            "temp_min" REAL DEFAULT NULL,
            "temp_max" REAL DEFAULT NULL,
            "estacion" TEXT DEFAULT NULL CHECK("estacion" IS NULL OR "estacion" IN ('verano', 'invierno')),
            "estado_caldera" INTEGER DEFAULT NULL CHECK("estado_caldera" IN (0, 1)),
            FOREIGN KEY("mac_dispositivo") REFERENCES "dispositivos"("mac") ON DELETE CASCADE
        )''')

        cursor.execute('''
        CREATE TABLE IF NOT EXISTS estados (
            mac_dispositivo TEXT PRIMARY KEY,
            temp_min REAL DEFAULT NULL,
            temp_max REAL DEFAULT NULL,
            estacion TEXT DEFAULT NULL CHECK("estacion" IS NULL OR "estacion" IN ('verano', 'invierno')),
            "estado_caldera" INTEGER DEFAULT NULL CHECK("estado_caldera" IN (0, 1)),
            last_updated TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (mac_dispositivo) REFERENCES dispositivos (mac) ON DELETE CASCADE
        )''')

        # Índice para mejor rendimiento
        cursor.execute(
            'CREATE INDEX IF NOT EXISTS idx_temperaturas_mac ON temperaturas (mac)')
        cursor.execute(
            'CREATE INDEX IF NOT EXISTS idx_estados_updated ON estados (last_updated)')

        # Trigger para borrado lógico (evita perder datos históricos)
        cursor.execute('''
        CREATE TRIGGER IF NOT EXISTS "logical_delete_dispositivo"
            BEFORE DELETE ON "dispositivos"
            BEGIN
                UPDATE "dispositivos" SET activo = 0 WHERE mac = OLD.mac;
                SELECT RAISE(IGNORE);  -- Cancela el borrado físico
            END
        ''')

        self.db_conn.commit()

    def setup_mqtt(self):
        """Configura el cliente MQTT"""
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        client.on_connect = self.on_connect
        client.on_message = self.on_message
        client.connect("192.168.3.1", 7983, 60)
        return client

    def start_config_monitor(self):
        """Inicia el hilo que monitorea cambios en la configuración"""
        def monitor_loop():
            while self.running:
                self.check_config_updates()
                time.sleep(30)  # Revisar cada 30 segundos

        Thread(target=monitor_loop, daemon=True).start()

    def check_config_updates(self):
        """Revisa y publica cambios en la configuración de dispositivos"""
        cursor = self.db_conn.cursor()

        # Obtener dispositivos activos
        cursor.execute('SELECT mac FROM dispositivos WHERE activo = 1')
        active_devices = [row[0] for row in cursor.fetchall()]

        for mac in active_devices:
            cursor.execute('''
            SELECT temp_min, temp_max, estacion, last_updated 
            FROM estados 
            WHERE mac_dispositivo = ?''', (mac,))
            result = cursor.fetchone()

            if not result:
                continue

            temp_min, temp_max, estacion, last_updated = result

            # Si es la primera vez o hubo cambios
            if mac not in self.last_config_check or \
               self.last_config_check[mac] != last_updated:

                config = {
                    'temp_min': temp_min,
                    'temp_max': temp_max,
                    'estacion': estacion
                }

                topic = f"ACS_Control/{mac}/ConfigUpdate"
                self.mqtt_client.publish(topic, json.dumps(config))
                print(f"Publicada actualización de configuración para {mac}")

                self.last_config_check[mac] = last_updated

    def on_connect(self, client, _, flags, rc, properties):
        """Callback para conexión MQTT"""
        print("Conectado al broker MQTT")
        client.subscribe("ACS_Control/+/Temperatura")
        client.subscribe("ACS_Control/+/ConfigRequest")
        self.start_config_monitor()

    def on_message(self, _, __, msg):
        """Procesa mensajes MQTT entrantes"""
        try:
            topic_parts = msg.topic.split('/')
            mac = topic_parts[1]

            if msg.payload.decode() == "1":  # Solicitud de configuración
                self.handle_config_request(mac)
            else:  # Datos de temperatura
                self.handle_temperature_data(mac, msg.payload)

        except Exception as e:
            print(f"Error procesando mensaje: {str(e)}")

    def handle_config_request(self, mac):
        """Procesa solicitud de configuración"""
        config = self.get_device_config(mac)
        if config:
            response_topic = f"ACS_Control/{mac}/ConfigResponse"
            self.mqtt_client.publish(response_topic, json.dumps(config))
            print(f"Config enviada a {mac}")

    def get_device_config(self, mac):
        """Obtiene configuración de la base de datos"""
        cursor = self.db_conn.cursor()
        cursor.execute('''
        SELECT temp_min, temp_max, estacion 
        FROM estados 
        WHERE mac_dispositivo = ?''', (mac,))
        result = cursor.fetchone()
        return {
            'temp_min': result[0],
            'temp_max': result[1],
            'estacion': result[2]
        } if result else None

    def handle_temperature_data(self, mac, payload):
        """Procesa y almacena datos de temperatura"""
        data = json.loads(payload.decode())
        if not self.validate_mac(mac):
            raise ValueError(f"MAC inválida: {mac}")

        cursor = self.db_conn.cursor()
        cursor.execute('''
        INSERT INTO temperaturas (mac, temperatura)
        VALUES (?, ?)''', (mac, data['temperatura']))
        self.db_conn.commit()

        print(f"Datos recibidos de {mac}: {data['temperatura']}°C")

    def validate_mac(self, mac):
        """Valida formato de dirección MAC"""
        return bool(re.match(r'^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$', mac))

    def run(self):
        """Inicia el servicio"""
        try:
            print("Servicio ACS Control iniciado")
            self.mqtt_client.loop_forever()
        except KeyboardInterrupt:
            print("\nDeteniendo servicio...")
            self.running = False
        finally:
            self.mqtt_client.disconnect()
            self.db_conn.close()
            print("Recursos liberados")


if __name__ == "__main__":
    server = ACSControlServer()
    server.run()
