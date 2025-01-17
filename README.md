# Sistema de control de temperatura calefacción y agua caliente

## Descripción del proyecto

Con este proyecto se pretende realizar un sistema que nos permita el control y monitorización del sistema de calefacción y agua caliente de la casa. Se trataría de controlar la temperatura del agua del acumulador. Esto lo conseguiremos controlando el encendido de la caldera y de la bomba de circulación del agua para aprovechar el calor residual de la caldera. Este sistema también pretende controlar el termostato del acumulador para alternar entre el modo verano y modo invierno. En modo invierno se bajará la temperatura del termostato para permitir la circulación del agua por los radiadores y en modo verano se aumentará al máximo la temperatura para que el agua para impedir la circulación del agua por el circuito de calefacción.

### Componentes del sistema

- **Sensor de temperatura acumulador:** este dispositivo realizará dos funciones. La primera y más importante es el monitoreo de la temperatura del agua dentro del acumulador. Enviará los datos tanto a la base de datos del sistema como al módulo de control de la caldera para actuar según la temperatura registrada. La segunda función es alternar entre los dos modos de funcionamiento, modo verano o modo invierno. Esto lo conseguiremos modificando el termostato del acumulador.
- **Control sistema / Sensor temperatura salón:** el dispositivo medirá la temperatura en el salón. A parte de la medición de temperatura también llevará a cabo control del sistema, encendido/apagado de la caldera de forma manual y también configurando el umbral de temperaturas para el funcionamiento automático de la calefacción.
- **Control caldera / bomba recirculación del agua:** el dispositivo instalado en el cuarto de calderas controlará el encendido/apagado de la caldera según el control del salón o la programación automática. También actuará sobre la bomba de circulación de agua dependiendo de la temperatura de la caldera y del acumulador.

## Información sobre los componentes del sistema

- **Sensor de temperatura del acumulador:**
  En la versión inicial del sensor de temperatura del acumulador no integrará el sistema de control de modo verano/invierno. Esté sistema se añadirá en futuras versiones del componente.

  Este dispositivo se encargará en un primer momento de medir la temperatura del agua dentro del acumulador y pasar la información de dicha temperatura a la base de datos para llevar un histórico de dichas temperaturas.

  Gracias a esta medición el sistema de control de la caldera actuará encendiéndose o apagándose cuando fuese necesario para mantener una temperatura optima del agua. Esta temperatura oscilará entre los 50ºC de mínima y los 70ºC de temperatura máxima.

- **Control sistema / Sensor temperatura salón:**
  Este componente se situará en el salón de la casa. Este medirá la temperatura de la estancia y enviando los datos al servidor. Dependiendo de la temperatura del salón se le dará orden a la caldera para que se encienda y permita calentar los radiadores de la casa.

  El componente permitirá programar las temperaturas de encendido/apagado del modo automático de la calefacción, también encender la caldera de forma manual por si se necesita calentar el agua del acumulador o los radiadores de la casa.

  Dispondrá de una pantalla con información util para el sistema, como temperatura del salón, temperatura de la caldera y acumulador. También mostrará si está encendida la caldera o la bomba y asi llevar un control del sistema de forma remota, sin necesidad de acudir al cuarto de calderas.

- **Control caldera / bomba recirculación del agua:**
  Este componente situado en el cuarto de calderas dispondrá de distintos reles para la activación/desactivación tanto de la caldera como de la bomba de recirculación del agua. En la pantalla se mostrará información como la temperaturas de caldera y acumulador, si está activada la bomba o la caldera y también el número de veces que estuvo encendida la caldera (tanto de forma manual como automática)

  El componente recibirá las ordenes de encendido y apagado del resto de componentes del sistema.
