# Contribuir a Trazo

Gracias por tu interés en contribuir a Trazo. Este proyecto está en desarrollo activo, y se agradecen las contribuciones que mejoren la claridad, la estabilidad o la documentación.

## Cómo contribuir

1. Haz fork del repositorio y crea una rama para tu trabajo.
2. Compila el proyecto localmente con `make`.
3. Verifica tus cambios en el código o la documentación.
4. Abre un pull request con una descripción clara de qué cambiaste y por qué.

## Flujo recomendado

- Usa ramas de características: `feature/<nombre>` o `fix/<descripción>`.
- Mantén los commits pequeños y focalizados.
- Explica las decisiones de diseño en la descripción del PR.
- Actualiza la documentación en inglés y español cuando cambie el comportamiento o la sintaxis.

## Convenciones del proyecto

- Mantén el código C simple y legible.
- Evita agregar comportamiento de compilación específico de plataforma, salvo que sea necesario.
- Las nuevas características del lenguaje deben quedar documentadas en `SYNTAX.md` y `SYNTAX.es.md`.
- Los archivos de ejemplo deben ir en `examples_ejemplos/`.

## Qué mejorar primero

Sugerencias de mejoras:

- Estabilizar el soporte del runtime para las características de bootstrap.
- Hacer que la ejecución de `unsafe.MAX` sea confiable y consistente.
- Corregir la gestión de memoria del parser y el manejo del AST.
- Añadir ejemplos claros para las capacidades actuales del lenguaje.

## Qué no cambiar todavía

- No modifiques código de proveedores o terceros a menos que sea necesario para la compilación.

## Licencia y conducta

Al contribuir, aceptas que tus aportes se licenciarán bajo Apache License 2.0 y que respetarás el Código de Conducta del proyecto.
