PS5 KBM Bridge v{VERSION}
=========================
https://github.com/coru182/ps5-kbm-bridge  ·  GPL-3.0-or-later
Built on Ghostcontrol by StonedModder (srbraboo). See CREDITS.md in the repo.

ESPAÑOL
-------
Requisitos: PS5 con jailbreak (probado en 11.60 con elfldr + kstuff + ShadowMountPlus)
y un teclado y mouse USB (un receptor inalámbrico con los dos, o dos dispositivos USB).

1. Primera vez (o si cambiaste de teclado/mouse): probá primero el ELF de diagnóstico.
   Mandá ps5-kbm-diag-v{TAG}.elf a elfldr (puerto 9021). No crea el mando virtual y se
   cierra solo a los 5 minutos. Mové el mouse y tocá teclas; el log queda en
   /data/ghostpad/gc_status.log ("matched USB ..." dice qué teclado/mouse encontró).
2. Instalación con icono: copiá la carpeta homebrew/PPSA51258 a /data/homebrew/PPSA51258
   por FTP (ps5upload u otro). Reiniciá, cargá tus payloads y abrí el icono
   "PS5 KBM Bridge". Sin ShadowMountPlus: mandá ps5-kbm-bridge-v{TAG}.elf a elfldr.
3. Mové el mouse y tocá una tecla: aparece el DualSense virtual. Ajustes: F11 en la PS5
   o http://IP-DE-LA-PS5:8099 desde el celular o la PC.
4. Arranque automático (solo etaHEN): copiá etaHEN/payloads/* a /data/etaHEN/payloads/.

Teclas reservadas: Windows = botón PS · F8 activar a mano · F9 mouse cursor/cámara ·
F10 medir zona muerta · F11 ajustes · F12 detener.
Perfiles propios: en la página de ajustes, "Compartir e importar perfiles".

ENGLISH
-------
Requirements: jailbroken PS5 (tested on 11.60 with elfldr + kstuff + ShadowMountPlus)
and a USB keyboard and mouse (one wireless receiver for both, or two USB devices).

1. First time (or new keyboard/mouse): try the diagnostics ELF first. Send
   ps5-kbm-diag-v{TAG}.elf to elfldr (port 9021). It never creates the virtual controller
   and exits after 5 minutes. Move the mouse and press keys; the log is
   /data/ghostpad/gc_status.log ("matched USB ..." says which keyboard/mouse it found).
2. Install with the icon: copy the homebrew/PPSA51258 folder to /data/homebrew/PPSA51258
   over FTP (ps5upload or similar). Reboot, load your payloads and open the
   "PS5 KBM Bridge" icon. Without ShadowMountPlus: send ps5-kbm-bridge-v{TAG}.elf to elfldr.
3. Move the mouse and press a key: the virtual DualSense appears. Settings: F11 on the PS5
   or http://PS5-IP:8099 from your phone or PC.
4. Automatic start (etaHEN only): copy etaHEN/payloads/* to /data/etaHEN/payloads/.

Reserved keys: Windows = PS button · F8 start by hand · F9 mouse cursor/camera ·
F10 measure dead zone · F11 settings · F12 stop.
Your own profiles: on the settings page, "Share and import profiles".

Experimental. Offline play only: some online games ban keyboard/mouse adapters.
It only translates input; it does not modify games.
