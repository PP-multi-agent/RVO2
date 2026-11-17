<!--
SPDX-FileCopyrightText: 2008 University of North Carolina at Chapel Hill
SPDX-License-Identifier: Apache-2.0
-->

# RVO2 — pornire locală (WSL / Linux)

Biblioteca simulează agenți (roboți) care se mișcă spre o țintă și se ocolesc între ei, fără să comunice. Algoritmul se numește ORCA (Optimal Reciprocal Collision Avoidance).

Nu există fereastră grafică. După compilare rulezi un program în terminal, iar el scrie, la fiecare pas, timpul simulării și poziția `(x,y)` a fiecărui agent. Când toți agenții ajung la țintă, programul se închide singur.

Codul original: <https://gamma.cs.unc.edu/RVO2/>. Licența bibliotecii este Apache-2.0, în fișierul `LICENSE`.

Lucrează în WSL sau pe Linux. Scripturile de benchmark și profiling sunt bash și nu merg în PowerShell.

Comenzile pornesc din **rădăcina proiectului**: folderul clonat, cel care conține `CMakeLists.txt`, `src/` și `examples/`.

## Windows

GLHF.

Nu rula proiectul direct pe Windows. Scripturile ar trebui rescrise ca să meargă acolo. Lucrezi în Ubuntu din WSL, apoi la secțiunea [Linux sau WSL](#linux-sau-wsl).

Pe un Windows proaspăt:

1. Repornește și intră în BIOS (la siglă, de obicei Del, F2 sau F10). Pornește virtualizarea: Intel Virtualization Technology / VT-x, sau AMD SVM Mode. Salvează și ieși.
2. În Windows, Task Manager → Performance → CPU. La Virtualization trebuie să scrie Enabled.
3. PowerShell ca Administrator:

```powershell
wsl --install
```

Comanda pune WSL2, platforma de mașini virtuale și Ubuntu. Repornește dacă ți se cere.

4. La prima deschidere, Ubuntu îți cere un utilizator și o parolă de Linux. Parola nu se vede în timp ce o tastezi. O folosești la `sudo`.
5. În PowerShell obișnuit, din folderul proiectului: `wsl`. De aici comenzile sunt cele din secțiunea următoare.

Dacă `wsl` nu e recunoscut, fă Windows Update și rulează din nou comanda de la pasul 3.

## Linux sau WSL

Pentru Debian, Ubuntu și derivate (Mint, Pop!_OS, Kali, Ubuntu din WSL).

Din PowerShell, `wsl` deschide terminalul Ubuntu. Dacă proiectul e pe discul Windows, din folderul lui rulează `wsl wslpath -u .` și folosești calea afișată (un `C:\...` este `/mnt/c/...`). În Ubuntu, din rădăcina proiectului:

```bash
bash profiling/install-tools.sh
```

Scriptul cere parola de Linux și instalează `g++`, CMake, `valgrind` și OpenMP. Dacă CMake din `apt` e mai vechi de 3.26, pune CMake 3.31 în `~/.local`.

Compilare și un exemplu:

```bash
mkdir _build-linux
cd _build-linux
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel
./examples/Blocks
```

Executabilele sunt `examples/Blocks`, `examples/Circle` și `examples/Roadmap`. Outputul într-un fișier: `./examples/Blocks > blocks.txt`. Pornește-le din `_build-linux`.

## Profiling pe WSL

Din rădăcina proiectului, după scriptul de instalare:

```bash
bash profiling/bench.sh
```

Măsoară 12 sarcini: `circle`, `blocks` și `roadmap`, câte 100, 400, 1000 și 2000 de agenți, 200 de pași, de 10 ori. Aceeași suită rulează o dată cu un thread și o dată cu toate threadurile logice. Raportul este `bench-results/<commit>/<calculator>_<cpu>/bench.txt`: CPU, memorie, iar pe fiecare linie `threads=` și `per_step_us` (microsecunde pe pas). Compară mediana, la același număr de agenți. La final, doar suita completă are o secțiune `Efficiency`: speedup-ul fiecărei sarcini împărțit la numărul de threaduri, apoi `overall`, media geometrică. `1` înseamnă că rularea paralelă a scalat cu toate threadurile. Același cod dă un număr apropiat și pe un calculator cu mai puține nuclee. Doar `bash profiling/bench.sh` scrie în `bench-results/`. O rulare pe un scenariu rămâne în `_build-prof/bench.txt`, fără efficiency.

Fiecare linie se termină cu `checksum=`, calculat din pozițiile finale ale tuturor agenților. Aceeași sarcină trebuie să dea aceeași valoare la orice rulare și cu orice număr de threaduri. Scriptul se oprește dacă două valori diferă.

Timpul cu toate threadurile depinde mult de ce mai rulează pe Windows în acel moment. Compară două versiuni doar dacă au fost măsurate una după alta, pe priză, cu aceleași programe deschise.

Dacă ai modificări locale, folderul este commitul curent. Dacă nu ai, este commitul anterior. Aceeași mașină și același commit suprascriu raportul vechi. Scriptul nu face commit; fișierele se adaugă în git când vreți.

Altă rulare:

```bash
bash profiling/bench.sh circle 200 1000
bash profiling/bench.sh blocks 200
REPEATS=3 bash profiling/bench.sh
```

Primul argument este scenariul (`all`, `circle`, `blocks`, `roadmap`), al doilea numărul de pași, al treilea numărul de agenți. Fără numărul de agenți rulează toate cele patru nivele. `REPEATS` schimbă de câte ori se repetă măsurarea.

### Callgrind

`bash profiling/bench.sh` spune cât durează un pas și ce efficiency iese. Callgrind spune în ce funcții se duc instrucțiunile. Pornește OpenMP și face aceleași 12 sarcini, cu 200 de pași, o singură dată: întâi un thread, apoi toate threadurile. Nu repetă măsurarea de 10 ori.

```bash
bash profiling/callgrind.sh
bash profiling/callgrind.sh circle 200 1000
```

Raportul este `bench-results/<commit>/<calculator>_<cpu>/callgrind-<scenariu>-<agenți>-threads-<n>.txt`. Prima linie are scenariul, agenții, pașii și `threads=`. Mai jos, funcțiile de sus sunt cele mai scumpe: `computeNewVelocity`, `insertAgentNeighbor`, `Frame::queryRecursive`, `Frame::build`. La roadmap apare și `queryVisibility`. Durează mult mai mult decât bench-ul. Efficiency rămâne în `bench.txt`, nu în callgrind.

`bash profiling/perf.sh` face același lucru cu `perf`, când kernelul îl acceptă.

### Cum rulează pasul pe mai multe threaduri

Numărul de threaduri este cel al OpenMP (`OMP_NUM_THREADS`), deci biblioteca trebuie compilată cu `-DENABLE_OPENMP=ON`. Buclele paralele nu sunt însă regiuni OpenMP. Rulează pe threadurile din `src/Team.cc`.

O regiune OpenMP se termină când toate threadurile au ajuns la barieră. În WSL, Windows oprește din când în când un CPU virtual pentru câteva milisecunde, iar tot pasul îl așteaptă. Pe laptopul de test, cu 8 threaduri, așa se pierdea de la 20% din timp în sus. În `Team`, o buclă se termină când sunt gata toți agenții, oricine i-a calculat:

- Un thread ia câte un agent, întâi din partea lui, apoi de la coada părților celorlalte. Un thread oprit ține pe loc cel mult un agent, iar bucla nu așteaptă un thread care nu ține niciunul.
- `doStep()` citește starea agenților dintr-un cadru (`Frame`, în `src/KdTree.h`) care nu se schimbă în timpul pasului și scrie starea nouă în alt cadru. Calculul unui agent scrie doar în zona de lucru a threadului (`AgentScratch`). Agentul ținut de un thread oprit este calculat încă o dată de threadul care a chemat `doStep()`, iar primul rezultat rămâne.
- Cât timp threadurile țin pasul, fiecare calculează aceiași agenți în fiecare pas, deci datele unui agent rămân în cache-ul unui singur nucleu. Starea unui agent ocupă exact o linie de cache.

Rezultatul este identic bit cu bit cu cel de pe un thread și cu cel al codului de dinainte. Asta verifică `checksum` din `bench.txt`.

Bucla din aplicație dintre pași, cea care pune viteza preferată a fiecărui agent, trece prin `simulator->forEachAgent(...)`, ca în `examples/` și în `profiling/Bench.cc`. O buclă `#pragma omp parallel for` în locul ei pornește al doilea set de threaduri, care se învârte în gol pe aceleași CPU-uri în timpul pasului. `forEachAgent` cheamă funcția exact o dată pentru fiecare agent, așa că acolo un thread oprit este așteptat.

Biblioteca cere acum C++11 (`std::thread`, `std::atomic`).

## Ce înseamnă textul afișat

Fiecare linie este un pas de simulare. Primul număr este timpul, în secunde. Pașii sunt din 0.25 în 0.25. După timp urmează câte o pereche `(x,y)` pentru fiecare agent, în ordinea în care au fost adăugați.

Exemplu de început de linie, pentru `Blocks`:

```text
0 (55,55) (-55,55) (55,-55) (-55,-55) ...
0.25 (55.48,54.52) ...
```

`0` este momentul de start: agenții sunt încă în pozițiile inițiale. Pe linia următoare, la `0.25`, coordonatele s-au schimbat pentru că au făcut un pas spre țintă, ocolindu-se între ei. Compari aceeași poziție din linie în linie ca să vezi traiectoria unui agent.

## Ce simulează fiecare exemplu

Sursa este în `examples/`. Toate trei folosesc aceeași bibliotecă din `src/`.

**Circle** (`examples/Circle.cc`) — 250 de agenți stau pe un cerc de rază 200. Fiecare vrea să ajungă în punctul de pe cerc diametral opus. Nu sunt obstacole. Se înghesuie în centru și se ocolesc. Ținta este atinsă când agentul ajunge la o distanță cel mult egală cu raza lui (1.5).

**Blocks** (`examples/Blocks.cc`) — 100 de agenți, câte 25 în fiecare colț. În centru sunt patru dreptunghiuri care lasă o trecere îngustă. Fiecare grup vrea să ajungă în colțul opus. Nu există hartă care să le spună pe unde să ocolească obstacolul; merg spre țintă și evitarea e doar locală. Un agent este „ajuns” când este la cel mult 20 de unități de țintă.

**Roadmap** (`examples/Roadmap.cc`) — aceeași scenă ca la Blocks (patru grupuri, patru obstacole), plus un graf de puncte care îi ghidează în jurul obstacolelor înainte să treacă prin trecătoare.

Ca să vezi diferența dintre Blocks și Roadmap, compară fișierele `blocks.txt` și `roadmap.txt`: la Roadmap agenții ocolesc obstacolele pe un traseu stabilit, la Blocks încearcă să treacă direct.
