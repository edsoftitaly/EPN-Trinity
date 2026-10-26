# EPN Trinity Stereo Chorus

VST3 (+ Standalone) per Windows di EPN Audio. Emula il comportamento dei tri-chorus rack dei primi anni '80 (famiglia Dyno-My-Piano TSC 618 / Dytronics CS-5 / Songbird): tre linee BBD sfasate di 120°, preset LFO "ensemble" e LFO manuale.

Non è un clone circuitale: lo schema che avevamo era una scheda preamp JFET (4x J310, Normal/Overtone/Bass Boost/Volume), non il chorus. Il modello è ricostruito dal comportamento documentato dell'unità.

## Controlli

| Pannello | Cosa fa |
|---|---|
| **-12dB PAD** | -12 dB prima delle BBD, +12 dB dopo. Più headroom sui segnali caldi, un filo più di fruscio. LED PEAK si accende quando l'ingresso BBD satura |
| **EFFECT** | On / bypass (il bypass passa l'ingresso intatto, anche stereo) |
| **CHORUS MODE** | Stereo (LED acceso): L = dry + I + II, R = dry + II + III. Spento = mono, tutto sommato |
| **LFO MODE PRESET** | Sweep sinusoidale lento fisso (0.52 Hz) + piccolo vibrato veloce (5.8 Hz). Il suono "string machine" |
| **LFO MODE MANUAL** | Sinusoide 0.03–7.45 Hz (RATE), profondità per linea con INTENSITY I / II / III |
| Preset + Manual | Si possono accendere insieme, gli sweep si sommano |
| **POWER** | Spento = segnale dry, LED e VU spenti |
| **OUT LEVEL** | VU a LED, 0 VU = -18 dBFS RMS |

Doppio click su una manopola = valore di default. La finestra è ridimensionabile (50–200%).

Nota: con solo PRESET acceso le Intensity non fanno nulla, come sull'originale dove la quantità del preset è fissa.

## Il modello BBD (Source/TriChorusDSP.h)

Per ogni linea: filtro anti-alias 4 poli, soft clip all'ingresso BBD, fruscio a circa -84 dBFS, ritardo modulato con interpolazione Hermite, filtro di ricostruzione 4 poli con taglio che segue il clock virtuale della BBD (1024 stadi: più ritardo = più scuro, tetto 9 kHz), low cut 70 Hz sul wet.

Tutte le costanti di voicing sono in cima alla classe `TriChorusEngine` (ritardi base 7.6 / 8.0 / 8.4 ms, sweep manuale max ±3.5 ms, frequenze del preset, livello wet, rumore). Il DSP è header-only e senza JUCE, si testa offline.

## Build

### GitHub Actions (consigliato)
Push su `main` e la CI su `windows-latest`:
1. compila VST3 + Standalone (MSVC, runtime statico, niente redistributable)
2. esegue il test DSP offline
3. valida il plugin con **pluginval** a strictness 5
4. pubblica lo zip come artifact `EPN-Trinity-Windows`

Con un tag `v1.0.0` crea anche la Release con lo zip allegato.

### In locale (Visual Studio 2022)
```
cmake -S . -B build -A x64
cmake --build build --config Release
```
Il plugin finisce in `build\EPNTrinity_artefacts\Release\VST3\EPN Trinity.vst3`, da copiare in `C:\Program Files\Common Files\VST3`. Con `-DTRINITY_COPY_PLUGIN=ON` lo copia da solo (serve la shell da amministratore).

JUCE 8.0.15 viene scaricato in automatico. Se ne hai già una copia: `-DJUCE_DIR=C:\path\JUCE`.

### Tool di sviluppo
`-DTRINITY_BUILD_TOOLS=ON` aggiunge:
- `TrinitySnapshot`: salva la GUI in PNG senza aprire una DAW
- `TrinityHostTest`: carica il .vst3 come una DAW e lo stressa (5 sample rate × 6 block size, automazione random, stato, editor)

## Struttura
```
Source/TriChorusDSP.h      motore DSP (3 BBD, LFO, pad, mix)
Source/PluginProcessor.*   parametri e wrapper JUCE
Source/PluginEditor.*      pannello rack, manopole, LED, VU
tests/render_test.cpp      test DSP offline + render WAV
tools/                     snapshot GUI e host test
.github/workflows/         CI Windows
```
