# Subir el proyecto a GitHub

Este zip NO incluye la carpeta `.git` (GitHub no deja subirla desde la web). Dos formas:

## A) Desde la web (sin instalar nada)
1. En github.com crea un repositorio nuevo llamado `bmo-player` (vacío).
2. Descomprime el zip. Entra a la carpeta `bmo-player/`.
3. En el repositorio: **Add file -> Upload files** y arrastra el CONTENIDO de la carpeta (las carpetas `3d-models`, `electronics`, `firmware`, `docs`, `tests`, `website`, `assets`, `wiring-diagrams` y los archivos `README.md`, `LICENSE`, `.gitignore`).
4. Cada archivo debe pesar menos de 25 MB (el mayor, `bmo_back_shell.stl`, pesa unos 20 MB). Si el navegador se queda colgado, súbelo en 2-3 tandas (una carpeta a la vez).
5. Escribe un mensaje de commit y pulsa **Commit changes**.
   Nota: `.gitignore` es un archivo oculto; si tu sistema no lo muestra, puedes omitirlo.

## B) Con git (terminal)
```
cd bmo-player
git init
git add -A
git commit -m "BMO PLAYER"
git branch -M main
git remote add origin https://github.com/TU_USUARIO/bmo-player.git
git push -u origin main
```

## Después
* Cambia el enlace del botón de GitHub en `website/index.html` (constante `REPO`, al inicio del script) por tu URL real.
* Para publicar la página con GitHub Pages: Settings -> Pages -> carpeta `/website` (o mueve `index.html` a la raíz).
