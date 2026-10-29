<!--
SPDX-FileCopyrightText: 2008 University of North Carolina at Chapel Hill
SPDX-License-Identifier: Apache-2.0
-->

# RVO2 — pornire locală (Windows și Linux / WSL)

Biblioteca simulează agenți (roboți) care se mișcă spre o țintă și se ocolesc între ei, fără să comunice. Algoritmul se numește ORCA (Optimal Reciprocal Collision Avoidance).

Nu există fereastră grafică. După compilare rulezi un program în terminal, iar el scrie, la fiecare pas, timpul simulării și poziția `(x,y)` a fiecărui agent. Când toți agenții ajung la țintă, programul se închide singur. Textul afișat este același pe Windows și pe Linux.

Codul original: <https://gamma.cs.unc.edu/RVO2/>. Licența bibliotecii este Apache-2.0, în fișierul `LICENSE`.

Alege una dintre variante și urmează-o până la capăt:

- [Windows clasic](#windows) — PowerShell, CMake și Visual Studio Build Tools.
- [Linux sau WSL](#linux-sau-wsl) — Debian, Ubuntu și derivate (Linux Mint, Pop!_OS, Kali, Ubuntu din WSL), cu `apt` și `g++`.

Comenzile pornesc din **rădăcina proiectului**: folderul clonat, cel care conține `CMakeLists.txt`, `src/` și `examples/`. Fiecare și-l are în alt loc pe disc. Înlocuiește căile din exemple cu folderul tău.

Poți avea ambele compilări în același folder de proiect. Windows scrie în `_build`, Linux scrie în `_build-linux`. Cele două foldere stau unul lângă altul.

## Windows

Ai nevoie de două programe care nu vin cu Windows:

1. **CMake 3.26 sau mai nou** — pregătește compilarea.
2. **Un compilator de C++** — aici folosim Visual Studio 2022 Build Tools, cu componenta „Desktop development with C++”. Editorul (Visual Studio Code, Cursor sau altul) nu conține compilatorul.

### 1. Verifică ce ai deja

Deschide **PowerShell** (nu e nevoie de Administrator) și rulează:

```powershell
cmake --version
```

- Dacă vezi `cmake version 3.26` sau un număr mai mare (`3.27`, `3.30`, `4.x`), CMake este bun. Treci la verificarea compilatorului.
- Dacă vezi `cmake is not recognized` sau o versiune mai mică decât 3.26, instalează CMake la pasul 2.

Compilatorul nu are o comandă simplă de verificat înainte de instalare. Dacă nu ai instalat niciodată Visual Studio sau „Build Tools”, instalează-l la pasul 2.

### 2. Instalează uneltele

În același PowerShell:

```powershell
winget install --id Kitware.CMake -e --accept-package-agreements --accept-source-agreements
```

Apoi compilatorul. Comanda cere drepturi de administrator; Windows va deschide o fereastră de confirmare. Instalarea poate dura câteva minute.

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-package-agreements --accept-source-agreements --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

`--add Microsoft.VisualStudio.Workload.VCTools` este partea care pune compilatorul de C++. Fără ea, Visual Studio se instalează, dar CMake tot nu găsește un compilator.

Dacă `winget` nu este recunoscut:

- CMake: descarcă „Windows x64 Installer” de pe <https://cmake.org/download/>. La instalare bifează **Add CMake to the system PATH for all users**.
- Compilator: descarcă **Build Tools for Visual Studio 2022** de pe <https://visualstudio.microsoft.com/downloads/> (secțiunea Tools for Visual Studio). În installer bifează **Desktop development with C++**, apoi Install.

Închide PowerShell-ul complet și deschide unul nou. Altfel Windows încă folosește PATH-ul vechi și `cmake` rămâne necunoscut.

Verifică din nou:

```powershell
cmake --version
```

Trebuie să apară `cmake version 3.26` sau mai nou.

### 3. Compilează proiectul

Deschide PowerShell în rădăcina proiectului. În Explorer: click dreapta pe folderul care conține `CMakeLists.txt`, apoi **Open in Terminal** sau, pe Windows 10, Shift + click dreapta și **Open PowerShell window here**.

Verifică unde ești:

```powershell
pwd
ls CMakeLists.txt
```

`pwd` arată folderul tău. `ls CMakeLists.txt` trebuie să găsească fișierul. Dacă nu, ești într-un subfolder: urcă cu `cd ..` până când comanda îl găsește.

Apoi:

```powershell
mkdir _build
cd _build
cmake ..
cmake --build . --config Release
```

Ce face fiecare comandă:

- `mkdir _build` creează folderul în care CMake pune fișierele generate. Codul sursă rămâne neschimbat.
- `cd _build` intră în acel folder. Comenzile următoare se rulează de aici.
- `cmake ..` citește `CMakeLists.txt` din folderul de deasupra și generează un proiect Visual Studio. Exemplele `Blocks`, `Circle` și `Roadmap` sunt incluse implicit.
- `cmake --build . --config Release` compilează biblioteca și cele trei exemple. `Release` înseamnă varianta optimizată, cea pe care o rulezi.

La prima rulare, `cmake ..` poate sta câteva zeci de secunde până găsește Visual Studio. La final trebuie să vezi `Build succeeded` sau `Generating done`, fără `error`.

Dacă vrei să o iei de la capăt, din rădăcina proiectului:

```powershell
Remove-Item -Recurse -Force _build
mkdir _build
cd _build
cmake ..
cmake --build . --config Release
```

### 4. Rulează un exemplu

Executabilele apar în rădăcina proiectului, la:

- `_build\examples\Release\Circle.exe`
- `_build\examples\Release\Blocks.exe`
- `_build\examples\Release\Roadmap.exe`

Biblioteca este legată static în fiecare `.exe`. Nu trebuie copiat niciun DLL lângă ele.

Pornește cu `Blocks`. Are 100 de agenți și output-ul este mai ușor de urmărit decât la `Circle`. Dacă tocmai ai compilat, ești deja în `_build`. Altfel, din rădăcina proiectului rulează `cd _build`.

```powershell
.\examples\Release\Blocks.exe
```

Ca să nu umpli terminalul, salvează tot output-ul într-un fișier și uită-te doar la primele linii:

```powershell
.\examples\Release\Blocks.exe > blocks.txt
Get-Content blocks.txt -TotalCount 5
```

`blocks.txt` apare în `_build`. Îl poți deschide în editor. Aceleași comenzi merg pentru celelalte două programe: înlocuiești `Blocks` cu `Circle` sau `Roadmap`.

```powershell
.\examples\Release\Circle.exe > circle.txt
.\examples\Release\Roadmap.exe > roadmap.txt
```

Programul nu cere input. Așteaptă până se termină (câteva secunde, uneori mai mult la `Circle`). Când revii la prompt și nu vezi un mesaj de eroare, simularea a ajuns la final.

### 5. Dacă ceva nu merge pe Windows

**`cmake` is not recognized**  
CMake nu este în PATH. Închide terminalul, deschide unul nou și încearcă iar `cmake --version`. Dacă tot nu merge, reinstalează CMake și bifează adăugarea în PATH. Poți apela și calea completă, de obicei `C:\Program Files\CMake\bin\cmake.exe`.

**CMake Error: CMAKE_CXX_COMPILER not set** sau **could not find any instance of Visual Studio**  
Compilatorul lipsește. Repetă instalarea Build Tools din pasul 2, cu workload-ul `VCTools`. Apoi șterge `_build` și rulează din nou `cmake ..`.

**CMake 3.26 or higher is required**  
Versiunea instalată este prea veche. Instalează CMake din nou cu comanda din pasul 2.

**`.\examples\Release\Blocks.exe` is not recognized** sau fișierul nu există  
Nu ești în `_build`, sau compilarea nu s-a terminat cu succes. Din rădăcina proiectului rulează `cd _build`, apoi `cmake --build . --config Release`. Executabilul este `examples\Release\Blocks.exe`, nu direct în `_build`.

**Output foarte lung la Circle**  
Este normal: 250 de poziții pe fiecare linie, multe linii. Scrie-l în fișier, ca la pasul 4, și deschide fișierul.

## Linux sau WSL

Pașii de aici sunt pentru distribuții din familia Debian, adică cele care instalează programe cu `apt`: Debian, Ubuntu, Linux Mint, Pop!_OS, Kali și Ubuntu-ul care vine cu WSL.

Îți trebuie:

1. **CMake 3.26 sau mai nou.**
2. **`g++`**, care vine în pachetul `build-essential` împreună cu `make`.

### 1. Deschide un terminal Linux

**WSL, când Ubuntu nu este instalat.** Deschide PowerShell ca Administrator (click dreapta pe PowerShell, „Run as administrator”) și rulează:

```powershell
wsl --install
```

Windows instalează WSL și Ubuntu. Repornește calculatorul dacă ți se cere. La prima deschidere, Ubuntu îți cere un nume de utilizator și o parolă. Parola este a contului Linux. O scrii când o comandă începe cu `sudo`. Caracterele nu apar pe ecran în timp ce o tastezi; apeși Enter la final.

**WSL, când Ubuntu este deja instalat.** În PowerShell obișnuit:

```powershell
wsl
```

Promptul se schimbă din `PS C:\...>` în ceva de forma `nume@calculator:~$`. Din momentul ăsta comenzile sunt de Linux. Ieși înapoi în PowerShell cu `exit`.

**Linux instalat pe calculator** (Debian, Ubuntu, Mint, Pop!_OS, Kali): deschide aplicația Terminal.

Verifică distribuția:

```bash
cat /etc/os-release
```

La `ID=` sau `ID_LIKE=` trebuie să apară `debian` sau `ubuntu`. Exemple: `ID=ubuntu`, `ID=debian`, `ID=linuxmint` cu `ID_LIKE=ubuntu`.

### 2. Intră în folderul proiectului

**Proiect clonat direct în Linux** (în home, de exemplu `~/RVO2`):

```bash
cd ~/RVO2
pwd
ls CMakeLists.txt
```

`ls CMakeLists.txt` trebuie să găsească fișierul. Dacă nu, ești într-un alt folder: `cd` până în rădăcina proiectului.

**Proiect ținut pe discul Windows și compilat din WSL.** `C:\` se vede în WSL ca `/mnt/c/`, iar `\` devine `/`. Un folder

`C:\Users\<nume>\proiecte\RVO2`

devine

`/mnt/c/Users/<nume>/proiecte/RVO2`

`<nume>` este utilizatorul Windows, diferit pentru fiecare. Ca să afli calea ta, în PowerShell, din rădăcina proiectului:

```powershell
wsl wslpath -u .
```

Comanda afișează calea Linux. O copiezi, apoi în terminalul WSL:

```bash
cd "/mnt/c/Users/<nume>/proiecte/RVO2"
pwd
ls CMakeLists.txt
```

Pune între ghilimele calea afișată de `wslpath`, nu textul cu `<nume>`.

Compilarea direct pe `/mnt/c` merge. Fișierele de pe discul Windows se citesc mai greu din WSL, așa că o copie în directorul tău Linux termină compilarea mai repede. O faci o singură dată, cu calea reală în locul celei de mai jos:

```bash
cp -a "/mnt/c/Users/<nume>/proiecte/RVO2" ~/RVO2
cd ~/RVO2
```

Dacă lucrezi pe copia din `~/RVO2`, modificările făcute acolo rămân în Linux. Ca editorul de pe Windows și terminalul WSL să vadă aceleași fișiere, rămâi pe calea `/mnt/c/...`.

### 3. Instalează compilatorul și CMake

```bash
sudo apt update
sudo apt install -y build-essential cmake
cmake --version
g++ --version
```

`sudo` cere parola creată la pasul 1. `apt update` reîmprospătează lista de pachete. `build-essential` instalează `g++`, `gcc` și `make`. A doua comandă, `g++ --version`, confirmă că compilatorul există.

`cmake --version` trebuie să arate **3.26 sau mai mult**. Versiuni bune, deja în `apt`, pe distribuțiile curente:

| Distribuție | CMake din `apt` |
| --- | --- |
| Ubuntu 24.04 | 3.28 |
| Ubuntu 25.04 și 25.10 | 3.31 |
| Ubuntu 26.04 | 4.2 |
| Debian 13 | 3.31 |
| Linux Mint 22 (bazat pe Ubuntu 24.04) | 3.28 |

Dacă vezi una dintre versiunile de mai sus, sau orice număr de la 3.26 în sus, treci la [pasul 5](#5-compilează-proiectul).

Ubuntu 22.04 are CMake 3.22. Debian 12 are CMake 3.25. Ambele sunt prea vechi pentru acest proiect. Continuă cu pasul 4.

### 4. CMake prea vechi

Fă **una** dintre variante, după ce vezi în `/etc/os-release`.

**Ubuntu 22.04, Linux Mint 21 sau Pop!_OS 22.04.** Acestea sunt bazate pe Ubuntu 22.04 (`jammy`). Repo-ul Kitware înlocuiește CMake-ul din Ubuntu cu o versiune nouă. Lipește blocul întreg:

```bash
sudo apt-get update
sudo apt-get install -y ca-certificates gpg wget
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor - | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main' | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null
sudo apt-get update
sudo apt-get install -y cmake kitware-archive-keyring
sudo rm -f /usr/share/keyrings/kitware-archive-keyring.gpg
sudo apt-get update
cmake --version
```

Ultima linie trebuie să arate `cmake version 3.26` sau mai mare. Instrucțiunile oficiale ale repo-ului sunt pe <https://apt.kitware.com/>.

**Debian 12 (bookworm).** CMake 3.31 este în backports, depozitul Debian cu pachete mai noi, reconstruite pentru Debian 12:

```bash
echo 'deb http://deb.debian.org/debian bookworm-backports main' | sudo tee /etc/apt/sources.list.d/backports.list
sudo apt update
sudo apt install -y -t bookworm-backports cmake
cmake --version
```

`-t bookworm-backports` îi spune lui `apt` să ia CMake din backports, nu versiunea 3.25 din depozitul principal. La final trebuie să vezi `cmake version 3.31` sau mai mare.

### 5. Compilează proiectul

Rămâi în rădăcina proiectului (`pwd` arată calea ei, iar `ls CMakeLists.txt` găsește fișierul). Folderul de build se numește `_build-linux`, ca să nu se amestece cu `_build`-ul făcut de Visual Studio pe Windows.

```bash
mkdir _build-linux
cd _build-linux
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel
```

Ce face fiecare comandă:

- `mkdir _build-linux` creează folderul de compilare. Sursele rămân unde sunt.
- `cd _build-linux` intră în el. Restul comenzilor se rulează de aici.
- `cmake -DCMAKE_BUILD_TYPE=Release ..` citește `CMakeLists.txt` din folderul de deasupra și pregătește un Makefile pentru `g++`. `Release` se alege acum, la configurare. Exemplele sunt incluse implicit.
- `cmake --build . --parallel` compilează biblioteca și cele trei exemple, folosind mai multe nuclee.

La final, în terminal apare link-uirea pentru `Blocks`, `Circle` și `Roadmap`, fără cuvântul `error`.

Ca să o iei de la capăt, din rădăcina proiectului (dacă ești în `_build-linux`, urcă întâi cu `cd ..`):

```bash
rm -rf _build-linux
mkdir _build-linux
cd _build-linux
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel
```

### 6. Rulează un exemplu

Rămâi în `_build-linux`. Executabilele sunt aici, fără extensia `.exe` și fără subfolderul `Release`:

- `examples/Circle`
- `examples/Blocks`
- `examples/Roadmap`

Pe Linux, biblioteca este un fișier partajat `src/libRVO.so`, iar executabilele știu să îl găsească cât timp le pornești din arborele `_build-linux`.

Pornește cu `Blocks`. Dacă tocmai ai compilat, ești deja în `_build-linux`. Altfel, din rădăcina proiectului rulează `cd _build-linux`.

```bash
./examples/Blocks
```

Ca să păstrezi output-ul și să vezi doar începutul:

```bash
./examples/Blocks > blocks.txt
head -n 5 blocks.txt
```

`blocks.txt` apare în `_build-linux`. Din editorul de pe Windows, același fișier este `_build-linux\blocks.txt` în folderul proiectului.

Celelalte două exemple:

```bash
./examples/Circle > circle.txt
./examples/Roadmap > roadmap.txt
```

Programul nu cere input. Așteaptă să se termine. Când revii la prompt și nu apare un mesaj de eroare, simularea a ajuns la final.

### 7. Dacă ceva nu merge pe Linux sau WSL

**`wsl` is not recognized** în PowerShell  
WSL nu este instalat. Rulează `wsl --install` într-un PowerShell pornit ca Administrator, apoi repornește.

**`sudo: command not found`** sau promptul încă arată `PS C:\`  
Ești încă în PowerShell. Rulează `wsl` și așteaptă promptul `nume@calculator:~$`.

**`cmake: command not found`**  
Pachetul nu s-a instalat. Rulează din nou `sudo apt update` și `sudo apt install -y cmake`, apoi `cmake --version`.

**CMake 3.26 or higher is required**  
Versiunea din `apt` este prea veche. Urmează pasul 4 pentru Ubuntu 22.04 sau pentru Debian 12, apoi șterge `_build-linux` și configurează din nou.

**`No CMAKE_CXX_COMPILER could be found`** sau **`g++: command not found`**  
Lipsește compilatorul. Rulează `sudo apt install -y build-essential`, șterge `_build-linux` și reia pasul 5.

**`./examples/Blocks: No such file or directory`**  
Nu ești în `_build-linux`, sau compilarea s-a oprit cu eroare. `cd` în `_build-linux` și rulează din nou `cmake --build . --parallel`. Numele este `./examples/Blocks`, cu `./` în față.

**`error while loading shared libraries: libRVO.so.2`**  
Pornește programul din `_build-linux`, cu calea `./examples/Blocks`. Dacă l-ai copiat în alt folder, biblioteca a rămas în urmă. O legare statică, la fel ca pe Windows, se face regenerând build-ul din `_build-linux`:

```bash
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF ..
cmake --build . --parallel
./examples/Blocks
```

**CMake vorbește despre Visual Studio sau generatorul nu se potrivește**  
Ai intrat în `_build`, folderul făcut pe Windows. Ieși și folosește `_build-linux`, ca la pasul 5.

**Output foarte lung la Circle**  
Este normal: 250 de poziții pe fiecare linie. Scrie-l în fișier și citește-l cu `head -n 5 circle.txt`.

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
