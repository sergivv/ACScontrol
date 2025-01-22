from flask import Flask, render_template, request
import sqlite3

app = Flask(__name__)

# Crear/conectar a la base de datos
conn = sqlite3.connect(
    './../Server_MQTT/src/ACS_control.db', check_same_thread=False)
cursor = conn.cursor()

# Función para obtener los datos con paginación


def obtener_datos(pagina=1, por_pagina=25):
    offset = (pagina - 1) * por_pagina  # Calculamos el desplazamiento (OFFSET)
    cursor.execute('''
        SELECT t.timestamp, t.temperatura, t.humedad, t.mac, d.ubicacion
        FROM temperaturas t
        LEFT JOIN dispositivos d ON t.mac = d.mac
        ORDER BY t.timestamp DESC
        LIMIT ? OFFSET ?
    ''', (por_pagina, offset))
    return cursor.fetchall()

# Función para contar el total de registros


def contar_total_registros():
    cursor.execute('''
        SELECT COUNT(*) FROM temperaturas
    ''')
    return cursor.fetchone()[0]


# Ruta principal para mostrar los datos
@app.route('/')
def index():
    # Parámetros para la paginación
    # Página actual (por defecto es 1)
    pagina = int(request.args.get('pagina', 1))
    por_pagina = 25  # Número de resultados por página

    # Obtener los datos de la base de datos
    datos = obtener_datos(pagina, por_pagina)

    # Contar el total de registros
    total_registros = contar_total_registros()

    # Calcular el número total de páginas
    total_paginas = (total_registros + por_pagina -
                     1) // por_pagina  # Redondear hacia arriba

    return render_template('index.html', datos=datos, pagina=pagina, total_paginas=total_paginas)


if __name__ == '__main__':
    app.run(host="0.0.0.0", port=5000, debug=True)
