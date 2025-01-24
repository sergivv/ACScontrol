SELECT 
    d.dispositivo AS Dispositivo,
	t.temperatura AS Temperatura,
	t.humedad AS Humedad,
	t.timestamp AS FechaHora
FROM 
    dispositivos d
JOIN 
    temperaturas t ON d.mac = t.mac
WHERE
	d.dispositivo = "Control"
ORDER BY
	FechaHora DESC;