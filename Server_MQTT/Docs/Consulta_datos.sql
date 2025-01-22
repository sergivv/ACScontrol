SELECT 
    d.ubicacion AS Ubicación,
	t.temperatura AS Temperatura,
	t.humedad AS Humedad,
	t.timestamp AS FechaHora
FROM 
    dispositivos d
JOIN 
    temperaturas t ON d.mac = t.mac
ORDER BY
	FechaHora DESC;