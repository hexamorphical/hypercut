/*
 This file is part of Livecut (DPF port).
 Livecut can be redistributed and/or modified under the terms of the
 GNU General Public License, as published by the Free Software Foundation;
 either version 2 of the License, or (at your option) any later version.
 */

/*
   UI languages: English (the source strings) and Spanish.

   The UI translates at its few text funnels (drawing, measuring, tooltips), so most labels need no changes at
   their call sites: tr() looks the English text up and returns the Spanish one, or the text itself when there is no
   entry (numbers, preset names, note names...). Text assembled with snprintf translates its format string instead.
   Plurals: trn(fmt, n) uses the entry "fmt|1" for n == 1. English formats that take a plural suffix ("" / "S") as a
   %s argument have Spanish formats that consume it with %.0s.
*/

#ifndef LIVECUT_LANG_HPP
#define LIVECUT_LANG_HPP

#include <string>
#include <unordered_map>

enum UiLang { kLangEn = 0, kLangEs, kNumLangs };

// one language for all plugin windows (it is a per-user preference, like the theme)
static inline int& uiLang()
{
    static int lang = kLangEn;
    return lang;
}

struct LangEntry { const char* en; const char* es; };

static const LangEntry kSpanish[] = {
    // ---- algorithms ----
    { "Classic jungle cutter. Blocks of repeated cuts with stutter fills toward the end of each phrase.",
      "Cortador jungle clásico. Bloques de cortes repetidos con stutters de relleno hacia el final de cada frase." },
    { "Accelerating / decelerating cut rolls. Blocks are cut straight, or warped into speeding rolls.",
      "Redobles de cortes que aceleran o frenan. Los bloques suenan tal cual o se deforman en redobles cada vez más rápidos." },
    { "Squarepusher-style pre-baked cut sequences, with fills chosen by activity.",
      "Secuencias de cortes prefabricadas al estilo Squarepusher, con rellenos elegidos según la actividad." },
    { "Retriggers each half / whole bar on a euclidean rhythm (3-3-2 and friends). Busier in the last bar.",
      "Redispara cada medio compás o compás entero con un ritmo euclidiano (3-3-2 y similares). Más denso en el último compás." },
    { "Build-up rolls: retriggers per beat grow across the phrase. Pitch range = riser, pan range = ping-pong.",
      "Redobles de subida: los redisparos por tiempo crecen a lo largo de la frase. Rango de tono = riser, rango de paneo = ping-pong." },
    { "Trance gate: the audio stays in place, steps are only muted. Random patterns, or click the steps to draw your own.",
      "Trance gate: el audio no se reordena, los pasos solo se silencian. Patrones aleatorios, o haz clic en los pasos para dibujar los tuyos." },

    // ---- themes ----
    { "Spectrum", "Espectro" }, { "Sunset", "Atardecer" }, { "Candy", "Caramelo" }, { "Twilight", "Crepúsculo" },
    { "Saffron", "Azafrán" }, { "Orchid", "Orquídea" }, { "Marigold", "Caléndula" }, { "Nebula", "Nebulosa" },
    { "Mint", "Menta" }, { "Graphite", "Grafito" }, { "Lagoon", "Laguna" }, { "Fern", "Helecho" },
    { "Blossom", "Flor" }, { "Amber", "Ámbar" }, { "Sage", "Salvia" },

    // ---- header, presets ----
    { "Randomize", "Aleatorizar" },
    { "Light mode", "Modo claro" },
    { "Dark mode", "Modo oscuro" },
    { "Theme: %s", "Tema: %s" },
    { "Redo (Ctrl+Shift+Z)", "Rehacer (Ctrl+Shift+Z)" },
    { "Undo (Ctrl+Z)", "Deshacer (Ctrl+Z)" },
    { "English", "Inglés" },
    { "Spanish", "Español" },
    { "Close the preset browser", "Cerrar el navegador de presets" },
    { "Browse presets", "Explorar presets" },
    { "Save preset", "Guardar preset" },
    { "Close", "Cerrar" },
    { "Showing favourites only: click to show all presets", "Solo favoritos: haz clic para ver todos los presets" },
    { "Show favourites only", "Mostrar solo favoritos" },
    { "Search name, author, category", "Buscar nombre, autor, categoría" },
    { "NAME", "NOMBRE" },
    { "AUTHOR", "AUTOR" },
    { "CATEGORY", "CATEGORÍA" },
    { "Sort by %s", "Ordenar por %s" },
    { "name", "nombre" },
    { "author", "autor" },
    { "category", "categoría" },
    { "Click again to delete", "Haz clic otra vez para borrar" },
    { "Delete preset (asks to confirm)", "Borrar preset (pide confirmación)" },
    { "Delete: factory presets are hidden (restore them at the bottom of the list)",
      "Borrar: los presets de fábrica se ocultan (se restauran al final de la lista)" },
    { "Remove from favourites", "Quitar de favoritos" },
    { "Add to favourites", "Añadir a favoritos" },
    { "delete?", "¿borrar?" },
    { "No favourites yet: click the star on a preset", "Aún no hay favoritos: haz clic en la estrella de un preset" },
    { "No preset matches the search", "Ningún preset coincide con la búsqueda" },
    { "%d hidden factory preset%s \xc2\xb7 restore", "Presets de fábrica ocultos: %d%.0s \xc2\xb7 restaurar" },
    { "by ", "por " },
    { "unknown author", "autor desconocido" },
    { "not saved", "sin guardar" },
    { "SAVE PRESET AS", "GUARDAR PRESET COMO" },
    { "optional", "opcional" },
    { "Overwrites existing", "Sobrescribe el existente" },
    { "SAVE", "GUARDAR" },
    { "THEME", "TEMA" },
    { "CRT screens", "Pantallas CRT" },

    // ---- views ----
    { "wheel: zoom  \xc2\xb7  shift+wheel: height  \xc2\xb7  drag: pan  \xc2\xb7  double-click: reset",
      "rueda: zoom  \xc2\xb7  shift+rueda: altura  \xc2\xb7  arrastrar: desplazar  \xc2\xb7  doble clic: restablecer" },
    { "wheel: zoom  \xc2\xb7  shift+wheel: height  \xc2\xb7  double-click: reset",
      "rueda: zoom  \xc2\xb7  shift+rueda: altura  \xc2\xb7  doble clic: restablecer" },
    { "CAPTURE BUFFER", "BÚFER DE CAPTURA" },
    { "Visualisation unavailable in this host", "Visualización no disponible en este host" },
    { "STEP %d / %d", "PASO %d / %d" },
    { "REPEAT %d / %d", "REPETICIÓN %d / %d" },
    { "IDLE", "INACTIVO" },
    { "ENVELOPE", "ENVOLVENTE" },
    { "Waiting for the next bar...", "Esperando el próximo compás..." },
    { "Start host playback to begin slicing", "Inicia la reproducción en el host para empezar a cortar" },
    { "LIVE CUTS", "CORTES EN VIVO" },
    { "FILL %d / %d", "RELLENO %d / %d" },
    { "FILL IN %d BAR%s", "RELLENO EN %d COMPASES%.0s" },
    { "FILL IN %d BAR%s|1", "RELLENO EN %d COMPÁS%.0s" },
    { "SLICER OFF", "CORTADOR APAGADO" },
    { "GATE PATTERN", "GATE: PATRÓN" },
    { "GATE RANDOM", "GATE: ALEATORIO" },
    { "%d BAR PHRASE", "FRASE DE %d COMPASES" },
    { "%d BAR PHRASE|1", "FRASE DE %d COMPÁS" },
    { "%d CUTS", "%d CORTES" },
    { "%d CUTS|1", "%d CORTE" },
    { "no phrase yet", "aún no hay frase" },
    { "DRY  \xc2\xb7  fill starts in %d bar%s", "SECO  \xc2\xb7  el relleno empieza en %d compases%.0s" },
    { "DRY  \xc2\xb7  fill starts in %d bar%s|1", "SECO  \xc2\xb7  el relleno empieza en %d compás%.0s" },
    { "RATE", "TASA" },
    { "OUTPUT  \xc2\xb7  LAST %.3g BAR%s", "SALIDA  \xc2\xb7  ÚLTIMOS %.3g COMPASES%.0s" },
    { "OUTPUT  \xc2\xb7  LAST %.3g BAR%s|1", "SALIDA  \xc2\xb7  ÚLTIMO %.3g COMPÁS%.0s" },
    { "OUTPUT  \xc2\xb7  LAST %.3g BEAT%s", "SALIDA  \xc2\xb7  ÚLTIMOS %.3g TIEMPOS%.0s" },
    { "OUTPUT  \xc2\xb7  LAST %.3g BEAT%s|1", "SALIDA  \xc2\xb7  ÚLTIMO %.3g TIEMPO%.0s" },
    { "%ld/bar", "%ld/compás" },

    // ---- sequence ----
    { "SEQUENCE", "SECUENCIA" },
    { "Auto slicer on: click to let the input run straight through the effects",
      "Cortador automático activado: haz clic para que la entrada pase directamente por los efectos" },
    { "Auto slicer off: the input runs straight through the effects",
      "Cortador automático desactivado: la entrada pasa directamente por los efectos" },
    { "SUBDIVISION", "SUBDIVISIÓN" },
    { "TRIPLET", "TRESILLO" },
    { "%s notes", "notas de %s" },
    { "%s  \xc2\xb7  triplets", "%s  \xc2\xb7  tresillos" },
    { "\xe2\x80\xa2 = triplet", "\xe2\x80\xa2 = tresillo" },
    { "SHORTEST CUT", "CORTE MÍNIMO" },
    { "ANY", "LIBRE" },
    { "PHRASE (BARS)", "FRASE (COMPASES)" },
    { "FILL MODE", "MODO RELLENO" },
    { "EVERY (BARS)", "CADA (COMPASES)" },
    { "SLICE LAST", "CORTAR ÚLTIMOS" },

    // ---- algorithm panel ----
    { "AREA", "ZONA" },
    { "REPEATS", "REPETICIONES" },
    { "STRAIGHT", "RECTO" },
    { "RITARD", "RITARD." },
    { "SPEED", "VELOCIDAD" },
    { "ACTIVITY", "ACTIVIDAD" },
    { "DENSITY", "DENSIDAD" },
    { "CHANCE", "PROBAB." },
    { "SLICE", "CORTE" },
    { "which hit's audio repeats", "qué golpe se repite" },
    { "length of one step", "duración de un paso" },
    { "RANDOM", "ALEATORIO" },
    { "PATTERN", "PATRÓN" },
    { "VARIATION", "VARIACIÓN" },
    { "ENVELOPE SHAPES", "MODULACIÓN" },
    { "depth", "profundidad" },
    { "VOLUME", "VOLUMEN" },
    { "LOWPASS", "PASA BAJOS" },
    { "HIGHPASS", "PASA ALTOS" },
    { "draw the steps in the LIVE CUTS view", "dibuja los pasos en la vista CORTES EN VIVO" },
    { "INTENSITY", "INTENSIDAD" },
    { "CURVE", "CURVA" },

    // ---- gate sequencer ----
    { "CLEAR", "BORRAR" },
    { "FILL", "RELLENO" },
    { "INVERT", "INVERTIR" },
    { "FLAT", "PLANO" },
    { "Mute every step", "Silenciar todos los pasos" },
    { "Open every step (untied)", "Abrir todos los pasos (sin ligar)" },
    { "Swap open and muted steps", "Intercambiar pasos abiertos y silenciados" },
    { "Remove every step's envelope and curve", "Quitar la envolvente y la curva de todos los pasos" },
    { "Shift steps and envelopes one step earlier", "Mover pasos y envolventes un paso antes" },
    { "Shift steps and envelopes one step later", "Mover pasos y envolventes un paso después" },
    { "straight", "recta" },
    { "curve %+.0f%%", "curva %+.0f%%" },
    { "STEP %d  \xc2\xb7  %s%s  \xc2\xb7  no envelope (wheel to add)",
      "PASO %d  \xc2\xb7  %s%s  \xc2\xb7  sin envolvente (rueda para añadir)" },
    { "OPEN", "ABIERTO" },
    { "MUTED", "SILENCIADO" },
    { ", TIED", ", LIGADO" },
    { "STEP %d  \xc2\xb7  %s %.0f%% = %.1f ms  \xc2\xb7  %s", "PASO %d  \xc2\xb7  %s %.0f%% = %.1f ms  \xc2\xb7  %s" },
    { "ATTACK", "ATAQUE" },
    { "DECAY", "CAÍDA" },
    { "%d STEPS  \xc2\xb7  %s%s  \xc2\xb7  click: open / mute  \xc2\xb7  drag: tie  \xc2\xb7  "
      "wheel / right-drag: attack up, decay down  \xc2\xb7  shift+drag: curve",
      "%d PASOS  \xc2\xb7  %s%s  \xc2\xb7  clic: abrir / silenciar  \xc2\xb7  arrastrar: ligar  \xc2\xb7  "
      "rueda / arrastre derecho: arriba ataque, abajo caída  \xc2\xb7  shift+arrastrar: curva" },
    { "  \xc2\xb7  RANDOM", "  \xc2\xb7  ALEATORIO" },
    { "DRY  \xc2\xb7  waiting for the fill", "SECO  \xc2\xb7  esperando el relleno" },

    // ---- algorithm monitor ----
    { "%d-UNIT CUT  x%d", "CORTE DE %d UNID.  x%d" },
    { "STUTTER 1/%d UNIT  x%d", "STUTTER 1/%d UNID.  x%d" },
    { "STRAIGHT  %d UNIT%s", "RECTO  %d UNID.%.0s" },
    { "ROLL  x%d", "REDOBLE  x%d" },
    { "ACCEL ROLL  x%d", "REDOBLE ACEL.  x%d" },
    { "RITARD ROLL  x%d", "REDOBLE RIT.  x%d" },
    { "%d QUAVER%s", "%d CORCHEAS%.0s" },
    { "%d QUAVER%s|1", "%d CORCHEA%.0s" },
    { "%d SEMIQUAVERS", "%d SEMICORCHEAS" },
    { "FILL #%d  %d/%d", "RELLENO #%d  %d/%d" },
    { "x%d PER BEAT", "x%d POR TIEMPO" },
    { "%s  %d/%d OPEN", "%s  %d/%d ABIERTOS" },
    { "LIVE", "EN VIVO" },
    { "unavailable in this host", "no disponible en este host" },
    { "waiting for a phrase", "esperando una frase" },
    { "CUT", "CORTE" },
    { "ROLL", "REDOBLE" },
    { "ACCEL", "ACEL." },
    { "QUAVERS", "CORCHEAS" },
    { "SEMIQ.", "SEMIC." },
    { "GATE BAR", "COMPÁS GATE" },
    { "ZONE", "ZONA" },

    // ---- cut shape, effects, master ----
    { "CUT SHAPE", "FORMA DEL CORTE" },
    { "AMPLITUDE", "AMPLITUD" },
    { "PAN", "PANEO" },
    { "PITCH", "TONO" },
    { "DUTY", "DURACIÓN" },
    { "FILL DUTY", "DUR. RELLENO" },
    { "FADE", "FUNDIDO" },
    { "SEED", "SEMILLA" },
    { "FILTER", "FILTRO" },
    { "PER SLICE", "POR CORTE" },
    { "TYPE", "TIPO" },
    { "TIME", "TIEMPO" },
    { "FEEDBACK", "REALIM." },
    { "TRANSIENT", "TRANSITORIO" },
    { "SHAPER", "MODELADOR" },
    { "PUMP", "BOMBEO" },
    { "SUSTAIN", "SOSTENIDO" },
    { "Boost (right) or cut (left) the hit of each transient",
      "Realza (derecha) o recorta (izquierda) el golpe de cada transitorio" },
    { "Dip the level right after each hit: the hit stands out without getting louder",
      "Baja el nivel justo después de cada golpe: el golpe destaca sin sonar más fuerte" },
    { "Boost (right) or cut (left) the tail after each hit",
      "Realza (derecha) o recorta (izquierda) la cola después de cada golpe" },
    { "MODE", "MODO" },
    { "SEND", "ENVÍO" },
    { "INSERT", "INSERCIÓN" },
    { "SIZE", "TAMAÑO" },
    { "DAMP", "AMORTIG." },
    { "MASTER", "MÁSTER" },
    { "LIMITER", "LIMITADOR" },
    { "CEILING", "TECHO" },
    { "RELEASE", "LIBERACIÓN" },
    { "GAIN REDUCTION", "REDUCCIÓN" },
    { "FILTERS", "FILTROS" },
    { "MAKEUP", "GANANCIA" },
    { "Output gain after the limiter, to bring the level back up.\n"
      "A boost here can go past the limiter's ceiling.",
      "Ganancia de salida después del limitador, para recuperar el nivel.\n"
      "Aquí un aumento puede superar el techo del limitador." },
    { "DRY / WET", "MEZCLA" },

    // ---- delay: echo mode ----
    { "ECHO", "ECO" },
    { "ON", "ACTIVO" },
    { "LEFT", "IZQ." },
    { "RIGHT", "DER." },
    { "MIX", "MEZCLA" },
    { "TONE", "TIMBRE" },
    { "1 BAR", "1 COMPÁS" },
    { "2 BARS", "2 COMPASES" },
    { "Mono: same time on both sides  \xc2\xb7  L / R: own time per side  \xc2\xb7  Ping: repeats bounce between the sides",
      "Mono: mismo tiempo en ambos lados  \xc2\xb7  L / R: tiempo propio por lado  \xc2\xb7  Ping: las repeticiones rebotan entre lados" },
    { "Synced to the host tempo (note values): click for free time in ms",
      "Sincronizado al tempo del host (figuras): clic para tiempo libre en ms" },
    { "Free time in ms: click to sync to the host tempo",
      "Tiempo libre en ms: clic para sincronizar al tempo del host" },
    { "Tune: the free times snap to notes of the pitchmap's key and scale (time = 1 / pitch).\n"
      "Short times with high feedback ring at that note; set the scale in SPECTRAL PITCHMAP.",
      "Afinar: los tiempos libres se ajustan a notas de la tonalidad y escala del pitchmap (tiempo = 1 / tono).\n"
      "Los tiempos cortos con mucha realimentación resuenan en esa nota; elige la escala en PITCHMAP ESPECTRAL." },
    { "Tune: each slice's comb delay snaps to a note of the pitchmap's key and scale,\n"
      "so the comb rings in key (FB rings the most). Set the scale in SPECTRAL PITCHMAP.",
      "Afinar: el retardo del comb de cada corte se ajusta a una nota de la tonalidad y escala del pitchmap,\n"
      "así el comb resuena en tonalidad (FB resuena más). Elige la escala en PITCHMAP ESPECTRAL." },
    { "DELAY SCALE", "ESCALA DELAY" },
    { "The delay's TUNE follows this key, scale and TUNE knob, also while the pitchmap is off",
      "El AFINAR del delay sigue esta tonalidad, escala y la perilla AFINAR, también con el pitchmap apagado" },

    // ---- pitchmap ----
    { "Root: %s (a blue line marks notes the pitchmap is moving sound to)%s",
      "Raíz: %s (una línea azul marca las notas hacia las que el pitchmap mueve el sonido)%s" },
    { "  (MIDI mode: held notes choose the notes)", "  (modo MIDI: las notas pulsadas eligen las notas)" },
    { "SCALE", "ESCALA" },
    { "Click: all scales  \xc2\xb7  wheel: step  \xc2\xb7  shift-click: relative scale",
      "Clic: todas las escalas  \xc2\xb7  rueda: siguiente  \xc2\xb7  shift+clic: escala relativa" },
    { "Click: all scales  \xc2\xb7  wheel: step", "Clic: todas las escalas  \xc2\xb7  rueda: siguiente" },
    { "OFF", "APAGADO" },
    { "DRY", "SECO" },
    { "TUNED", "AFINADO" },
    { "PITCHMAP IS OFF", "PITCHMAP APAGADO" },
    { "  (in key)", "  (en tonalidad)" },
    { "Drag to set the %s edge of the region (click anywhere moves the nearest edge, double-click resets)",
      "Arrastra para fijar el borde %s de la región (un clic en cualquier sitio mueve el borde más cercano, doble clic restablece)" },
    { "low", "inferior" },
    { "high", "superior" },
    { "SPECTRAL PITCHMAP", "PITCHMAP ESPECTRAL" },
    { "KEY", "TONALIDAD" },
    { "Latency added by the pitchmap (reported to the host, which compensates it)",
      "Latencia añadida por el pitchmap (se informa al host, que la compensa)" },
    { "REGION", "REGIÓN" },
    { "TUNE", "AFINAR" },
    { "GAIN", "GANANCIA" },
    { "Spectral morph: blends the processed magnitudes toward the dry ones (keeps the processed phase),\n"
      "so the pitchmap's colour sits on the dry signal's transients and tails. Adds the FFT latency.",
      "Morph espectral: mezcla las magnitudes procesadas hacia las secas (mantiene la fase procesada),\n"
      "así el color del pitchmap se asienta sobre los transitorios y colas de la señal seca. Añade la latencia de la FFT." },
    { "Spectral gate: silences each processed bin while the same dry bin is under the threshold,\n"
      "which shortens ringing and tails. Adds the FFT latency.",
      "Gate espectral: silencia cada bin procesado mientras el mismo bin seco está bajo el umbral,\n"
      "lo que acorta resonancias y colas. Añade la latencia de la FFT." },
    { "FFT size of morph / gate: larger is cleaner, with more latency",
      "Tamaño de FFT de morph / gate: más grande es más limpio, con más latencia" },
    // scales
    { "Major", "Mayor" }, { "Minor", "Menor" }, { "Harmonic Minor", "Menor armónica" }, { "Melodic Minor", "Menor melódica" },
    { "Dorian", "Dórica" }, { "Phrygian", "Frigia" }, { "Lydian", "Lidia" }, { "Mixolydian", "Mixolidia" },
    { "Locrian", "Locria" }, { "Major Pentatonic", "Pentatónica mayor" }, { "Minor Pentatonic", "Pentatónica menor" },
    { "Phrygian Dominant", "Frigia dominante" }, { "Hungarian Minor", "Menor húngara" }, { "Whole Tone", "Tonos enteros" },
    { "Major Triad", "Tríada mayor" }, { "Minor Triad", "Tríada menor" }, { "Major 7th", "Séptima mayor" },
    { "Minor 7th", "Séptima menor" }, { "Dominant 7th", "Séptima dominante" }, { "Fifths", "Quintas" },
    { "Root Only", "Solo raíz" }, { "Chromatic", "Cromática" },

    // ---- signal flow ----
    { "SIGNAL FLOW   \xc2\xb7   DRAG ANY BLOCK TO REORDER (EFFECTS BEFORE SLICE PROCESS WHAT IT RECORDS)   \xc2\xb7   CLICK SLICE TO SWITCH THE SLICER",
      "FLUJO DE SEÑAL   \xc2\xb7   ARRASTRA UN BLOQUE PARA REORDENAR (LOS EFECTOS ANTES DE CORTE PROCESAN LO QUE GRABA)   \xc2\xb7   CLIC EN CORTE PARA ACTIVAR EL CORTADOR" },
    { "IN", "ENTRADA" },
    { "OUT", "SALIDA" },
    { "DRY/WET", "MEZCLA" },
    { "LIMIT", "LÍMITE" },
    { "Drag to move the slicer in the chain, click to switch it off",
      "Arrastra para mover el cortador en la cadena, haz clic para apagarlo" },
    { "Drag to move the slicer in the chain, click to switch it on",
      "Arrastra para mover el cortador en la cadena, haz clic para encenderlo" },
    { "Drag to move it anywhere in the chain (before SLICE it processes what the slicer records)",
      "Arrastra para moverlo a cualquier lugar de la cadena (antes de CORTE procesa lo que graba el cortador)" },
};

static inline const std::unordered_map<std::string, const char*>& spanishTable()
{
    static const std::unordered_map<std::string, const char*> table = [] {
        std::unordered_map<std::string, const char*> t;
        for (const LangEntry& e : kSpanish)
            t.emplace(e.en, e.es);
        return t;
    }();
    return table;
}

// the text in the current language (the English text itself when there is no translation)
static inline const char* tr(const char* en)
{
    if (en == nullptr || uiLang() == kLangEn || en[0] == '\0')
        return en;
    const auto& t = spanishTable();
    const auto it = t.find(en);
    return it != t.end() ? it->second : en;
}

// plural-aware: the entry "en|1" when n == 1
static inline const char* trn(const char* en, long n)
{
    if (uiLang() != kLangEn && n == 1)
    {
        const auto& t = spanishTable();
        const auto it = t.find(std::string(en) + "|1");
        if (it != t.end())
            return it->second;
    }
    return tr(en);
}

#endif
