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
| A10 | DONE | `tools/compare_cna_demos.py` → `docs/CNA_DEMOS_COMPARISON.md`, připnuto k cna-demos `1a8d88c1`, SHA katalogu a matice. Mapuje API 719 dem a 155 issue na deklarace inventáře týmž resolverem: per kategorie počty deklarací dem a jejich úroveň v muzeu; 24 podkategorií dem bez jediného claimu muzea a každá má klasifikaci oblasti (gap s úkolem / supporting / n-a); 117 otevřených nebo neověřených issue s úrovní důkazu muzea pro tytéž deklarace — žádná z nich nemá correctness oracle muzea, nic není PASS jen díky katalogu. Evidence cizího repozitáře není běh muzea. |
| A11 | DONE | Všech 70 veřejných deklarací s internal/detail typem v signatuře má revidovanou kategorii v `audit/internal_type_classification.json`: 24 `constraint-concept` (Texture3D/TextureCube Get/SetData, veřejné API), 25 `renderer-plumbing` (GetRenderer, FillGpuDrawParams, lease… — není aplikační API, muzeum je nevolá, nepočítá se jako mezera), 18 `pipeline-internal-type`, 2 `public-alias-or-proxy`, 1 `defaulted-internal-parameter`. `generate_api_inventory.py --check` selže při nové neklasifikované nebo už neprosakující deklaraci (unit test); tabulka je v `docs/CNA_API_INVENTORY.md`. Jediné užití muzeem: `ContentCompiler::Compile` (5 exponátů čte `bytes`, CompilerLab i `rootReaderName`) = upstream blokér: CNA nemá veřejný typ výsledku; potřebný zásah je veřejný `CompiledXnb { bytes, rootReaderName }` nebo alias v pipeline namespace. CNA vlastní jiný agent, proto jen evidováno. |

## T — důvěryhodné ověřovací nástroje (P0)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| T01 | DONE | `renderer_check.sh` odmítá prázdný seznam místností, selhání enumerace, chybějící log, prázdné/neúplné výsledky, předčasný exit 0, draw/exhibit chyby, nenulové chyby ve stats a nedokončené místnosti. Důkaz: 10 regresních testů v `tests/test_validate_renderer_log.py`; starý 85/0/1/1 log bez stats je odmítnut; nový privátní OPENGLES3 běh 2026-10-19 dosáhl stats a skončil 85 PASS, 0 FAIL, 1 INFO, 1 SKIP, exit 0. |
| T02 | DONE | `check_all.sh` používá sdílený `tools/_steps.sh`: exit 77 se tiskne jako `skip` s důvodem, nikdy jako ok; `ONLINE=1` je vyžadovaný profil, takže skip online kontroly je FAIL; poslední řádek je souhrn passed/failed/skipped/not-run včetně neprovedených SLOW/ONLINE částí. `online_stack.sh start` vrací 3 jen pro nesestavenou službu, 1 pro selhání provisioningu/portu; `online_check.sh` mapuje jen 3 na skip, jiné selhání startu je FAIL. 4 unit testy `tests/test_check_steps.py` (CTest `museum_check_steps_unit`). |
| T03 | DONE | `gen_docs.sh --check` generuje očekávaný obsah do `build/gen_docs`, porovná pouze své bloky/soubor a nepřepisuje zdroje ani nekontroluje nesouvisející git diff. Update režim zůstal výchozí. Tři izolované testy ověřují write→check bez změny hashů, stale odmítnutí bez přepisu, chybějící marker a binárku; CTest registruje `museum_generated_docs_unit`. `check_all.sh` používá nový režim. |
| T04 | DONE | Script runner: neznámá místnost/příkaz, chybné argumenty a nenalezený `press` jsou `[museum][script-error]` počítané ve stats; výjimka v `pressall` je exhibit-error. `tour.sh` ověřuje návštěvu všech požadovaných místností. Audit všech 282 catch bloků (`scratchpad/t04_catch_audit.md` shrnut zde): 194 očekávaných odmítnutí, 57 polykaných chyb nyní hlášeno přes nový `Exhibit::ReportError`/`Museum::ReportExhibitError` (počítá se do „exhibit errors“, místnost běží dál), 22 teardown guardů, 9 reportovacích cest; únik handlerů v PlatformLab opraven. První úplný tour po auditu odhalil skutečnou chybu muzea (net-packets: `Stream.Write(null, 0, 0)` pro prázdný paket), opravenou; tour 305 místností pak čistý. |
| T05 | DONE | `tools/scenarios.py` + `tests/scenarios/*.json` (přesné očekávané řádky, regexy, zakázané řetězce, build přepínače → SKIP, `env`/`touch` pro izolované prostředí, kontrola exit statusu, stats a chybových markerů). Scénáře: inspector, notifications, vertextype, meshhelper, content_cache, diagnostics, video, model_sampler, storage_contracts, storage_not_connected, clipboard_facades, legacy_accelerometer, 8× workshop_*, plus scénáře C API. Výsledkové runnery mimo scénáře: devices_check, audio_check, voice_check, net_check, online_check. Single-room scénáře nahrazují sweep v per-room coverage běhu. |
| T06 | DONE | `tools/audio_check.sh` spouští muzeum se zapnutým zvukem přes SDL ovladač „disk“, který zapisuje přesný smíchaný výstup zařízení (formát čte z řádku SDL v logu: S16LE, 2 kanály, 44 100 Hz). Analýza nejhlasitějšího úseku (numpy FFT): generovaný zvon v au-generate má dominantu 880,0 Hz, sinus A4 syntezátoru (DynamicSoundEffectInstance) 440,0 Hz, a s `SoundEffect.MasterVolume = 0` je výstup úplně tichý (peak 0) i při přehrání chirpu. Dokazuje mix a doručení vzorků; fyzickou slyšitelnost z reproduktoru dokládá jen manuální kontrola (I07/P12). |
| T07 | IN_PROGRESS | Dříve: zámek logu v `_headless.sh`, vlastní stack `online_check.sh`, stats v `nav_test.sh`, mazání starých snímků v `room_diff.sh`, kontrola serveru ve `web_smoke.sh`. Nově: `tools/build.sh` zamyká strom (`flock`), takže dva buildy jednoho stromu se nepřekryjí; `leak_check.sh` počítá i script chyby (nedostupná místnost znehodnotí měření); `reference.sh` maže předchozí snímek před pořízením a odmítne snímek bez čerstvého souboru nebo s exhibit/draw/script chybou; `execution_coverage.py` bere provenance na začátku, odmítne zastaralou binárku a nic nezapíše, když se zdroje během běhu změnily. Zbývá ověřit upravené runnery skutečným během (po stabilizaci buildu). |
| T08 | IN_PROGRESS | `tools/record_profile.py` + `tools/profile_summary.py`: každý runner (check_all, scenarios, renderer_check po rendererech včetně -gpu, devices, audio, voice, net, online, web_smoke) zapíše počty passed/failed/skipped/unavailable/not-run s SHA-256 binárky do `<build>/profiles/`; souhrn ukáže tabulku 20 profilů, výsledek starší binárky jako „stale“, chybějící jako „not run“ a „complete“ řekne jen když je vše aktuální a bez selhání (jinak exit 1). Zbývá naplnit závěrečnou sadou. |
| T09 | DONE | Reconfigure s `CNA_BUILD_TESTS=OFF` nesmí nechat CTest načíst starý CNA test manifest. CMake nyní odstraní pouze stale child `CTestTestfile.cmake`; zapnutí CNA testů jej znovu generuje. Reprodukce před opravou: `CnaTests[1]_include.cmake` zahrnoval sám sebe a CTest překročil hloubku 1000. Důkaz po opravě: child manifest chybí a oba museum CTesty prošly 2/2. |

## C — obsah a pipeline (P1)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| C01 | DONE | wk-meshhelper (PipelineChecksLab): krychle po plochách přes MeshBuilder; 11 přesných kontrol PASS: MergeDuplicatePositions 36→8, MergeDuplicateVertices 36→24 se zachováním trojúhelníků, CalculateNormals přesně podle algoritmu XNA (součet normál trojúhelníků na pozici; moje původní očekávání stejné váhy stěn bylo chybné, ověřeno v xna4-decomp), tečny/binormály a jejich kolmost, OptimizeForCache zachová množinu trojúhelníků, SwapWindingOrder obrátí všechny, TransformScene posune svět o 2 m, FindSkeleton/FlattenSkeleton. Doplněno: místnost wk-animation – AnimationContent z XImporteru (arm.x: kanály, časy a transformace klíčů, délka), řazení/členství/odmítnutí AnimationChannel, VertexCollections (BoneWeight, IndexCollection, PositionCollection); `NormalizeWeights` ponechá nulovou váhu (XNA ji zahodí) = FAIL, nález 10. |
| C02 | DONE | wk-processor: CNA má veřejný kontext s plnými službami XNA (`XnaBridgeProcessorContext` nad `ContentPipeline`), místnost ho používá; vlastní importer/procesory registrované `RegisterXnaImporter/RegisterXnaProcessor/RegisterXnaXnbOutput`. 18 PASS: vlastnosti kontextu, AddDependency, AddOutputFile, Convert na stock MaterialProcessor, BuildAsset (jména plaque_0, stabilita, explicitní jméno), BuildAndLoadAsset vlastního typu, vnořené výstupy, logger, načtení .xnb s externími referencemi. 6 FAIL = potvrzené divergence CNA (kolize vnořených jmen, TextureProcessor alias vrací CNA typ, typy výjimek Convert a AddDependency) v `docs/CNA_FINDINGS.md`; INFO přiznává, že museum `PipelineSupport::ProcessorContext` nestaví nic. Doplněno: PipelineComponentScanner ve wk-processor (7 PASS, 1 FAIL: `Update` vrací false při změněném seznamu, nález 11). |
| C03 | DONE | wk-typewriter: vlastní ContentTypeWriter/ContentTypeReader round trip přes MemoryContentManager, každé pole bitově, polymorfní vnořený objekt, null, sdílený zdroj odkazovaný dvakrát (stejný objekt po fixupech), varianta LZX; odmítnutí špatného magic, zkráceného souboru, TypeVersion a neregistrovaného readeru (ContentLoadException). 23 PASS, 3 FAIL (ReadRawObject<Int32>, zkrácené tělo → EndOfStreamException místo ContentLoadException, XNB v4 přijata) v nálezech. `ReflectiveTypeReader<T>` zatím nepředveden. Doplněno: ReflectiveTypeReader<T> ve wk-typewriter nad fixture `reflective.xnb` v rozložení ReflectiveWriteru XNA (7 PASS: základní typ, string, enum, TimeSpan, Color, vlastní krok, sdílený čtenář, null člen, odmítnutí SharedResourceField, selhání po odebrání čtenářů). |
| C04 | DONE | wk-material: MaterialProcessor výchozí hodnoty, zachování vstupu/hodnot, `Textures["Texture"]` → `<out>/plaque_0.xnb`, načtená textura s přesnými pixely (magenta → průhledná, 3 mipy), parametry k TextureProcessor, 4 změny DefaultEffect, Process(null) → ArgumentNullException. 15 PASS, 2 FAIL (EffectMaterialContent bez/s prázdným efektem) v nálezech. |
| C05 | DONE | wk-importers: WmaImporter (atribut, chybějící soubor, kanály, Duration = PCM), PassThroughProcessor (týž objekt), BuildTimeMedia DecodeAudio pro MP3 a WMA (frekvence ±1 %, resampling zachová délku), odmítnutí, ProbeVideo, ProbeAudioDurationTicks, EncodeWindowsMediaAudio → decode, XMA služba. 22 PASS + 5 INFO dostupnosti (FFmpeg dostupný, XMA encoder nepřipojen). Fixtures `tools/gen_workshop.py` → `Content/Workshop/`. |
| C06 | DONE | wk-routes (Cnj, SoundEffect/ProcessedSoundEffect, Song, Video, Model, Effect, Xnb, XnaXml routy s načtením výsledku), wk-incremental (ContentBuildConfiguration, ContentBuildManifest se SHA-256, inkrementální rozhodnutí podle otisků, selhání závislosti, CnjToCnb, CnbSourceImport, CnbChunkCompression None/Lz4 odmítnuto/Zstd), wk-tasks (ContentTask, BuildXact, XnaComponentNames) a wk-contentproject (ContentProject s Condition, BuildContent: první build, druhý build bez změny nic nepřestaví ani nepřepíše, změna vector.xml přestaví jen vector.xnb, RebuildAll, odmítnutí; GetLastOutputs; CleanContent smaže přesně své výstupy a manifest a nechá cizí soubor; RunContentCompiler build/clean s kódy 0/1/2). Výstupy jen v build stromu (kontrolováno). Muzeum linkuje `cna_content_compiler`. Scénáře workshop_* (8, PASS). Nálezy 13 (glTF s vloženými obrázky → nenačitatelný Model .cnb), 14 (DecodeWavAsCnbSoundEffect vs. dokumentace), 15 (TaskItem::operator== nekompiluje), 16 (task hlavičky jen přes cna_content_compiler), 17 (clean nechá `.cna-content.lock`). |
| C07 | DONE | wk-cnbloader: FNV-1a id přepočítané nezávisle, CnbWriter/CnbByteWriter → CnbDocument, .cnb v build stromu, `CnbLoaderRegistry::Register`, `ContentManager.Load<Fossil>` přesně, odmítnutí (vestavěné id, jméno nesedí s id, prázdný loader, nesoulad jména, podvržený soubor s přepočteným CRC, poškozený chunk, Load po Remove), registrace odebrána v OnLeave; vlastní ContentTypeSerializer XML round trip a InvalidContentException. 21 PASS, 1 INFO (veřejně nelze serializer odregistrovat). Doplněno: ContentTypeSerializerAttribute ve wk-cnbloader (1 PASS, 1 FAIL: dvě instance nejsou Equals/Match, nález 12). |
| C08 | DONE | wk-gltf: UV0/UV1 ve vertex bufferu, texCoord jednotlivých map vč. KHR_texture_transform override, transformy, IOR a specular, sparse POSITION i TEXCOORD_0 bez bufferView, husté i sparse morph targety s váhami, neznámé required rozšíření odmítnuto jménem, used-only načteno a hlášeno UnsupportedFeature; EXT_mesh_gpu_instancing hlášeno jako DroppedData (INFO, nepočítá se jako instancing). 21 PASS, 2 INFO. |

## G — grafika, runtime, matematika (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| G01 | DONE | rv-vertextype (ValidationLab): vlastní IVertexType registrovaný přes VertexTypeRegistryEXT, VertexBuffer i DynamicVertexBuffer z `System::Type`, data jako POD stejného layoutu (IVertexType i Color jsou polymorfní, `SetData<T>` je odmítne při překladu), kreslení a pixelové ověření (40,200,120 uvnitř), Resolve neregistrovaného typu → ArgumentException. 5 PASS + 1 INFO (sizeof 8 vs stride 20) na OPENGLES3; místnost je v `renderer_check` všech rendererů. Zpráva CNA obsahuje nedemanglované jméno typu (drobný nález). |
| G02 | DONE | model-sampler: volba „Left quad: ModelMeshPart.SetSamplerStateEXT(0, …)“ (ze souboru / PointClamp / LinearMirror) volá skutečný setter `setSamplerStateEXTProperty`; getter vrátí nastavenou hodnotu, levý quad se vykreslí do 64×64 render targetu se samplerem souboru (LinearMipPoint Wrap) a s aktuálním samplerem dílu a porovná: Clamp 2 834 a Mirror 2 819 z 3 721 osvětlených pixelů se liší, návrat na sampler souboru 0. Model je cachovaný asset ContentManageru, proto se sampler při odchodu (OnLeave), reloadu i zničení vrací (kontrola PASS) a volba se resetuje. Claim `ModelMeshPart.SamplerStateEXT` + implementační záznam, scénář `tests/scenarios/model_sampler.json` (PASS). |
| G03 | DONE | tex-volshader vybírá zdroj v dialektu rendereru a tlačítko „Check“ zvlášť ověří storage (`Texture3D.GetData` = SetData, 32 768 texelů) a shader sampling (řez 20 do cíle 32×32, Point/Clamp, každý pixel). Storage PASS na OPENGLES3, OPENGL33, SOFTWARE, FNA3D, WEBGPU, SDL_GPU. Sampling: GLSL ES (OPENGLES3) PASS, WGSL podle descriptor kontraktu CNA (volume v `@group(1) @binding(8)`, sampler 40, skaláry `@group(3)`) na WEBGPU PASS (0 z 1024), SOFTWARE a FNA3D nespouští zdrojové shadery (SKIP, pravdivě). SDL_GPU: žádné zdokumentované rozložení GLSL 450 volume nenavzorkuje (nativní: svazek se nenaváže; přenosný fragment + nativní vertex: pád procesu; přenosné obojí: nic se nevykreslí, v tour pád) – místnost na SDL_GPU volume shader nespouští a uvádí proč, nález v `docs/CNA_FINDINGS.md`. Scénář `volume_shader`. |
| G04 | DONE | `build-probe` má zapnuté `CNA_EASYGL_COMPILED_EFFECTS`, `CNA_WEBGPU_COMPILED_EFFECTS`, `CNA_SDL_GPU_COMPILED_EFFECTS` (MojoShader z `~/deps/FNA3D`). fx-compiled tlačítko „Check“ (scénář `compiled_effects`, vyžaduje volbu): reflexe blur2tap (Weight scalar, Offset float2, technika Blur s 1 průchodem) a jeho aritmetika (Weight 0,5 → 200,100,40, Weight 0,25 → 100,50,20 na plochém 200,100,40) PASS na OPENGLES3, FNA3D, WEBGPU, SDL_GPU; vs_3_0 vertex texture: tvar grafu se mění s VertexSamplerStates[2] PointClamp/PointWrap PASS na OPENGLES3 a FNA3D, na WEBGPU (formát Vector4) a SDL_GPU (vertexové vzorkování) odmítnuto → SKIP + nález. Instancing kompilovaného efektu (rv-effects) PASS na všech čtyřech. Kompilace ze zdroje .fx potřebuje fxc (CNA_FXC, na Linuxu přes wine) – volba v místnosti, bez fxc pravdivě odmítnuto. |
| G05 | BLOCKED_EXTERNAL | dev-window: TextDropEXT odběr a zobrazení, MinimizeEXT/RestoreEXT (ruční; v testu vrátily), DeviceLost/Resetting/Reset čítače. Skutečný drop textu a fokus/lifetime v privátním okně zbývá (xdotool drag není k dispozici). Blokér: skutečný drag textu z jiného programu a fokus na skutečné ploše (na privátním displeji není správce oken ani zdroj XDND; xdotool drag neumí) – manuální krok v `docs/HARDWARE_CHECKLIST.md` („Text drag and drop“, „Window minimise“). |
| G06 | BLOCKED_EXTERNAL | Události DeviceLost/Resetting/Reset odebírány pravdivě (0 bez skutečné ztráty). Emulace jasně označená: `RenderTarget2D.NotifyContentLostEXT` na vlastním cíli místnosti → ContentLost 1×, IsContentLost true, po nabindování false (ověřeno). Skutečná ztráta zařízení vyžaduje hardware/driver (P12). Blokér: skutečná ztráta zařízení vyžaduje reset ovladače nebo uspání stroje – manuální krok „Device loss“ v `docs/HARDWARE_CHECKLIST.md`. |
| G07 | DONE | Matematika: místnost math-checks (163 by-ref overloadů bitově proti hodnotovým + hraniční kontrakty XNA; 168 PASS, 1 FAIL = `BoundingFrustum.Intersects(Ray z vnějšku)`, nález v `docs/CNA_FINDINGS.md`). Runtime: `Main.cpp` deklaruje `AssemblyTitleAttributeEXT{"CNA 3D Museum"}` a `ProjectGraphicsProfileEXT{HiDef}` jako XNA projekt (atribut, projektové nastavení); konstruktor hry už titulek ani profil nenastavuje a zaznamená výchozí hodnoty; app-device kontroluje, že první `Window.Title` = `GetAssemblyTitleEXT()` a první profil GraphicsDeviceManageru = `GetProjectGraphicsProfileEXT()` (obě PASS). Modul design nemá veřejné API (jen Internal). |
| G08 | DONE | Místnost rv-capabilities (floor 9, v renderer_check všech rendererů): pro každou deklaraci profilu `GetRendererCapabilityProfileEXT().GetFeature` provede operaci: Supported musí fungovat, Unsupported musí odmítnout, Restricted/Unknown jako INFO. Na OPENGLES3 7/7 souhlasí: WireFrame Unsupported → odmítnuto, OcclusionQueries, MRT (druhý cíl smazán na 10,200,30), Float32 RT (15/16 hodnot gradientu mezi 8bit kroky), Float16 přes HdrBlendable (aditivně 2,000; ostatní float formáty blending v HiDef odmítají stejně jako XNA), Texture3D storage, MSAA 4×. Chybějící dotaz na readback render targetu evidován jako nález (HEADLESS odmítá NotSupportedException). |
| G10 | DONE | dev-identity: getCurrentPlatform/Name, isMobile/isApple, CNA_TARGET_* makra, getCurrentDesktopOS, kategorie běžícího rendereru proti kompilačnímu výchozímu (`getCurrentGraphicsRendererCategory` je constexpr výchozího buildu, ne běžící zařízení), SetUnsupported3DGraphicsCallBehavior s obnovením. Ověřeno: „Linux, Desktop, OPENGLES3 is Native“. |
| G11 | DONE | wk-xml: bitově přesné XmlSerializer round tripy Vector2/3/4, Quaternion, Matrix včetně INF/-INF/NaN, Keys jménem, vnořený záznam nastavení, odmítnutí. 16 PASS, 2 FAIL: CNA nemá XML členy pro Color a Rectangle (XmlSerializer<Color/Rectangle> nejde přeložit) – nález. |
| G09 | DONE | Stencil (rv-depth: Replace/Equal, NotEqual, Increment×2) PASS na OPENGLES3, OPENGL33, SOFTWARE, FNA3D, WEBGPU, SDL_GPU. PbrEffect (rv-shading, skutečná kamera): OPENGLES3/OPENGL33/SOFTWARE/FNA3D PASS (červený dielektrik 151,10,10, kov 255 proti 148, emise 10,255,10); WEBGPU vrací neosvětlenou základní barvu (MetallicFactor bez účinku) = FAIL, SDL_GPU odmítne PbrEffect pro VertexPositionNormalTexture (jen 48/60B vrcholy s tangentou) = INFO; obojí v `docs/CNA_FINDINGS.md`. Instancing kompilovaným efektem PASS na všech čtyřech (G04). Stíny: CNAEXT ShadowLab bez pixelové kontroly – mimo rozsah XNA API. |

## I — vstup, zařízení, systémové integrace (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| I01 | BLOCKED_EXTERNAL | Místnost dev-haptics (DeviceLab): GetHapticsEXT, OpenEXT, rumble, constant effect create/update/run/status/stop/destroy, gain/autocenter/pause/resume, StopAll a Dispose při odchodu; vše otevírající zařízení je ruční tlačítko. `tools/devices_check.sh` ověřil cestu bez zařízení: OpenEXT na neznámé id vrací zavřené zařízení, rumble/efekt/gain jsou odmítnuty. Výchozí konstruktor `HapticDevice` nejde mimo CNA přeložit (CNA-DEMOS-ISSUE-009 reprodukováno, workaround `std::optional`). Zbývá fyzické ověření (I07/P12). Blokér: na stroji není haptické zařízení – manuální krok „Haptic device“ v `docs/HARDWARE_CHECKLIST.md`. |
| I02 | BLOCKED_EXTERNAL | dev-dialogs: ShowOpenFile/ShowSaveFile/ShowOpenFolder s callbackem a filtry, MessageBox.ShowSimple a Show s tlačítky, vše ruční tlačítka (sweep je přeskakuje: `ControlPanel::AddManualButton`). `devices_check.sh` na privátním Xvfb bez session D-Bus s podvrženým zenity: všechny tři dialogy vrátily přesnou cestu callbackem, ShowSimple zobrazen a zavřen, Show vrací -1 při zavření (CNA neoznačuje žádné tlačítko jako výchozí pro Return; volba tlačítka klikem zůstává UNAVAILABLE). Blokér: skutečné portálové/nativní dialogy a výběr tlačítka klikem vyžadují živou plochu se session D-Bus, kterou automatické testy nesmí použít – manuální kroky „Real file dialogs“ a „Message box with a pointer click“. |
| I03 | BLOCKED_EXTERNAL | dev-tray: tray ikona se dvěma položkami, checkable/enable přepínače, odstranění; UrlLauncher na lokální soubor. `devices_check.sh`: UrlLauncher předal `file://…/Textures/checker.png` podvrženému xdg-open (nic se neotevřelo); tray: `IsSupported` je true, ale vytvoření bez StatusNotifier hostu selže (`no SNI support`) – UNAVAILABLE v izolovaném testu, nesoulad IsSupported/runtime evidován. Blokér: na privátním displeji není StatusNotifier host a prohlížeč se automaticky neotevírá – manuální kroky „Desktop with a system tray“ a „Browser“. |
| I04 | BLOCKED_EXTERNAL | dev-camera: AvailableCameras, otevření, TryAcquireFrame do Texture2D, stav. Stroj má skutečnou kameru (`Integrated Camera`); výpis ověřen, otevření je soukromé a v `devices_check.sh` proběhne jen s `MUSEUM_ALLOW_CAMERA=1` (zatím neprovedeno). Blokér: kamera stroje je soukromá; otevření jen se souhlasem vlastníka (`MUSEUM_ALLOW_CAMERA=1 tools/devices_check.sh`) – manuální krok „Camera“. |
| I05 | DONE | in-text: tlačítko „Both clipboard facades agree“ – `CNA::Devices::Clipboard` a `CNA::Input::Clipboard` vidí jednu schránku oběma směry (PASS, scénář `clipboard_facades`); místnost si před prvním zápisem zapamatuje návštěvníkovu schránku a při odchodu ji vrátí (zalogováno). Emulace (klávesnice → gamepad, myš → dotyk), rozměry/gesta TouchPanelu a kurzor se v `RestoreGlobals` vrací při odchodu. Fyzický hotplug gamepadu/joysticku, gyro/touchpad a IME na skutečném zařízení jsou v `docs/HARDWARE_CHECKLIST.md` (tady žádné zařízení). |
| I06 | DONE | in-phone: zastaralá WP7 událost `Accelerometer.ReadingChanged` vedle `CurrentValueChanged`: klávesnicová emulace dovolí senzor spustit a `InjectSyntheticSensorUpdate` (CNAEXT) mu předá jedno jasně označené EMULOVANÉ čtení (0, 9,80665, −9,80665 m/s²): obě události přijdou v pořadí CurrentValueChanged → ReadingChanged, X/Y/Z přesně 0/1/−1 g, stejný Timestamp (scénář `legacy_accelerometer`, PASS). Bez telefonu jsou `IsSupported` false a skutečná čtení/kalibrace/vibrace jen v checklistu (fyzické scénáře pro gyroskop, kompas, Motion a vibraci). |
| I07 | DONE | `docs/HARDWARE_CHECKLIST.md`: pro každé zařízení (fyzická GPU, reproduktory, mikrofon, gamepad/gyro/touchpad, haptika, joystick, dotyk, kamera, tray, nativní dialogy, MessageBox klikem, prohlížeč, senzory telefonu, hotplug displeje, minimalizace, drag textu, ztráta zařízení, Windows/macOS/mobil) místnost, kroky, co pozorovat a jaký důkaz uložit; přesné blokéry tohoto prostředí včetně GPU (AMD Phoenix/RADV existuje, ale Xvfb nemá DRI3, takže Vulkan nemůže prezentovat; postup s headless westonem). |

## M — média, audio a storage (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| M01 | DONE | dev-medialib (MediaLibraryLab) čte knihovnu jen ručním tlačítkem. Veřejné API nemá volbu kořenů (override je `CNA::Internal`), proto izolovaný proces: `devices_check.sh` nastaví HOME/XDG_CONFIG_HOME na kopii `Content/MediaLibrary` (generuje `tools/gen_media_library.py`: 3 FLAC s tagy, M3U playlist, 2 alba obrázků). Ověřeno přesně: 3 písně / 2 alba / 2 interpreti / 2 žánry, tagy a délky, playlist Favourites = Sunrise, The Archive, 4 obrázky ve 2 albech s rodičem, SavePicture zapsal 2×2 obrázek do izolované složky, Dispose → IsDisposed; nic nevzniklo v reálném HOME. |
| M02 | DONE | Enumerace, property/collection API a SavePicture (M01). `tools/devices_check.sh` má druhý proces muzea s prázdným HOME: první čtení `0 / 0 / 0 / 0 …, 0 playlist(s), 0 / 0`; skript pak (až po zalogování předchozího čtení, bez závodu) nakopíruje fixture → `3 / 2 / 2 / 2, 1, 4 / 2`; odebere skladbu a přidá album obrázků → `2 / 1 / 1 / 1, 1, 5 / 3`, položka playlistu bez souboru vynechána („Favourites: Sunrise“); po SavePicture → `6 / 4` s albem „Saved Pictures“. Každé čtení je nová instance MediaLibrary, takže změna obsahu mezi čteními je vidět. devices_check 33 PASS / 0 FAIL / 4 UNAVAILABLE. |
| M03 | DONE | `tools/audio_check.sh` (SDL disk ovladač, PCM analyzováno numpy): generovaný zvon 880,0 Hz, DynamicSoundEffectInstance (BufferNeeded) sinus 440,0 Hz, MasterVolume 0 = úplné ticho, SoundEffect.Play s pan −1/+1 = chirp v levém/pravém kanálu (poměr 173/148), XACT `GetCue`+`Play` cue Tone = 220,0 Hz, s kategorií Default na hlasitosti 0 neslyšet (RMS 0,023 proti 0,171), MediaPlayer song 1 = arpeggio (392,0 Hz) s MediaStateChanged a ActiveSongChanged, mikrofon (M04). 20/20 PASS. Životnost a opakované vstupy do místností pokrývá tour ×3 v leak_check (0 chyb, stejný počet textur); prostorový zvuk (AudioEmitter/Listener) a slyšitelnost reproduktorem jsou v HARDWARE_CHECKLIST. |
| M04 | DONE | au-mic čte data jako v XNA až po `BufferReady` (dřív GetData každý snímek, takže buffer nikdy nebyl plný a BufferReady nepřišel) a při zastavení (tlačítkem i odchodem – mikrofon už nezůstává zapnutý po opuštění místnosti) zaloguje počet BufferReady, bajty, RMS a frekvenci z průchodů nulou. `tools/audio_check.sh` podstrčí SDL disk ovladači jako mikrofon soubor s tónem 500 Hz (S16LE mono 44,1 kHz): 12 BufferReady, 960 000 bajtů, 500,0 Hz, RMS 0,354 = RMS souboru (PASS; audio_check 9/9). Fyzický mikrofon a poslech jsou v `docs/HARDWARE_CHECKLIST.md`. |
| M05 | IN_PROGRESS | Scénář `video` (PASS): Play → Playing s rostoucími snímky, obraz sleduje hodiny zvuku (`GetFramePresentationTimeEXT` proti `PlayPosition`, mezera 0,09–0,13 s, mez 0,25 s = jeden softwarově kreslený snímek + dva snímky videa), Pause drží pozici, Resume, Stop → 0,0, nesmyčkové video skončí samo. Seek nemá XNA ani CNA (shodné). Zvuk videa: s `SoundEffect.MasterVolume` 0 je video tiché, ačkoli jeho Volume je 1 – zapsáno jako rozdíl k potvrzení (XNA spravovaný kód Volume předá nativnímu dekodéru a MasterVolume nečte). Automatické měření soundtracku v PCM odloženo: místnost au-video slyší vlastní zvuky sousedních audio místností (smyčka arpeggia v au-layers), takže porovnání energie nebylo spolehlivé. Zbývá varianta MUSEUM_VIDEO=OFF (P10). |
| M06 | DONE | ct-storage tlačítko „Error paths and async results, checked against XNA 4.0“ (22 kontrol, očekávání z dekompilátu XNA, vlastní kontejner smazán po běhu): async callback/AsyncState, cesty mimo kontejner, FileNotFoundException, DeleteFile chybějícího, Dispose/Disposing souhlasí; původně 13 rozdílů (end-twice, sizeInBytes, cizí zařízení, ArgumentNullException, IOException, chybějící ObjectDisposedException); tři kontroly po Dispose CNA opravilo v `5865ae938` (teď PASS), zbývá 10 v `docs/CNA_FINDINGS.md`. Odpojení simulováno jasně označenou cestou (XDG_DATA_HOME pod obyčejným souborem): `IsConnected false`, `StorageDeviceNotConnectedException` z EndOpenContainer; odhalilo, že `SetAppNameEXT` hází hned (muzeum při startu padalo → ošetřeno, nález). `DeviceChanged` CNA nikdy nevyvolá (jako FNA na PC), čítač ukazuje pravdivě 0. Izolace rootu: všechny automatické běhy mají `XDG_DATA_HOME` v build stromu (`MUSEUM_DATA_HOME`), nic do `~/.local/share`. Scénáře `storage_contracts`, `storage_not_connected` (PASS; `scenarios.py` umí `env`/`touch`). |

## N — networking, hráči a telefonní notifikace (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| N01 | DONE | dev-notify (NotificationLab): kanál na loopbacku, ChannelUriUpdated, Find, raw i toast SendAsync, DispatchPendingNotificationsEXT ve vlákně hry, BindToShellToast (jen záznam, shell neexistuje – přiznáno), Close a pozdní odeslání, odeslání na port bez posluchače. Ověřeno tourem i `devices_check.sh`: raw tělo doručeno přesně (13 B), toast jako data, zavřený kanál odmítá, Find vrací null, port 9 odmítnut. |
| N02 | IN_PROGRESS | Opakované otevření a cleanup při odchodu implementovány; zjištěno: zprávy zařazené před Close se při dalším dispatch ještě doručí (2), zdokumentováno jako chování. Zbývá vláknová kontrola (TSan, P11). |
| N03 | BLOCKED_EXTERNAL | `tools/net_check.sh` (tři procesy, dvě kola): s jedním lokálním hráčem na hostiteli po jeho odchodu proběhne HostChanged a B session drží (PASS); se dvěma lokálními hráči na hostiteli session skončí `HostEndedSession`, ačkoli klienti vidí AllowHostMigration true. Příčina ověřena v CNA: `AttemptHostMigration` vybírá nejnižší wire id i mezi zbylými hráči odcházejícího stroje (zapsáno v `docs/CNA_FINDINGS.md` s potřebnou změnou). Hotovo po opravě v CNA (vlastník CNA), v muzeu nic dalšího. Blokér: vada `AttemptHostMigration` je v CNA, které vlastní jiný agent; muzeum ji reprodukuje (`tools/net_check.sh` kolo 2) a nález s potřebnou změnou je v `docs/CNA_FINDINGS.md`. |
| N04 | IN_PROGRESS | Session část přes SystemLink (`tools/net_check.sh`, kolo lifecycle, kroky řízené novým příkazem skriptu `await:TEXT[,N]` místo pevných čekání): hostitel s AllowJoinInProgress spustí hru, B se připojí (FAIL: B zůstane ve stavu Lobby a nedostane GameStarted – `ServerWelcomeMessage` stav session nenese, nález v `docs/CNA_FINDINGS.md`), hostitel odebere B (`NetworkMachine.RemoveFromSession`) → B `SessionEnded: RemovedByHost`, hostitel GamerLeft, B se znovu připojí a hostitel vidí druhé GamerJoined (4 PASS). Zbývá party a pozvánky s ≥3 účty přes stránky Guide (vyžaduje automatizaci Guide pro více účtů v online stacku) a privátní sloty. |
| N05 | DONE | CNA hlas je skutečně implementován (VoiceChat: mikrofon jednoho lokálního hráče, detekce řeči, Opus 16 kHz po 20 ms, přehrání přes SoundEffect; `CNA_VOICE_OPUS` v tomto buildu). `tools/voice_check.sh`: dva procesy bez fyzického audia, SDL „disk“ ovladač čte mikrofon mluvčího ze souboru (300 Hz tón, mono S16 44,1 kHz dle logu SDL) a zapisuje výstup posluchače. Ověřeno: posluchač se připojí, NetLab zaloguje „voice: Visitor#0 started talking (HasVoice true)“ a výstup posluchače má dominantu 300,0 Hz (RMS 0,354). Ztlumení (`IsMutedByLocalUser`) je v XNA jen přes Guide/gamer card; veřejný setter neexistuje, proto není skriptováno. |
| N06 | DONE | net-session: tlačítko „Burst“ pošle po připojení vzdáleného muzea 100 číslovaných paketů s každou hodnotou SendDataOptions; přijímající muzeum je spočítá (přijato, mimo pořadí, duplicity) a zaloguje. `tools/net_check.sh` (kolo packets): odesílatel má SimulatedPacketLoss 0,3 a SimulatedLatency 100 ms (simulace působí na odesílané pakety – jako `NetworkSimulator.SendData` v XNA; popisek a text místnosti, které tvrdily „na přicházející“, opraveny): Reliable 100/100 bez duplicit, ReliableInOrder 100/100 v pořadí, InOrder 69/100 nikdy mimo pořadí, None 72/100 (68 mimo pořadí díky rozptylu latence), GameStarted/GameEnded na obou stranách, GamerJoined (remote) na hostiteli – 8/8 PASS. Bez simulace ztrácí transport u dávky 100 InOrder paketů v jednom snímku 79 % (nespolehlivé sekvencované pakety, kontrakt XNA to dovoluje; zaznamenáno jako chování). Claimy GameEnded, GamerLeft, GamerJoinedEventArgs(.Gamer). |
| N07 | DONE | `tools/online_check.sh` + `tools/online_stack.sh`: vlastní stack v `build/online-check` na vlastním portu 47832 (interaktivní stack v `build/online` se nikdy nesmaže ani nezastaví), databáze od nuly se třemi účty (visitor, curator, archivist), úspěchy, žebříček se dvěma nasazenými hodnoceními, přátelství a zpráva; profily, pověření a uložená data v build stromu; trap zastaví službu a muzeum i při chybě, `timeout -k` na muzeu; exit 77 jen při chybějícím nástroji nebo nepostavené službě, postavená služba, která nenaběhne, je FAIL s důvodem. |
| N08 | IN_PROGRESS | Skutečný serverový round trip (`tools/online_check.sh`, PASS): přihlášení účtu Visitor přes Guide (xdotool na privátním displeji), zápis hodnocení 10 a úspěchu do služby (ověřeno v její databázi), zpětné čtení žebříčku z vrcholu `1. Curator 120; 2. Archivist 80; 3. Visitor 10 (total 3)` a seznamu přátel `Curator`. Odmítnutí bez služby/offline (Guide.ShowFriends, ShowMarketplace, IsFriend) v pl-exceptions. Zbývá odhlášení (SignedOut) a party/pozvánky s více účty (N04). |

## O — diagnostika, Inspector a nativní C API (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| O01 | DONE | Scénář `tests/scenarios/inspector.json` (T05 runner) na OPENGLES3: Agent::Start na portu z OS, Connect se špatným tokenem odmítnut, se správným „negotiated“, Ping → pong, CaptureSnapshot s metrikami, ReadEvents (výsledek dle runtime úrovně), RequestPreview cizího zdroje → Unavailable, Agent::Stop a Ping po zastavení → „not connected“; bez exhibit/script chyb. Teardown v destruktoru i OnLeave. |
| O02 | DONE | Muzeum implementuje veřejný `CNA::Inspector::IResourcePreviewProvider` pro vlastní 64×64 texturu místnosti registrovanou `CNA::Diagnostics::ResourceHandle`: požadavek z vlákna agenta se jen zařadí (bez dotyku zařízení), místnost v Update kóduje PNG `Texture2D.SaveAsPng` ve vlákně hry, další PollPreview vrátí Ready. Scénář ověřil „RequestPreview (own texture): Pending, ticket 1“ a „PollPreview: Ready 64x64 image/png, N bytes, PNG signature ok“. Externí cna-inspector most a browser UI tu nejsou (separátní repozitář) – zbývá jako P-ověření, ne implementace. |
| O03 | IN_PROGRESS | `CNA_DIAGNOSTICS` už není vynucen přes FORCE (výchozí FULL). pl-diagnostics: ohraničený záznam (16 z 200 markerů, 184 zahozených), ResourceHandle.Reset (scénář diagnostics). Nově tlačítko „Instrumentation macros“: `CNA_DIAGNOSTICS_COUNTER_ADD` (+3), `GAUGE_SET 40` + `GAUGE_ADD 2` = 42, `FRAME_COUNTER_ADD` registrován, `CNA_PROFILE_SCOPE` jako zóna a `CNA_DIAGNOSTICS_EVENT` jako marker v trace (runtime Full) – vše PASS na úrovni 2 (FULL); očekávání se řídí `CNA_DIAGNOSTICS_LEVEL`, takže stejné kontroly při OFF vyžadují nepřítomnost. Zbývá spustit je v buildech OFF a STATS (P10, build-probe po dokončení ostatních buildů). |
| O04 | DONE | Volba `MUSEUM_C_API` (výchozí ON, zapíná `CNA_BUILD_C_API`) překládá skutečnou C17 TU `src/CApi/NativeCProbe.c` linkovanou proti `cna_c_api`; C++ místnost cx-capi ji jen volá přes `museum_c_api_run` a zobrazí/zaloguje výsledky. Mapa veřejného C ABI: verze `cna_get_abi_version`, výsledkové kódy, thread-local diagnostika (`cna_error_get_last_info`, kopie zprávy bez terminátoru – kontrakt dodržen), 64bit handly se stale kontrolou. Hra v C má vlastní runtime (`cna_game_create`), což v jednoprocesovém muzeu nedává smysl – evidováno. |
| O05 | DONE | Všech 59 C hlaviček s deklaracemi je claimováno a ověřeno přes C ABI: 880 claimů cx-capi, každý se zrevidovaným záznamem. C klient v muzeu (`src/CApi/NativeCProbe.c` + `CProbe*.c`): 65 kontrol, 60 PASS, 5 FAIL (hodnoty, cnb, content readers, vstupní pomocníci, senzory, devices, gamer services, net). Samostatný C17 program `cna-c-probe` (vlastní hra přes `cna_game_create`, jedna hra na proces) spouštěný tlačítkem cx-capi jako podproces s izolovaným HOME (`build/c-probe-home`, kopie fixture knihovny médií), timeout 120 s, kill podle PID: 63 kontrol, 61 PASS, 2 FAIL (runtime/okno/grafický manažer, grafika s readbackem render targetu pixel po pixelu, efekty, modely včetně XNA shape.xnb, sprite font, content, audio/XACT/media/video, knihovna médií přesně podle fixture, vstup, storage, gamer services offline, lokální síťová session). 7 rozdílů v `docs/CNA_FINDINGS.md` („Native C API“), mj. dvě kopie stavu CNA v procesu (`libcna_c_api.so` má statickou kopii CNA). Scénář `capi_native` (130 přesných řádků) PASS. |
| O06 | DONE | Logging: pl-log tlačítko „Contracts“ (scénář `logger_contracts`, 5 PASS): SetMinimumLevel(WARN) zahodí Info a doručí Warn, text sinku `[WARN][AUDIO] message`, WarnIf(false)/InfoIf pod úrovní nic a ErrorIf(true) jeden řádek, nahrazený sink už není volán, po ResetSink není volán žádný programový sink; vlastní sink a úroveň místnosti se obnoví. `IsEnabled`/`ToString` jsou v CNA private (nejsou veřejné API). Výjimky: pl-exceptions vyvolá 11 skutečných dosažitelných chyb (disposed textura, 0×0 textura, SpriteBatch End/Begin, chybějící obsah a titulní soubor, únik ze save složky, Guide bez služby/offline, dvojí Guide) s typem a zprávou; storage chyby v ct-storage (M06). Životnost observerů: diagnostický zdroj a ResourceHandle (Reregister), odběry událostí se odhlašují při odchodu (A05, clipboard/sensors restore). Nebezpečná hardwarová selhání se neprovokují. |

## P — rendererové, platformní a hardwarové ověření (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| P01 | DONE | Baseline 2026-10-20 (museum `9b3a4ec`, CNA `41305f5ba`, `tools/check_all.sh`, OPENGLES3 llvmpipe na privátním Xvfb): build, museum_tests 9 513 kontrol 0 selhání, --validate, generované dokumenty aktuální, press-all tour 316 navštívených buněk (315 místností) 0 exhibit/0 script chyb za 172 s, výtah/schody 20/20, renderer self-checks 100 výsledků (97/0/2/1), 36 referenčních pohledů do 1 %, paměť přes tři tour 646 → 747 MB (růst 15 %, 0 chyb), scénáře 7/7. Proti výchozí revizi `d34c4a9`: 295 → 315 místností, 1 292 → 1 771 API jmen, renderer kontroly 87 → 100. Inventář/audit selhaly jen proto, že CNA mezitím změnilo revizi (+0/−0), regenerováno. ONLINE=1 a SLOW=1 profily jsou samostatné (P03, N07). |
| P02 | DONE | OPENGLES3 a OPENGL33: software GL (llvmpipe/Xvfb) i fyzická GPU (radeonsi, `MUSEUM_GPU=1`) – renderer_check 103/0/2/1 v `build/` (OPENGL33 i po stencil/PBR), tour bez chyb, reference do 1 %; s `CNA_EASYGL_COMPILED_EFFECTS` (build-probe) OPENGLES3 104/0/2/0 včetně kompilovaného instancingu a fx-compiled 4/4 PASS. |
| P03 | TODO | SOFTWARE: dostupné schopnosti a explicitní skips, compiled-effect volba, numerické/pixelové kontroly bez předpokladu shader source podpory. |
| P04 | DONE | build-probe (všechny renderery, compiled effects zapnuté, lavapipe/llvmpipe): renderer_check FNA3D 81/0/2/23 (formáty, které FNA3D neumí), WEBGPU 82/1/2/21 (FAIL = PbrEffect bez osvětlení, nález), SDL_GPU 101/0/3/0 (INFO = PbrEffect odmítnut pro VertexPositionNormalTexture), OPENGLES3 104/0/2/0. Press-all tour 320 místností: FNA3D 267 s, WEBGPU 282 s, SDL_GPU 580 s, všechny 0 exhibit/0 script chyb (SDL_GPU původně padal v tex-volshader – vada CNA, místnost tam volume shader nespouští). Na fyzické GPU: SDL_GPU (RADV) 97/0/2/1, FNA3D (radeonsi) 75/0/2/23, WEBGPU (RADV přes privátní weston) 76/0/2/22 – před přidáním stencil/PBR kontrol. Shaderové dialekty: WGSL a GLSL ES verze místností (volume, MRT) podle rendereru. |
| P05 | TODO | HEADLESS/STUB: správné refusal/skip a úspěšné datové operace; STUB odmítnutí startu nesmí být úspěšný grafický test. |
| P06 | TODO | WEBGL2: kompletní tour, všechny validační místnosti, dostupné nové scénáře, persistence a cleanup. Dosavadní smoke není full coverage. |
| P07 | TODO | Browser WEBGPU: zjistit dostupnost toolchainu a podpory; sestavit/spustit odpovídající profil, bez nahrazení WEBGL2 výsledku tvrzením o WebGPU. |
| P08 | TODO | Windows DIRECTX9/SOFTWARE a další dostupné renderery: Wine výsledek označit Wine, skutečná Windows ověření zvlášť. Nepřepsat aktivní web build v build-consumer bez kontroly. |
| P09 | TODO | macOS/Metal přes podporovaný renderer, Android/iOS a senzory: připravit portabilní build/scénáře a doložit, co zde skutečně lze ověřit. Nepřidávat zpět vyřazené CNA renderer identity. |
| P10 | TODO | Feature-off kombinace: networking/Inspector/pipeline/video/CNAEXT/devices/diagnostics, jejich link closure a truthful notices. Zakázaná feature není pokrytá functional testem default buildu. |
| P11 | TODO | ASan/UBSan po relevantních změnách lifetime; TSan pro notifikace, audio, síť, Inspector. Cílené mutace nejdůležitějších oracle kontrol, ne bezdůvodné opakování všech drahých sestavení. |
| P12 | DONE | Fyzická GPU ověřena bez displeje: `MUSEUM_GPU=1` (SDL offscreen, EGL přes render node, Vulkan přes VK_EXT_headless_surface; nic nejde na živou plochu). AMD Phoenix 1002:15bf, Mesa radeonsi (doloženo EGL logem v `build/renderer_check/<R>-gpu.device`): renderer_check OPENGLES3 i OPENGL33 97 PASS/0 FAIL/2 INFO/1 SKIP, press-all tour 316 místností 0 chyb za 36 s, všechny referenční pohledy do 1 % od llvmpipe. Audio PCM (`audio_check.sh`, disk driver) a hlas přes síť (`voice_check.sh`) ověřeny bez zařízení. Zbytek (reproduktory poslechem, mikrofon, gamepad/gyro, haptika, joystick, dotyk, kamera, tray, nativní dialogy, senzory telefonu, hotplug, ztráta zařízení, prezentace na skutečnou obrazovku, Windows/macOS) je v `docs/HARDWARE_CHECKLIST.md` s kroky, očekáváním, důkazem a přesným blokérem. |

## D — dokumentace, UX exponátů a závěrečné ověření (P1/P2)

| ID | Stav | Úkol a akceptační podmínka |
|---|---|---|
| D01 | TODO | CNA_COVERAGE/README/HANDOFF/RENDERER_TESTING: opravit neplatné tvrzení FNA3D-only compiled effects, staré počty a už implementované online služby. Historické výsledky nepřepisovat jako nové. |
| D02 | DONE | `docs/CNA_API_NOTES.md` začíná výrazným upozorněním, že jde o historický podklad z 2026-10-13: jmenuje vyřazené renderery (VULKAN, SDL_RENDERER, DIRECTX11, METAL), odkazuje na živý seznam v `GraphicsRendererType.hpp`, na přesun compiled effects za volby `CNA_<R>_COMPILED_EFFECTS` a na auditovaný inventář/evidenci jako zdroj pravdy. |
| D03 | TODO | Každý nový exponát: katalog, API evidence, registry, ovládání, vysvětlení očekávaného výsledku, absence závislosti, cleanup; žádné prázdné místnosti s pouze marketingovým názvem. |
| D04 | TODO | Po přidání místností regenerovat mapy/counts/coverage, zkontrolovat otočené místnosti, přístupnost dveří/stairs/elevators a referenční kompozice. Nové screenshoty pořizovat z reálného běhu. |
| D05 | DONE | `docs/VERIFICATION.md`: automatický a manuální profil odděleně; tabulka předpokladů (CMake/Ninja/C++23/ccache/SDL3, ../cna, ~/deps FNA3D a wgpu-native, Xvfb, xdotool, Pillow, numpy, sqlite3, ffmpeg, Chrome, Mesa radeonsi/RADV, ~/deps/weston); pro každý automatický profil příkaz, očekávaný výsledek a kde je důkaz (včetně GPU režimů, execution coverage a API auditu); manuální profil = `docs/HARDWARE_CHECKLIST.md`. Odkaz z README. |
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

- 2026-10-20: Fyzická GPU bez displeje (P12): SDL `offscreen` driver renderuje na skutečné GPU bez
  X/Wayland serveru, takže GPU testy nejsou blokované chybějícím DRI3 v Xvfb. `MUSEUM_GPU=1` v
  `_headless.sh`; renderer_check ukládá identitu ovladače z EGL/Vulkan loaderu (CNA `GraphicsAdapter`
  ji neumí říct – hlásí PCI id stroje i při llvmpipe, zapsáno v nálezech jako chování shodné s FNA).
  Muzeum loguje adaptér při startu. Plná sada (tour, scénáře 7/7, renderer, reference, nav, devices
  27/0/4 unavailable, audio 6/6, voice 5/5, net 8/2 – 2 = vada migrace v CNA) prošla na CNA `b2c7288ca`.

## Obnovení práce

Přečti aktuální stav nahoře, `git status`, příslušné instrukce a log poslední rozpracované
položky. Ověř běžící procesy dřív, než začneš nový build nebo použiješ sdílený build tree.
Vyber nejbližší odblokovaný úkol. Napiš důkaz do plánu, když ho dokončíš, a přejdi dál.
Pokud v tomto vlákně goal zůstává aktivní, není potřeba od uživatele nové zadání.
