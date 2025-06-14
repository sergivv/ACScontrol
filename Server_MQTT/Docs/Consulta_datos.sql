SELECT 
    d.dispositivo AS Dispositivo,
	t.temperatura AS Temperatura,
	t.humedad AS Humedad,
	t.timestamp AS FechaHora,
	t.bateria AS Bateria
FROM 
    dispositivos d
JOIN 
    temperaturas t ON d.mac = t.mac
ORDER BY
	FechaHora DESC;