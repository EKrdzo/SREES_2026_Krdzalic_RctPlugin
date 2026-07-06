==============================================================
  MatPower Converter Plugin (mpconv4)
  dTwin plugin za analizu toka snage u pravougaonim koordinatama
==============================================================

--------------------------------------------------------------
STA PLUGIN RADI
--------------------------------------------------------------

Plugin cita MATPOWER case fajl (.m), gradi admitansnu matricu
Y_bus i generise sistem nelinearnih jednacina (NLEs) u
pravougaonim koordinatama koji dTwin solver rjesava interno
Newton-Raphsonovom metodom.

Pravougaone koordinate:
  e_i = Re(V_i)   f_i = Im(V_i)

MATLAB nije potreban. Plugin direktno parsira .m fajl.
Rezultat je .dmodl fajl koji dTwin ucitava i rjesava.

--------------------------------------------------------------
INSTALACIJA
--------------------------------------------------------------

1. Pokreni cmake i buildi (Release x64):

   mkdir "C:\Users\<korisnik>\natID.RAMDisk\Out\MatPowerPlugin4Sol"
   cd    "C:\Users\<korisnik>\natID.RAMDisk\Out\MatPowerPlugin4Sol"
   cmake "C:\Users\<korisnik>\Desktop\Target\MatPowerPlugin4"

   Otvori .sln u Visual Studiju i buildi Release x64.

2. Kopiraj mpconv4.dll u:
   C:\Users\<korisnik>\ba.natID\dTwin\plugins\

3. Pokreni dTwin — u meniju se pojavljuje stavka "RctPlugin".

--------------------------------------------------------------
KORISCENJE
--------------------------------------------------------------

Klikni "RctPlugin" u dTwin meniju. Otvara se prozor sa dva taba:

  [ Converter tab ]

  Import model (.m):  [polje]  [...]
    - Klikni "..." i odaberi MATPOWER case fajl (.m)
    - Plugin automatski prelazi u Convert mod

  Load model / Save as:  [polje]  [...]
    - U Convert modu (odabran .m):
        labela postaje "Save as:" — upiši ime izlaznog .dmodl fajla
        ili klikni "..." da odaberes lokaciju
    - U Load modu (prazno polje):
        labela ostaje "Load model:" — odaberi postojeci .dmodl

  Status: [poruka o uspjehu ili greski]

  [Info]  [Load]  [Convert]  [Cancel]

    Info    — provjerava da li odabrani fajl postoji
    Load    — ucitava postojeci .dmodl direktno u dTwin
              (aktivan samo u Load modu)
    Convert — parsira .m, gradi Y matricu, generise .dmodl
              i automatski ga ucitava u dTwin
              (aktivan samo u Convert modu)
    Cancel  — brise sva polja i vraca u pocetno stanje

  [ Options tab ]

  Model name:  naziv koji se upisuje u .dmodl header
  Max iters:   maksimalan broj Newton-Raphson iteracija (default: 100)
  eps:         tolerancija konvergencije (default: 1e-6)

--------------------------------------------------------------
TIPICAN TOK RADA
--------------------------------------------------------------

  A) Nova konverzija (.m -> .dmodl):
     1. Klikni "..." pored "Import model (.m):" -> odaberi case14.m
     2. Plugin prelazi u Convert mod, "Save as:" se aktivira
     3. Upisi ime izlaza (npr. case14) ili klikni "..." za lokaciju
     4. Po zelji podesi Options (Model name, Max iters, eps)
     5. Klikni Convert
     6. dTwin automatski ucitava i rjesava model

  B) Ucitavanje postojeceg modela:
     1. U polje "Load model:" upisi put do .dmodl fajla
        ili klikni "..." da ga odaberes
     2. Klikni Load
     3. dTwin ucitava model bez ponovne konverzije

  C) Resetovanje:
     Klikni Cancel da ocistis sva polja i pocnes iznova.

--------------------------------------------------------------
MATEMATIKA
--------------------------------------------------------------

1. CITANJE .m FAJLA (parseMFile)
   Cita blokove: mpc.bus, mpc.gen, mpc.branch
   Podaci po tipu:
     Bus    : id, tip (1=PQ, 2=PV, 3=Slack), Pd, Qd, Gs, Bs, Vm, Va
     Gen    : busId, Pg, Qg, Vg
     Branch : from, to, r, x, b, tap, shift, status

2. GRADNJA Y MATRICE (buildY)
   Koristi natID sparse::ICmplxMatrix. Pi model transformatora:

     a    = tap * exp(j * shift_rad)
     y_s  = 1 / (r + j*x)
     y_b  = j*b/2

     Yii[fi] += (y_s + y_b) / |a|^2
     Yii[ti] += y_s + y_b
     Yij[fi,ti] += -y_s / conj(a)
     Yij[ti,fi] += -y_s / a

   Shunt admitanse iz bus podataka:
     Yii[i] += Gs_i/baseMVA + j*Bs_i/baseMVA

3. INJEKCIJE SNAGA (per unit)
     P_i = (sum(Pg) - Pd_i) / baseMVA
     Q_i = (sum(Qg) - Qd_i) / baseMVA

4. GENERISANJE NLEs (writeDmodl)

   PQ cvor (tip 1) — dvije jednacine (P i Q):
     G_ii*(e_i^2+f_i^2) + sum_j[G_ij*(e_i*e_j+f_i*f_j)
                               - B_ij*(e_i*f_j-f_i*e_j)] = P_i

     -B_ii*(e_i^2+f_i^2) + sum_j[G_ij*(f_i*e_j-e_i*f_j)
                                - B_ij*(e_i*e_j+f_i*f_j)] = Q_i

   PV cvor (tip 2) — P jednacina + naponski uslov:
     (P jednacina kao gore)
     e_i^2 + f_i^2 = Vsp_i^2

   Slack cvor (tip 3) — fiksirani parametri:
     e_slack i f_slack su Params u .dmodl fajlu (ne ulaze u NLEs)

--------------------------------------------------------------
FORMAT IZLAZNOG FAJLA (.dmodl)
--------------------------------------------------------------

Header:
    maxIter = 100
    report  = Solved
    maxReps = -1
    outToTxt = false
end

Model [type=NL domain=real eps=1e-6 name="..."]:

Vars [out=true]:
    e_2 = 1.0; f_2 = 0.0
    e_3 = 1.0; f_3 = 0.0
    ...  (sve osim slack cvora)

Params:
    e_1 = 1.06 [out=true]; f_1 = 0.0 [out=true]   (slack)
    G_i_j = ...; B_i_j = ...   (elementi Y matrice)
    P_i = ...; Q_i = ...        (injekcije snaga)
    Vsp2_i = ...                (PV naponski setpoint)

NLEs:
    G_2_2*(e_2^2+f_2^2) + ... = P_2
    -B_2_2*(e_2^2+f_2^2) + ... = Q_2
    e_6^2 + f_6^2 = Vsp2_6
    ...
end

--------------------------------------------------------------
STRUKTURA PROJEKTA
--------------------------------------------------------------

MatPowerPlugin4/
├── CMakeLists.txt          cmake konfiguracija (MatPowerPlugin4Sol)
├── MatPowerPlugin4.cmake   build pravila (DLL: mpconv4)
├── README.txt              ovaj fajl
├── res/
│   ├── DevRes.xml
│   ├── main.xml
│   └── tr/
│       ├── EN/main.xml
│       └── BA/main.xml
└── src/
    ├── MatPowerPlugin.h    Options struct, PLUGIN_API makro
    ├── MatPowerPlugin.cpp  parser, Y matrica, NLEs, plugin klasa
    ├── ViewConv.h          GUI - Converter tab
    ├── ViewOptions.h       GUI - Options tab
    ├── TabView.h           tab kontejner
    └── WindowPlugin.h      glavni prozor

--------------------------------------------------------------
STA GDJE MIJENJATI
--------------------------------------------------------------

  GUI (labele, dugmad, velicina prozora):
    -> ViewConv.h       (Converter tab)
    -> ViewOptions.h    (Options tab)
    -> WindowPlugin.h   gui::Size(800, 300) — sirina x visina

  Matematika:
    -> MatPowerPlugin.cpp
         parseMFile()   citanje .m fajla
         buildY()       gradnja Y matrice
         writeDmodl()   generisanje NLE jednacina

  Naziv u dTwin meniju:
    -> MatPowerPlugin.cpp, Plugin::getMenuName()

  Defaulti solvera:
    -> ViewOptions.h (_editMaxIter, _editEps)
    -> MatPowerPlugin.h (Options struct)

--------------------------------------------------------------
KORISCENE natID BIBLIOTEKE
--------------------------------------------------------------

  sparse::ICmplxMatrix          rijetka kompleksna Y matrica
  sparse::CmplxMatrixReleaser   RAII za sparse matricu
  fo::InFile, fo::openExistingBinaryFile,
  fo::LineNormal, fo::getLine   citanje fajlova liniju po liniju
  fo::createTextFile            pisanje .dmodl fajla
  fo::fileExists                provjera postojanja fajla
  fo::loadBinaryFile            ucitavanje fajla u memoriju
  arch::MemoryOut               in-memory buffer za .dmodl
  cnt::PushBackVector           natID vektor za tokenizaciju
  mu::ScopedCLocale             ispravno parsiranje decimala
  td::cmplx                     kompleksni broj (std::complex<double>)
  gui::*                        svi GUI elementi
  sc::IPlugin                   plugin interfejs prema dTwin-u

--------------------------------------------------------------
TESTIRAN NA
--------------------------------------------------------------

  case9.m  (IEEE 9-bus)  — konvergira za 2 iteracije
  case14.m (IEEE 14-bus) — konvergira za 2 iteracije

Naponi odgovaraju referentnim MATPOWER rezultatima.

==============================================================
