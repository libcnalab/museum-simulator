# CNA museum: audit, doplnění a ověření pokrytí

Založeno 2026-10-19. Tento soubor je průběžný pracovní seznam pro aktivní dlouhodobý goal.
Uživatel schválil realizaci všech tří částí auditu i řešení souvisejících nových nálezů.
Nečekat na další souhlas typu „may I continue“ po dokončení etapy. Pokračovat dalším
realizovatelným úkolem, dokud není splněna definice dokončení níže.

## Stav a nejbližší krok

- Aktivní goal byl vytvořen 2026-10-19 bez přidaného časového/tokenového rozpočtu.
- Audit zdrojů začal na museum `d34c4a9`, CNA `c35475ca4`; výchozí pracovní strom
  muzea byl čistý. Aktuální sestavená CNA baseline je níže v A07.
- Aktuální `build/bin/museum_tests`: 8 484 kontrol, 0 selhání; `--validate`: OK.
- Aktuální binárka: 295 místností, 72 druhů exponátů, 1 284 normalizovaných
  katalogových názvů API; 231 XNA, 63 CNA extension a 1 aplikační místnost.
- Historický `build/build.ninja` uváděl CNA `v0.1.0-351-g83c7b324c`; tato stará
  binárka není důkaz. Nová baseline A07 už byla sestavena a ověřena.
- Generovaná API tabulka odpovídala existující binárce; 36 referenčních PNG.
- Zatím nebyla provedena implementace nových exponátů. Nový soukromý OPENGLES3
  validační běh pro T01: 85 PASS, 0 FAIL, 1 INFO, 1 SKIP.
- A02 inventář a A04 drift gate jsou hotové. A03 má prvních 99 ručně kontrolovaných
  implementačních cest; dva aktuální OPENGLES3 běhy vykonaly všech 81 takto doložených
  CNA deklarací a 8 deklarací už má konkrétní výsledkový oracle.
- **Další krok:** škálovat A03/A05 call-site revizi na zbývající claimy, poté přidat
  běhové a výsledkové důkazy a první nové exponáty C01/G01.

## Pravidla práce a dokončení

1. Status úkolu: `TODO`, `IN_PROGRESS`, `DONE`, `BLOCKED_EXTERNAL` nebo `NOT_APPLICABLE`.
   `DONE` vyžaduje konkrétní důkaz; napsaný kód bez ověření není hotová funkcionalita.
   Poslední dva stavy vyžadují vysvětlení, důkaz a další postup, ne pouhé pohodlí.
2. Oddělovat: veřejné API existuje / exponát implementován / cesta vykonána /
   výsledek ověřen. Zvlášť vést emulaci, hardware, odmítnutí, fallback a SKIP.
3. Každý nový související problém doplnit stabilním ID, reprodukcí, prioritou,
   závislostí a podmínkou dokončení. Nesnažit se předem vyjmenovat hypotetické chyby.
4. Po každé ověřené dávce aktualizovat tento stav, evidenci a další krok. Po ztrátě
   kontextu navázat, neopakovat automaticky již úspěšné drahé testy.
5. Pořadí: pravdivost měření a aktuální baseline → chybějící demonstrace → kombinace
   rendererů/platforem/hardwaru → závěrečný audit. Blokér jednoho úkolu neblokuje ostatní.
6. Zachovat aplikaci nad veřejným CNA API, nebudovat náhradní engine a nezakrývat
   frameworkové chyby aplikovanými workaroundy bez pojmenování. Související chyby CNA
   minimalizovat a opravit v jeho repozitáři, pokud to dovolují oprávnění; jinak zaznamenat
   reprodukci a přesný potřebný zásah. Respektovat cizí rozpracované změny.
7. Build muzea nejvýše 2 úlohy; jediná sdílená ccache `/rv/cnaccache`, base `/rv`;
   používat existující `build`, `build-probe`, `build-consumer` a oprávněné sanitizerové
   stromy. Žádná kompilace v `/tmp`, žádný zbytečný clean rebuild ani nové klony závislostí.
8. Grafické testy na privátním displeji, nikdy na živém desktopu. Nespouštět více
   náročných testů/buildů současně. Pro skutečné GPU použít odpovídající privátní runner.
9. Fixture obsah, lokální testovací účty a izolované datové adresáře. Automatické
   testy nesmějí skenovat osobní Music/Pictures, používat soukromé účty nebo posílat
   zprávy skutečným lidem. Systémové dialogy a zařízení spouštět v produktu explicitním
   tlačítkem; nelze je automaticky aktivovat obecným `pressall` bez řízeného režimu.
10. Nezhoršovat testy, tolerance ani seznam referencí pro dosažení zeleného výsledku.
    Nepoužívat SKIP jako PASS; historické výsledky zachovat s datem a revizí.
11. Dokončení: všechny realizovatelné položky mají ověřený výsledek, všechny aktuální
    veřejné oblasti jsou explicitně klasifikované, dokumentace je aktuální, relevantní
    regrese prošly. Externě neproveditelné ověření má hotovou dostupnou implementaci,
    doložený blokér a opakovatelný návod. Žádné neověřené tvrzení „100 % CNA funguje“.
12. Samotný uplynulý čas není důvod skončit. Limity účtu/prostředí nelze obejít;
    pokud zasáhnou, zachovat přesný stav rozpracování. Goal neoznačit za dokončený
    jen proto, že zbyla obtížná práce.

## A — úplný inventář a měřitelné pokrytí (priorita P0)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| A01 | DONE | Uchovat závěry výchozího auditu, revize, omezení a konkrétní backlog. Důkaz: tento plán, výchozí kontroly výše. |
| A02 | DONE | Doxygen 1.9 inventář `tools/generate_api_inventory.py` pokrývá všech 699 veřejných hlaviček ve všech modulech a C ABI: 19 851 deklarací (12 941 C++, 6 910 C) na CNA `df9234202`, public-header SHA-256 `16f94e…a69`. Explicitně vylučuje 225 internal/detail hlaviček a 40 deklarací v internal/detail namespace, zahrnuje veřejnou fasádu `IntermediateSerializer` fyzicky uloženou v detail hlavičce a eviduje 70 veřejných deklarací s internal/detail typem v signatuře. Deterministický JSON je `build/api-audit/public-api.json`, tracked souhrn `docs/CNA_API_INVENTORY.md`; `--write`, `--check` a 6 unit testů prošly. |
| A03 | DONE | Implementační revize je úplná: `audit/implementation_evidence.json` má 1 864 ručně zkontrolovaných záznamů (direct/indirect, unikátní anchor, stable ID, podmínky, poznámka) a pokrývá všech 1 780/1 780 claim-room dvojic; 1 343 je úplných, 437 poctivě částečných (volán jen getter/setter nebo některé overloady), 1 355 deklarací má implementační důkaz. Resolver nyní nikdy nerozšíří argumentově kvalifikovaný claim na celou rodinu (dříve `Texture3D.GetData(level, box)` → 6 overloadů; unit test). Běhové a výsledkové úrovně pokračují v A08/A09/T05. |
| A04 | DONE | Reprodukovatelné `generate_api_inventory.py --{write,check}` a `audit_api_coverage.py --{write,check}` hlásí nepřiřazené deklarace po modulech, chybné/nejednoznačné claimy, stale důkazy a konkrétní delta přidaných/odstraněných stable ID. `check_all.sh` má tři nemutující source gates pro generované docs, inventář a coverage evidence; čistý build smí vytvořit pouze ignorovanou JSON cache. Unit testy dokazují properties/EXT/overloads, namespace výluky, drift stable ID a že nová nekatalogovaná deklarace zůstane `unverified`; CTest 6/6. Suffix index zachoval bitově stejnou matici a zkrátil coverage check přibližně z 30 s na 0,58 s. Jmenovatel i tři stupně důkazu jsou oddělené. |
| A05 | DONE | Všech 1 274 claimů prošlo call-site revizí. Revize našla 56 nepravdivých claim-room dvojic: 30 odstraněno z katalogu (např. SpriteBatch v 11 particle místnostech, Game/GraphicsAdapter v info, DynamicVertexBuffer v sky, rozhraní jen v textu), 3 přejmenovány na skutečně volané API (`NetworkSession.Begin/EndJoinInvited`, `RasterizerState.CullCounterClockwise`, `Texture3D.GetData(level, left, front)`) a 23 opraveno tak, že kód API skutečně používá: in-pad identita (5 GamePad EXT dotazů + GamePadType, readout byl vždy „-“), `KeyboardState.IsKeyDown`, `Vector3.Lerp/SmoothStep`, `MathHelper.WrapAngle`, `DepthStencilState.DepthRead`, `ContentManager.RootDirectory`, `GraphicsResource.IsDisposed`, `EffectPass`, `IPackedVector(T)` a `IShadowReceiverEXT` přes rozhraní. Opraveny i skutečné chyby: rt-lowres sampler přepisovaný `SetOpaque3D`, mrtvé ovladače Depth format/Mip levels v rt-basic (a následně MiniView mazání hloubky u `DepthFormat::None`, odhalené tourem). hist-xna/vr-index nyní poctivě uvádějí jen `SpriteBatch.DrawString`. |
| A06 | DONE | `audit/area_classification.json` (121 revidovaných pravidel, first-match) klasifikuje každou veřejnou hlavičku bez katalogového claimu: 143 gap (s backlog ID, např. C API O04/O05 = 59 hlaviček/6 910 deklarací, MediaLibrary M01/M02, build engine C06), 157 supporting a 24 not-applicable s konkrétním důvodem; 369 hlaviček má aspoň jeden claim. `audit_api_coverage.py --check` selže při neklasifikované hlavičce nebo zastaralém pravidle (unit test); tabulka oblastí je v `docs/CNA_COVERAGE_AUDIT.md`. Dvě oblasti bez backlog domova (platform identity, XmlSerializer adaptéry) jsou nové úkoly G10/G11. |
| A07 | DONE | Nová ověřená binární baseline 2026-10-19: CNA `705ea2324` (`v0.1.0-467`, čistý strom při buildu; pozdější cizí dirty změny jsou jen c-api testy a dokumentace), inventář i coverage připnuty, declaration delta +0/-0, public-header SHA `16f94e…a69`. Build 2 joby OK, museum `7a624369…eb62f`, libcna `340a18e3…6904`, `museum_tests` 8 484/0, `--validate` OK, CTest 7/7, renderer self-check OPENGLES3 85 PASS/0 FAIL/1 INFO/1 SKIP. Každá další změna zdrojů muzea mění hash binárky: execution runy se proto opakují až po dávce A05 oprav (A08). |
| A08 | IN_PROGRESS | `audit/execution_evidence.json` a tracked run artefakty vážou datum, příkaz, privátní displej/platformu, explicitní OPENGLES3, build volby, museum/CNA revize a hlavně SHA-256 skutečných museum/libcna binárek. Audit počítá jen run se shodnými aktuálními binárkami; pozdější rebuild záznam zachová historicky, ale vyřadí z current counts. Validátor podporuje jak úplný `pressall`, tak přesné scripted controls s pořadím a waits; aktuálně jsou 2/2 runy current. Zbývá stejná provenance pro další profily a hardware. |
| A09 | IN_PROGRESS | Report a JSON oddělují catalogue link, implementation, execution a correctness. Execution vyžaduje hashovaný script/log, přesné místnosti a interakce, čistý stats marker a nulové exhibit/draw chyby; correctness vyžaduje samostatný exact witness + očekávání. Aktuální stav: 81 executed CNA declarations, 8 correctness-verified CNA declarations, 6 úplných correctness claimů a 7 dvojic. Zbývá sjednotit PASS/FAIL/SKIP/INFO/UNVERIFIED i v ostatních runnerech. |
| A10 | TODO | Porovnat tematické a členské pokrytí s aktuálním `cna-demos` katalogem (719 demo ID) a jeho 155položkovou issue maticí. Muzeum nemusí kopírovat každý diferenciální test, ale žádná celá oblast ani známé omezení nesmí zmizet a známé selhání nesmí být označeno jako PASS jen kvůli katalogovému claimu. Evidenci jiného repozitáře připnout k jeho revizi a nepovažovat ji za běh muzea. |
| A11 | DONE | Všech 70 veřejných deklarací s internal/detail typem v signatuře má revidovanou kategorii v `audit/internal_type_classification.json`: 24 `constraint-concept` (Texture3D/TextureCube Get/SetData, veřejné API), 25 `renderer-plumbing` (GetRenderer, FillGpuDrawParams, lease… — není aplikační API, muzeum je nevolá, nepočítá se jako mezera), 18 `pipeline-internal-type`, 2 `public-alias-or-proxy`, 1 `defaulted-internal-parameter`. `generate_api_inventory.py --check` selže při nové neklasifikované nebo už neprosakující deklaraci (unit test); tabulka je v `docs/CNA_API_INVENTORY.md`. Jediné užití muzeem: `ContentCompiler::Compile` (5 exponátů čte `bytes`, CompilerLab i `rootReaderName`) = upstream blokér: CNA nemá veřejný typ výsledku; potřebný zásah je veřejný `CompiledXnb { bytes, rootReaderName }` nebo alias v pipeline namespace. CNA vlastní jiný agent, proto jen evidováno. |

## T — důvěryhodné ověřovací nástroje (P0)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| T01 | DONE | `renderer_check.sh` odmítá prázdný seznam místností, selhání enumerace, chybějící log, prázdné/neúplné výsledky, předčasný exit 0, draw/exhibit chyby, nenulové chyby ve stats a nedokončené místnosti. Důkaz: 10 regresních testů v `tests/test_validate_renderer_log.py`; starý 85/0/1/1 log bez stats je odmítnut; nový privátní OPENGLES3 běh 2026-10-19 dosáhl stats a skončil 85 PASS, 0 FAIL, 1 INFO, 1 SKIP, exit 0. |
| T02 | DONE | `check_all.sh` používá sdílený `tools/_steps.sh`: exit 77 se tiskne jako `skip` s důvodem, nikdy jako ok; `ONLINE=1` je vyžadovaný profil, takže skip online kontroly je FAIL; poslední řádek je souhrn passed/failed/skipped/not-run včetně neprovedených SLOW/ONLINE částí. `online_stack.sh start` vrací 3 jen pro nesestavenou službu, 1 pro selhání provisioningu/portu; `online_check.sh` mapuje jen 3 na skip, jiné selhání startu je FAIL. 4 unit testy `tests/test_check_steps.py` (CTest `museum_check_steps_unit`). |
| T03 | DONE | `gen_docs.sh --check` generuje očekávaný obsah do `build/gen_docs`, porovná pouze své bloky/soubor a nepřepisuje zdroje ani nekontroluje nesouvisející git diff. Update režim zůstal výchozí. Tři izolované testy ověřují write→check bez změny hashů, stale odmítnutí bez přepisu, chybějící marker a binárku; CTest registruje `museum_generated_docs_unit`. `check_all.sh` používá nový režim. |
| T04 | IN_PROGRESS | Tour: ověřit všechny požadované místnosti a dokončení, unknown room/command a chyby `pressall`; auditovat catch bloky, které jen logují nebo ignorují chybu. Explicitní očekávané odmítnutí oddělit od selhání. |
| T05 | TODO | Funkční scénáře s očekávanými hodnotami, ne pouze `pressall`. Sekvence Inspector Start→Connect→Read, síť host→join→send, audio play→observe atd. Každý nový exponát má alespoň jednu výsledkovou kontrolu. |
| T06 | TODO | Audio testovací profil s aktivním audio systémem (tour používá `--no-audio`), kontrola PCM/eventů/stavu a samostatná evidence fyzického poslechu. Dummy driver není důkaz slyšitelnosti. |
| T07 | IN_PROGRESS | Audit nav/reference/room_diff/leak/web/online runnerů: timeout, procesní cleanup, staré logy, dostupnost závislostí, chybové kódy, izolace a nepřepisování aktivních běhů. Přidat jen konkrétně odůvodněné regrese. |
| T08 | IN_PROGRESS | Souhrnný výsledek každého testovacího profilu s počty splněno/selhalo/přeskočeno/neověřeno; částečně zelená konfigurace nesmí tvrdit úplné pokrytí. |
| T09 | DONE | Reconfigure s `CNA_BUILD_TESTS=OFF` nesmí nechat CTest načíst starý CNA test manifest. CMake nyní odstraní pouze stale child `CTestTestfile.cmake`; zapnutí CNA testů jej znovu generuje. Reprodukce před opravou: `CnaTests[1]_include.cmake` zahrnoval sám sebe a CTest překročil hloubku 1000. Důkaz po opravě: child manifest chybí a oba museum CTesty prošly 2/2. |

## C — obsah a pipeline (P1)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| C01 | TODO | MeshHelper lab: CalculateNormals/TangentFrames, MergeDuplicatePositions/Vertices, OptimizeForCache, TransformScene; skeleton Find/Flatten na malém modelu. Ukázat rozdíl geometrie/deklarace a ověřit numerické výsledky. |
| C02 | TODO | Skutečný procesorový kontext/dependency scénář: AddDependency, AddOutputFile, Convert, BuildAsset a BuildAndLoadAsset. Použít CNA pipeline službu, pokud existuje; nevytvářet druhý content engine. Současné no-op/identity/throw v PipelineSupport nesmějí představovat úplné pokrytí. |
| C03 | TODO | Vlastní binární ContentTypeWriter + ContentTypeReader: compile→load round trip vlastního datového objektu, nested/shared/external reference dle skutečného veřejného kontraktu; odmítnutí poškozeného vstupu. |
| C04 | TODO | Reálný MaterialProcessor a návazné textury; ověřit hodnoty i render výsledného materiálu, ne jen přeposlání ContentObject. |
| C05 | TODO | WmaImporter a zbývající veřejné importery/procesory podle A02, legální reprodukovatelné fixtures, přiznaná dostupnost codeců a validace výsledku. |
| C06 | TODO | CNJ/CNB/XNB matice: veřejné build/load routes, komprese, konfigurace, manifest, cache/incremental rebuild a clean, selhání závislosti. Zvlášť odlišit interní codec detaily od veřejného API. |
| C07 | TODO | ResourceContentManager, vlastní reader/serializer a lifecycle: ověřit reálné volání, disposal/shared resource chování a registraci/unregistraci včetně opakované návštěvy. |
| C08 | TODO | glTF importní scénáře dle aktuálního CNA: podporovaná rozšíření, Draco podle buildu, UV0/UV1, specular/IOR, transformy jednotlivých map, topology, sparse/morph/skinning/material variants; nepodporované required extensions musí mít jmenované odmítnutí. Nevydávat implementaci application instancingu za EXT_mesh_gpu_instancing import. |

## G — grafika, runtime, matematika (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| G01 | TODO | Vlastní IVertexType registrovaný přes VertexTypeRegistryEXT; vytvořit buffer z System::Type, vykreslit a ověřit chybu neznámého typu. |
| G02 | TODO | Doplnit skutečný setter sampleru model part a správné claimy; porovnat efekt změny a obnovit cached asset při opuštění. |
| G03 | TODO | Sampler3D lab rozšířit na dostupné WGSL/SPIR-V/HLSL cesty dle rendereru, ne pouze GLSL; storage round trip a shader sampling evidovat odděleně. |
| G04 | TODO | Compiled XNA effects na současných rendererech (EasyGL, FNA3D, DirectX9, SDL_GPU, WebGPU, volitelně SOFTWARE). Effect reflection, vertex texture sampling, instancing a load/compile cesty reálně ověřit; bez fxc použít dodané legální bytecode fixtures, source compile podmíněně. |
| G05 | TODO | Window lab: TextDropEXT, MinimizeEXT/RestoreEXT, orientation a focus/lifetime události; ověřit skutečné události v privátním okně a bezpečný návrat z minimalizace. |
| G06 | TODO | DeviceLost/Reset/ContentLost a obnova prostředků. Pokud platforma událost neumí vyvolat, nepředstírat skutečnou ztrátu; veřejná testovací cesta může být zvlášť označená emulace. |
| G07 | TODO | Matematika/design/runtime member audit podle A02: doplnit chybějící smysluplné overloady, chyby/hranice, události a lifecycle; známý CNA NotImplemented BoundingFrustum.Intersects(Ray) vykázat jako omezení frameworku, ne opomenout. |
| G08 | TODO | Veřejná diagnostika renderer capabilities/limits/formats: porovnat tvrzení s vykonanou operací; rozlišit unsupported/restricted/unknown a skutečnou chybu deklarované podpory. |
| G10 | TODO | Platform identity readout: `CNA::getCurrentPlatform()`, `isMobilePlatform()`, `getCurrentDesktopOS()` a `CNA_TARGET_*` v pl-platform; výsledek porovnat s buildem. |
| G11 | TODO | XmlSerializer round trip XNA hodnot (Vector/Matrix/Color/Keys adaptéry) např. nastavení ve StorageContainer; ověřit přesné hodnoty po načtení. |
| G09 | TODO | PBR/shadow/instancing a render-state kombinace: z auditních mezer vytvořit diskriminační pixelové kontroly, ověřit na více rendererových rodinách, chyby minimalizovat v CNA. |

## I — vstup, zařízení, systémové integrace (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| I01 | TODO | Haptics/HapticDevice: otevření, jednoduchý rumble, create/update/run/stop/destroy efektu, status, gain/autocenter, pause/resume. Ukázat capabilities a konkrétní odmítnutí bez hardwaru. Vždy stop/cleanup při odchodu a výjimce. |
| I02 | TODO | FileDialog, MessageBox: explicitní akce, success/cancel/error, filtry, vlastnictví/callbacky a návrat do hry. Automatický scénář na izolovaném displeji; obecný pressall nesmí zablokovat tour. |
| I03 | TODO | SystemTray: ikona/menu/eventy a odstranění; prostředí bez tray evidovat jako unavailable. UrlLauncher: lokální testovací URL a řízená akce, bez svévolného otevírání externích stránek. |
| I04 | TODO | Kamera: seznam zařízení, výběr, otevření/start/frame/stop/dispose dle veřejného API, práva a odpojení. Automaticky netvrdit skutečný obraz při absenci kamery. |
| I05 | TODO | Gamepad/joystick/touch/IME/clipboard audit rozšíření a hotplug; emulované a fyzické vstupy oddělit, obnovit změněný globální stav při odchodu. |
| I06 | TODO | Accelerometer/gyro/compass/motion/vibration: připravit konkrétní fyzické scénáře, kalibraci/události/stop dle inventáře; emulované hodnoty i odmítnutí popsat pravdivě. |
| I07 | TODO | Izolovaný manuální hardware checklist: zařízení/model, platforma, kroky, očekávané výsledky a důkazy. Chybějící hardware není důvod odložit dostupnou implementaci a test refusal/cleanup větví. |

## M — média, audio a storage (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| M01 | TODO | MediaLibrary s izolovanými fixture kořeny (Music/Pictures), alba/artists/genres/songs/playlists/pictures a ukládání obrázku. Nejprve ověřit současnou veřejnou možnost volby kořenů; je-li jen test hook, navrhnout korektní CNA rozšíření nebo izolovaný proces. Žádné tiché čtení osobních knihoven. |
| M02 | TODO | MediaLibrary chybové/empty/change scénáře a property/collection API; doložit skutečnou enumeraci a round trip, ne pouhé vystavení tříd. |
| M03 | TODO | Audio výstupní cesta: SoundEffect, instance, spatial, synth, BufferNeeded, XACT/RPC a MediaPlayer eventy; deterministicky ověřit dostupný výstup/PCM, lifecycle a opakované vstupy do místnosti. |
| M04 | TODO | Microphone capture, BufferReady, start/stop a audio loopback s řízeným fixture zdrojem, kde to prostředí umožní; fyzický mikrofon/poslech vést odděleně. |
| M05 | TODO | Video: end/loop/pause/seek, texture, audio sync a disposal dle veřejné plochy; OFF/AUTO/ON varianty a explicitní nedostupnost v browseru/ostatních targetech. |
| M06 | TODO | StorageDevice/StorageContainer chyby, odpojení, async lifecycle, události a izolace rootu; oprávnění/kapacitu simulovat jen jasně označenou cestou. Obnovit a uklidit pouze vlastní fixtures. |

## N — networking, hráči a telefonní notifikace (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| N01 | TODO | Lokální HttpNotificationChannel room: Open/URI/Find, raw a toast send, DispatchPendingNotificationsEXT, receive, Close a pozdní/invalid request. CNA již má loopback endpoint; externí Microsoft push služba není podmínkou. |
| N02 | TODO | Notifikace: opakované otevření, queue/thread ownership, nedostupný endpoint, cleanup během odchodu a přesné pojmenování omezení shell toastu. |
| N03 | TODO | Host migration: 3 privátní procesy, připojit, ukončit hosta, potvrdit nový HostChanged a pokračující přenos. Samotný přepínač AllowHostMigration nestačí. |
| N04 | TODO | Party ≥3 testovacích účtů, invite/accept/leave a přesné počty; session invite/join-in-progress/private slots, host removal a disconnect/reconnect. |
| N05 | TODO | Voice: zjistit skutečnou veřejnou/implementovanou podporu CNA, vytvořit řízený lokální přenos a mute/talking scénář, je-li dostupný. Pokud je API pouze stub, doložit frameworkový blokér a nepřidávat imitaci hlasu. |
| N06 | TODO | Packet overloady, reliabilita, ordering/latency/loss, QoS a session events dle A02 s ověřením na obou stranách. |
| N07 | TODO | Online fixture/harness: životnost vlastní služby, nejméně 3 izolované účty, žádné kolize s jinými relacemi, jasné příčiny skip/fail a cleanup po timeoutu. |
| N08 | TODO | GamerServices/avatars/Guide/achievements/leaderboards členský audit, trial/privileges/sign-out a async chyby; skutečný serverový round trip, ne pouze lokální stav. |

## O — diagnostika, Inspector a nativní C API (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| O01 | TODO | Inspector: cílený úspěšný Start→Connect→Ping→Snapshot→Events scénář; špatný token, timeout/disconnect a teardown bez leak/race. |
| O02 | TODO | Inspector preview: implementovat dostupnou veřejnou provider cestu a získat Ready+skutečná data, pokud CNA kontrakt dovoluje; odlišit Unavailable. Externí cna-inspector bridge a browser UI ověřit proti muzeu. |
| O03 | TODO | Diagnostics OFF/STATS/FULL build/runtime profily, trace serialization, overflow/lost events, registrace/odregistrace source/resource a teardown dle inventáře. Vyřešit FORCE FULL vs tvrzená vypnutelná konfigurace. |
| O04 | TODO | C API samostatná volitelná demonstrační cesta přes skutečné C17 TU/C ABI, ne C++ napodobeninu. Nejprve zmapovat veřejnou dostupnost aktuálního CNA_C_API a lifecycle/handle boundaries. |
| O05 | TODO | C API demonstrace relevantních rodin: verze/chyby/handles, math, resource/data transfers, input/audio/content a dostupné další subsystémy; sestavení C klienta a přenos skutečných výsledků do muzea nebo jeho companion nástroje. Žádné tiše vzniklé language bindings mimo C. |
| O06 | TODO | Logging, exceptions a diagnostické členy: ověřit dosažitelné chyby, zprávy, callback/observer lifetime. Neprovokovat nebezpečné hardwarové selhání jen pro naplnění seznamu. |

## P — rendererové, platformní a hardwarové ověření (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| P01 | TODO | Aktuální native baseline: structure + functional scenarios + tour + nav + pixel checks + reference + leak. Zapsat změnu čísel proti výchozí revizi. |
| P02 | TODO | OPENGLES3 a OPENGL33: nové i stávající cesty včetně compiled effects; odlišit software GL od fyzické GPU. |
| P03 | TODO | SOFTWARE: dostupné schopnosti a explicitní skips, compiled-effect volba, numerické/pixelové kontroly bez předpokladu shader source podpory. |
| P04 | TODO | FNA3D, WEBGPU, SDL_GPU: functional/tour/validation a nové exponáty; respektovat schopnosti a portovat aplikaci tam, kde chybí jen shaderový dialekt. |
| P05 | TODO | HEADLESS/STUB: správné refusal/skip a úspěšné datové operace; STUB odmítnutí startu nesmí být úspěšný grafický test. |
| P06 | TODO | WEBGL2: kompletní tour, všechny validační místnosti, dostupné nové scénáře, persistence a cleanup. Dosavadní smoke není full coverage. |
| P07 | TODO | Browser WEBGPU: zjistit dostupnost toolchainu a podpory; sestavit/spustit odpovídající profil, bez nahrazení WEBGL2 výsledku tvrzením o WebGPU. |
| P08 | TODO | Windows DIRECTX9/SOFTWARE a další dostupné renderery: Wine výsledek označit Wine, skutečná Windows ověření zvlášť. Nepřepsat aktivní web build v build-consumer bez kontroly. |
| P09 | TODO | macOS/Metal přes podporovaný renderer, Android/iOS a senzory: připravit portabilní build/scénáře a doložit, co zde skutečně lze ověřit. Nepřidávat zpět vyřazené CNA renderer identity. |
| P10 | TODO | Feature-off kombinace: networking/Inspector/pipeline/video/CNAEXT/devices/diagnostics, jejich link closure a truthful notices. Zakázaná feature není pokrytá functional testem default buildu. |
| P11 | TODO | ASan/UBSan po relevantních změnách lifetime; TSan pro notifikace, audio, síť, Inspector. Cílené mutace nejdůležitějších oracle kontrol, ne bezdůvodné opakování všech drahých sestavení. |
| P12 | TODO | Fyzické GPU/audio/gamepad/haptics/camera/touch/sensors/hotplug: provést dostupné ověření, zbytek vést jako konkrétní externí blokéry s postupem. |

## D — dokumentace, UX exponátů a závěrečné ověření (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| D01 | TODO | CNA_COVERAGE/README/HANDOFF/RENDERER_TESTING: opravit neplatné tvrzení FNA3D-only compiled effects, staré počty a už implementované online služby. Historické výsledky nepřepisovat jako nové. |
| D02 | TODO | Označit CNA_API_NOTES jako historický orientační podklad; opravit odkazy na vyřazené renderery a významné dnes nepravdivé rady. Hlavičky aktuálního CNA jsou výchozí zdroj. |
| D03 | TODO | Každý nový exponát: katalog, API evidence, registry, ovládání, vysvětlení očekávaného výsledku, absence závislosti, cleanup; žádné prázdné místnosti s pouze marketingovým názvem. |
| D04 | TODO | Po přidání místností regenerovat mapy/counts/coverage, zkontrolovat otočené místnosti, přístupnost dveří/stairs/elevators a referenční kompozice. Nové screenshoty pořizovat z reálného běhu. |
| D05 | TODO | Průvodce ověřením: příkazy, potřebné verze/dependencies/fixtures, očekávaný výsledek a hardwarové checklisty. Jasně odlišit automatický a manuální profil. |
| D06 | TODO | Závěrečný audit proti aktuálním veřejným hlavičkám, novým API od baseline a všem nálezům. Žádná neklasifikovaná oblast nebo otevřený realizovatelný úkol; report zvlášť chybějící implementace a chybějící důkazy. |
| D07 | TODO | Závěrečná kontrola diffů, změněných files, testů a revizí; finální stav plánu obsahuje konkrétní hotové výsledky a zbývající externí omezení. Goal dokončit pouze při splnění stanoveného cíle. |

## Nově nalezené úkoly a průběžná evidence

- 2026-10-19: T03 vznikl při čtení `check_all.sh`: kontrola dokumentace nyní spouští
  přepis a `git diff --quiet -- README.md docs`, takže selže i kvůli legitimní ruční
  změně docs. Opraveno `gen_docs.sh --check`; gate nyní porovnává jen generovaný obsah,
  je krytý třemi izolovanými testy a spolu s oběma API audity běží v `check_all.sh`.
- 2026-10-19: T04: `MuseumGame.cpp` loguje výjimky při `pressall` pod kategorií script;
  ověřit, že se tyto chyby neztrácejí vedle zvlášť počítaných exhibit/draw errors.
- 2026-10-19: T02/T07/N07: `online_check.sh` převádí jakýkoli neúspěšný start stacku
  na skip „service is not built“, používá sdílený adresář a neověřuje explicitně status
  `wait $museum` před kontrolou databáze. Přidat odlišení skutečných selhání.
- 2026-10-19: A02: CNA `tools/c-api/generate_coverage_inventory.py` používá Doxygen,
  nikoli regexp scanner; existující C ABI scope záměrně vynechává mimo jiné platformní
  a build-time oblasti. Je to vhodný podklad, ale ne automaticky úplný jmenovatel muzea.
- 2026-10-19: A02 dokončen vlastním širším Doxygen inventářem. První verze chybně
  vynechala veřejný `IntermediateSerializer`, protože je fyzicky definován v detail
  hlavičce; úzká fasádní výjimka jej přidala. Opačný filtr odstranil 40 helper deklarací
  v `detail`/`Internal` namespace, i když leží ve veřejné hlavičce. Úplný JSON navíc
  uchovává seznam 70 veřejných signatur odkazujících na interní typy pro A11.
- 2026-10-19: A03/A05: první konzervativní resolver našel 36 ambiguous, 43 unresolved
  a 4 nedeklarační labely. Po unit-tested podpoře XNA properties/EXT accessorů a ruční
  opravě call-site claimů je stav 0/0/0; sedm SharpRuntime `System.*` claimů je explicitně
  mimo CNA jmenovatel. Katalogový link stále není implementační ani běhový důkaz.
- 2026-10-19: A03 získal strojově ověřovaný manifest implementačních důkazů. Prvních
  99 záznamů propojuje claim, místnost, stable declaration ID a přesný unikátní anchor;
  zdrojový hash zahrnuje i nepřímou cestu přes `MuseumGame.cpp`. Výsledek je 81 skutečně
  implementovaných deklarací a 69 úplných claimů. Regresní test odhalil a opravil chybu,
  kdy čtení chybějícího klíče v `defaultdict` mohlo nafouknout počet na celý inventář.
  Další test brání tomu, aby se různé symboly jedné rodiny z různých místností chybně
  sloučily do úplného claimu; audit nyní počítá i 89/1 813 ověřených claim-room dvojic.
- 2026-10-19: během A03 se CNA posunulo z `82b1bf60c` na `60b6b747f`. Drift gate
  správně odmítl stale souhrn, ale declaration delta byla +0/-0 a hash všech veřejných
  hlaviček zůstal `16f94e…a69`; inventář a coverage evidence byly připnuty znovu.
- 2026-10-19: CNA následně commitnulo tutéž Curve implementaci jako `d753d0997` a
  přidalo nový test zachycený globem. Úplný reconfigure/rebuild 138 kroků prošel;
  veřejný hash a počet deklarací se opět nezměnil, `libcna.so` zůstal bitově shodný.
- 2026-10-19: resolver A04 původně skenoval 19 851 deklarací pro každý z 1 284
  claimů. Jednorázový index všech kvalifikovaných suffixů zachoval přesně stejný JSON
  a zrychlil `audit_api_coverage.py --check` zhruba z 30 sekund na 0,58 sekundy.
- 2026-10-19: A08/A09 mají první reprodukovatelný execution/correctness řetězec.
  Explicitní OPENGLES3 tour prošel 15 zrevidovaných místností, 69 panel controls a
  skončil s `exhibit errors 0`; po změně CNA Matrix byl zopakován se shodným log SHA.
  Audit ověřuje hash skriptu/logu, pořadí místností, `pressall`, stats a SHA obou binárek.
  Druhý scripted-controls run s waits oddělil JPEG, PNG a oba sized FromStream režimy.
  Celkem se započítává 81 executed a 8 correctness-verified CNA declarations; oracles
  ověřují přesné pixely/rozměry i ResourceManager missing-asset refusal.
- 2026-10-19: A03/A05: všech 14 claimů místnosti `ct-compiler` má zkontrolovanou
  direct/indirect cestu a execution důkaz ze současné binárky. Revize zahrnuje vytvoření
  compileru a options, platform/profile/compression pole, importer/processor/content typ,
  type writer i chráněnou `ReadAsset`/`OpenStream` cestu přes `MemoryContentManager`.
- 2026-10-19: A03/A09: `app-gametime` je zrevidováno 5/5 včetně getteru i setteru
  `InactiveSleepTime` a nepřímých volání `ResetElapsedTime`/`SuppressDraw` z hlavní
  smyčky. Hashovaný běh dokládá, že po 400ms stall a resetu dostal další Update přesně
  `ElapsedGameTime 0.0 ms`; tento witness je pátý correctness oracle.
- 2026-10-19: A03: `rt-formats` je 3/3 implementačně i běhově ověřeno: šest hodnot
  `SurfaceFormat`, dotaz `SupportsSurfaceFormatAsRenderTargetEXT` a porovnání skutečného
  `Texture.Format`. Výsledky jsou zatím pouze v panelu, proto nemají correctness oracle.
- 2026-10-19: A05/A11: ze zdrojů muzea odstraněna všechna explicitní jména
  `CNA::Internal`. `CompilerLab` nabízel none/LZ4/LZX, ale veřejný `ContentCompiler`
  interní volbu vždy přepsal podle boolean `compressContent`, takže LZ4 ve skutečnosti
  nikdy nevzniklo. UI nyní pravdivě nabízí none/LZX a všechny výsledky `Compile` používají
  `auto`; nezbytný přístup k polím interního návratového typu zůstává evidovaný upstream leak.
- 2026-10-19: běhové testy během prvního auditu použily starou binárku. Bez nové
  kompilace netvrdit, že dnešní CNA změny prošly; viz A07/A08.
- 2026-10-19: T01 opraven. Původní `build/renderer_check/FNA3D.checks` byl prázdný,
  ale starý runner rozhodoval pouze podle počtu FAIL a mohl vrátit úspěch. Nový parser
  vyžaduje návštěvu a výsledek každé `rv-*` místnosti, stats marker a čistý process/log.
- 2026-10-19: A10: souběžný CNA repair campaign eviduje 719 samostatných demo ID a
  155 historických issue ID v `../cna/CNA_DEMOS_REPAIR_{HANDOFF.md,MATRIX.json}`.
  Jeho rozsáhlejší členské/oracle pokrytí je vhodný kontrolní zdroj, nikoli náhrada
  za vykonaný museum scénář; část issue je stále pouze `PENDING_VERIFICATION`.
- 2026-10-19: A07 odhalil řízené odstranění `GraphicsBackendMaturity` v CNA commitu
  `91408ad08` bez forwarding headeru/aliasu. Muzeum převedeno na dnešní renderer názvy
  a `toStringView`, takže neudržuje neúplné pole enum hodnot (chyběl `Deprecated`).
- 2026-10-19: T09 odhalil stale CTest podstrom z dřívější konfigurace s CNA testy.
  Přímý `museum_tests` byl zelený, ale root CTest nemohl ani enumerovat. Lokální CMake
  cleanup je omezen na konfiguraci `CNA_BUILD_TESTS=OFF`; žádný CNA zdroj nebyl změněn.

- 2026-10-19 (druhá relace): A03 škálováno osmi paralelními revizemi po exponátech. Do manifestu
  přibylo 1 741 záznamů (celkem 1 840); validátor (unikátní anchor, symboly jen z claimu, direct =
  jeden krok ve zdroji místnosti) prošel. Implementačně zrevidováno 1 757/1 813 dvojic, z toho 1 325
  úplně; 1 339 deklarací má implementační důkaz. Zbylých 56 dvojic jsou nepravdivé claimy (53 nalezeno
  revizí + 3 rozhraní používaná jen přes konkrétní override), řeší je A05. Revize našla i skutečné
  chyby: rt-lowres sampler přepsán `SetOpaque3D` (ovladač nemá účinek), in-pad „Identity“ vždy „-“,
  rt-basic mrtvé ovladače Depth format/Mip levels, ct-cache mrtvý `IsDisposed` cyklus, rv-effects
  nekreslí DualTextureEffect. Spuštění v aktuální binárce zatím chybí (A08: 0/2 runů current po
  rebuildu).
- 2026-10-19: T04: neznámá místnost/příkaz, chybný argument `pos/at/look/walk/floor`, nenalezený
  `press` a výjimka v `pressall` jsou nyní `[museum][script-error]` (resp. exhibit-error), počítají
  se v řádku stats (`script errors N`). `tour.sh` selže, když nebyly navštíveny všechny požadované
  místnosti nebo byla script chyba; renderer i execution validátor script chyby odmítají (testy).
  Zbývá audit catch bloků v exponátech.
- 2026-10-19: T07: `_headless.sh` zamyká log (`flock`), takže druhý běh do stejného `run.log` je
  odmítnut místo prolnutí; `MUSEUM_LOG` umožní paralelní běh. `online_check.sh` má vlastní stack
  (`build/online-check`, port 47832), trap cleanup, kontrolu exit statusu muzea a exhibit chyb.
  `nav_test.sh` vyžaduje čistý stats řádek, `room_diff.sh` maže staré snímky a vrací chybu při
  selhání běhu, `web_smoke.sh` ověří, že HTTP server opravdu běží, a odmítá script/draw/exhibit chyby.

## Obnovení práce

Přečti aktuální stav nahoře, `git status`, příslušné instrukce a log poslední rozpracované
položky. Ověř běžící procesy dřív, než začneš nový build nebo použiješ sdílený build tree.
Vyber nejbližší odblokovaný úkol. Napiš důkaz do plánu, když ho dokončíš, a přejdi dál.
Pokud v tomto vlákně goal zůstává aktivní, není potřeba od uživatele nové zadání.
