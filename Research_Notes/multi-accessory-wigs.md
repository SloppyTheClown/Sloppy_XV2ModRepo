# Multi accessory: stacking a second part per slot (wigs, helmets)

Goal: an extra accessory that replaces a part slot (2nd wig / helmet)
draws *as well as* the equipped one, skinned to the body like a normal
part. Static RE of 1.26.01 (2026-10-26) unless marked. Context:
`multi-accessory.md` (dead ends: skip / made-up physics object),
`multi-accessory-phase0.md` (builder, CharaResourceBase, physics cap).

## Findings
* **Chara part slots** (`Chara@Common`, vtable 0xf9bb00): one part object
  per slot at **Chara +0x370 + slot*8** (10 slots, 0..9); the part's own
  physics objects at +0x3c0 + (slot*3 + i)*8 (3 each, 30) then the 4
  accessory physics slots +0x4b0. Skeleton at **Chara +0x38**.
* **Attach** `vt+0x368` = 0x27e9e0(chara, slot, part): `UnregisterModel
  (slot)`, frees the slot's 3 physics objects (0x28d8e0), releases the old
  part (vt+0x20), stores + addrefs (vt+0x18) the new one, 0x289340 (part
  physics -> +0x3c0 slots), re-links accessory physics part indices
  (vt+0x398), then **0x288930** (register pass) and more refreshes
  (0x289a20, 0x28aa90, 0x2894a0, vt+0x410/0x418, 0x289930, 0x288710),
  slot 0/1 extra animation set-up (+0xa8 / +0xb0), vt+0x2d8.
* **Register pass 0x288930** (callers 0x27eb0e, 0x284bf8): for slot 0..9
  with a part: Model = 0x29aee0(part); the slot is *unregistered* when any
  other slot's part has bit `slot` in its hide mask 0x29e000(part), or any
  of the 34 physics objects (+0x3c0.., active when obj +0x12c != 0) has it
  in 0x29e000(obj +8); else `Skeleton::RegisterModel(model, slot, 0)`.
  That mask is how hats hide hair and why FRI helmet + hat 835 lost the
  head (835's object hides slots).
* **xgcore `Skeleton`** (xgcore.dll RVAs: RegisterModel 0xaf060,
  UnregisterModel 0xaf1a0, GetModel 0xaf010, HasModel 0xaef80): models are
  a **std::map<uint index, {Model*, int}> at skeleton +8, no capacity**.
  RegisterModel(model, index, flag): refuses a model already in the map
  (any index), unregisters the old one at `index`, inserts, binds the
  model to the skeleton's nodes (0xaff60), refresh 0xaf490. So a model
  at index >= 10 is drawn + skinned like a part (assumed: drawing walks
  the map - untested) and the game's register pass never touches it.

* **Who attaches (2026-10-26):** only 0x27e9e0 writes Chara +0x370, and
  vt+0x368 is called only by the builder (apply 0x65e290: 0x65a69d
  restore-base for the *old* buffer's holders, 0x65a77e current holders;
  0x65a8b0: base parts from builder +0x160), a battle mob special
  (0xca709, slot 4 null) and quest cast 0x7c37bb. **The CharaResourceBase
  path (lobby, save select, battle) never attaches parts to the Chara.**
* **Builder apply 0x65e290** (builder rdi, buffer `[rdi+0xb0]`, 0x18
  stride): other buffer's holders (+0x80/+0x98): per slot the holder fills,
  re-attach the base part (0x65aed0 / 0x65ae20(+0x160, slot, +0x2f4[slot])),
  then 0x29d790(holder), dtor 0x29ce90, HeapFree, vector cleared. Then the
  current buffer: for each holder, slot 0..9: `part = 0x29d2a0(holder,
  slot)`, if set `vt+0x368(slot, part)` (later holder wins). Then physics
  i: vt+0x390(i, obj), vt+0x398(i, +0x50[i]).
* **Resource path draws through its own Skeleton:** 0x2999b0(res)
  (states 5/6 of the state machine 0x297ac0, after ready check 0x2997a0)
  does `CreateSkeleton` into res +0x80+idx*8 (idx = res +0x118; <0 ->
  +0x78), finds the holder for part set `res+0xd4` in the std::map res
  +0x30 (0x148e10; node +0x18 = holder) and **0x29cfc0(holder, skel)** =
  `RegisterModel(skel, 0x29aee0(holder[slot]), slot, 0)` for slot 0..9.
  When res +0xfc: a second skeleton at +0xa8+idx*8 (+0xa0) with the holder
  at res +0x70. Unload (0x297ac0 ~0x297f1e): 0x29d850(holder, idx) per map
  entry, then `~Skeleton` + HeapFree of +0x80[idx] and +0x78.
* **Holder build** in 0x299ac0 (= vtable slot 5): new 0x60 + ctor
  0x29ce40; 10x `0x29a700(res, &entry[slot], slot, res+0xd8, res+0xdc)`
  (16-byte entries: +0 part, +8 flags, +0xc arg5); then per non-null
  entry `0x29d620(holder, slot, part, flags, arg5)` (new + 0x29b960 ctor +
  0x29c070 load). Holder = 10 part pointers (0x29d2a0(holder, slot) reads
  them) + std::list +0x50 of physics objects; no spare slot.
* **Materials:** Chara vt[47] 0x282760(chara, arg) walks every part set
  holder of the resource map (0x298090(res, i)) and per slot calls
  **0x29e020(part, model, arg)** = DynToon material block set-up
  (`DynToonMaterialBlockMapper::Build`, material callbacks). A model we
  register ourselves misses this unless its part is in a mapped holder or
  we call 0x29e020 with vt47's arg (capture by patching slot 47 at
  0xf8dc18).
* **xgcore Skeleton lifetime:** `~Skeleton` (0xae9c0) destroys a map
  entry's Model only when it was registered with flag (3rd arg) != 0; the
  game passes 0, so a skeleton dying with our model in it is safe. The
  crash risk is the other order: the part/holder freed while its model is
  still registered and drawn.
* **Index loops:** `renderPost` (0xaf5a0) walks the whole map (in order),
  but only runs when a post-render listener (xgcore global 0x5c2d38) is
  set; the main draw path is not confirmed. Several game loops do `for i
  < GetModelNum(): GetModel(i)` **by key** (Chara vt47 physics models,
  0x321ae0 = DuplicateModel clone of a skeleton, MapShadow vt[2]
  0x33f050): a key past a gap is never seen. So stack at **key 10 (+n)**,
  contiguous; on the builder path the register pass unregisters hidden
  slots (gaps), so shadows / clones may miss it there.

## Plan (untested)
1. Load the extra's replacer part as a part object (equip-screen builder
   already does: retest 5 "replaces the equipped wig" = its holder went
   through vt+0x368 for the same slot, last wins).
2. Don't attach it to the slot: keep it ourselves (addref vt+0x18) and
   `RegisterModel(skel, 0x29aee0(part), 10 + extra*10 + slot, 0)` after
   the game's attach / register pass (hook the 0x288930 calls).
3. Unregister + release before the holder buffer is freed (apply 0x65e290
   frees the other buffer) and on Chara teardown - a freed Model left in
   the map would crash.
4. Same on the CharaResourceBase path (lobby, save select, battle):
   entries per slot from 0x29a700, loaded by 0x29d620 / 0x29d6d0 - find
   where they reach vt+0x368.
5. Open: the part's own physics (3 slots per part slot - an extra wig's
   physics would have nowhere to go; skip them), its hide mask (ignore),
   dyt/colour (part colours come from the CaC's colour slots - likely
   fine), transformations (+0xf8 override part set).

## Dead ends (in game)
* A part's attached physics added with part index -1 through
  AddPhysicsObject on the CharaResourceBase path ("loose", FRI helmet
  2035 + hat 835): save-select previews stop loading, soft lock (retests
  7 + 8). The part set's *own* physics list at -1 is fine (the game does
  that).
* A wig's part model loaded as a made-up physics object (file
  `<race>_<model>_<slot>`, bone b_C_Head): the character's load hangs
  (retest 6) - part models have no own .esk.

## Design from the 2026-10-26 RE (replaces Plan 2-4 where they differ)
* **Resource path first** (lobby, save select, battle; where the extras
  matter): our own holder per (resource, stacked extra): new 0x60 + ctor
  0x29ce40, `0x29d620(ours, slot, part, flags, arg5)` with the extra's
  clashing replacer part (we already wrap 0x29a700 and 0x299c46 there).
  Hold the state machine until it is loaded (hook the ready check
  0x2997a0). Hook the 0x29cfc0 call at 0x299a4e: after it,
  `RegisterModel(skel, 0x29aee0(ourPart), 10 + n, 0)`; materials via
  0x29e020. Before our holder is freed (resource unload 0x29d850 pass /
  resource dtor 0x2993d0): `UnregisterModel(skel, 10 + n)` on every
  skeleton we registered in, then dtor 0x29ce90 + HeapFree.
* Cheaper if callable: insert our holder into the resource map res +0x30
  under a made-up part set key - the game then loads, ready-checks,
  material-maps (vt47) and unloads it with the others; only the
  RegisterModel at 10+n stays ours.
* **Builder path** (equip screen preview): hook 0x65a77e: when the slot
  already got a part from an earlier holder in this apply, register at
  10+n instead of attaching; unregister at the start of every apply
  (before 0x65a6e5 frees the old holders) and before the builder dies.

* **The map route exists in the game (static, 2026-10-26):** res +0x30 is
  an **unordered_map<int part set, holder*>** (FNV-1a over the 4 key
  bytes, list head +0x38, buckets +0x48, mask +0x60); insert =
  `0x1a60b0(&map, &outIter, &{int key; holder*}, hint byte)`. 0x299ac0
  inserts the main holder under key res +0xd4 (0x299d12), then for every
  part set id in the **vector<int> res +0xe0..+0xe8** not yet in the map:
  new holder (0x29ce40) + **0x29d290(holder, BCS part set, res+0xd8,
  res+0xdc)** (loads that whole part set) + insert (0x299e71) - a preload
  list, probably transformation forms. So: push the stacked extra's part
  set id into res +0xe0 before 0x299ac0 runs and the game builds, loads,
  ready-checks, material-maps and unloads its holder; after 0x29cfc0 at
  0x299a4e we look it up (0x148e10(&map, &out, &key), node +0x18) and
  `RegisterModel(skel, 0x29aee0(holder[slot]), 10 + n, 0)`. Unknown: who
  fills/reads res +0xe0 (a form switch may treat our id as a form), and
  whether unload frees holders before or after `~Skeleton` (unregister
  ours first either way).

* **res +0xe0 users** (CharaResourceBase range 0x2900c0-0x2a0000): filled
  only by init vt[0] 0x298300 (copies a creation descriptor's vector, or
  push_back of its single id: `if end != cap: *end = id, end += 4; else
  0x4bce0(&vec, end, &id)`), read only by 0x299ac0's preload loop
  (0x299dd7), freed by the dtors (vt[6] 0x2977c0 / Lobby 0x416880 ->
  0x577f0). Appending in our existing 0x299c46 hook works (same call,
  before the preload loop). Readers outside that range (battle form
  switch?) not searched.

* **Implemented (test build, 2026-10-26, untested):** as the map route
  above; `ResSkeletonHook` wraps both 0x2999b0 calls (0x297e91,
  0x297f01) and registers into the main skeleton only (not the +0xa8 one
  for holder res +0x70). Unload (0x297f1e): 0x29d850(holder, old idx) per
  map holder = part vt+0x10(idx) frees that index's model, *then*
  ~Skeleton - the game's own slot models are freed while still registered
  too, so a map holder's model at key 10 needs no unregister. Part model =
  part +0x1c8 + part[+0xe8]*8 (double-buffered per skeleton index; 0x297ea3
  calls 0x29d0c0 on every map holder before the ready check). Stale: an
  id stays in res +0xe0 when the pick changes (loaded, not drawn).

* **Retest 10 (2026-10-26): stacking works** (3 wigs: 1410 equipped, 612
  + 1406 at keys 10/11; skinned, right place, save select/lobby/battle, no
  hangs) but the stacked wigs' colours were the equipped wig's and changed
  with view angle. Cause (static): each part has its own DynToon ramp unit
  (`DynToonRUController` at part +0xd0, dyt container +0xc0) that is
  prepared only by **ModelPartResource vt+0x18 = activate 0x29bd80**
  (Detach + `PrepareToonRenderingUnit` + SetMainCluster, then its
  attached objects +0x1f0). The game activates only the active part set:
  0x29d020(src holder, dst holder) from Chara 0x27e7e6 (via 0x297910 map
  lookup) does dst part vt+0x20 (deactivate 0x29c020 -> DetachTRU) + src
  part vt+0x18 per slot. A preloaded map holder is never activated, so the
  material callback (`DynToonTextureSettingMCbFunctor` 0x29dc10: skips
  SetTexture when !IsPrepared) leaves the previously bound ramp - same
  shader, drawn right after the equipped wig. (Notes calling vt+0x18 in
  attach 0x27e9e0 "addref" were wrong: it is activate.) Part dtor base
  0x29e420 DetachTRUs unconditionally. **Fix built + installed
  (untested):** `ResSkeletonHook` calls the stacked part's vt+0x18 after
  a successful RegisterModel.
* Chara vt47 0x282760(chara, arg): loops part set ids 0 .. BCS count,
  holder = map lookup by that id (0x298090), 0x29e020(part, model, arg)
  per slot = DynToon material block mapping + registers a
  `DynToonTextureSettingMCbFunctor` with +8 = arg (another RU controller);
  covers preloaded holders too.

* **Activate 0x29bd80 (static):** if part +0xc0 (EmTextureContainer, the
  ramp emb) is set: DetachTRU if prepared, then
  `PrepareToonRenderingUnit(renderer, part +0xd0, +0xc0, +0xec)`, which
  fills the controller's unit pointer (part +0xe0), `SetMainCluster(+0x104)`,
  then unit vt+0x28(part +0x1e8). Loader 0x29c070 sets +0xe8 =
  0x29ef60(flags), +0xec = 0x29ef90(...) & 1, +0x104 = BCS part word +4,
  +0x1e8 = word +0x10.
* **Retest 12 (2026-10-26):** the vt47 calls of 0x29e020 never ran
  (save select, lobby, character select, battle) - vt47 is not how
  resource-path parts get their materials. After our activation +e0 is a
  pooled unit (same address per extra across builds; battle: +ec 1, other
  unit type), but stayed null in builds right after a battle (several
  characters at once). Equipped wig's fields not dumped yet (wrong holder
  looked up; retest 13 uses the main holder, key res +0xd4).
* **Retest 13 (2026-10-26):** at 0x2999b0 time the equipped wig (main
  holder slot 5) is not activated yet: +e0 null, +fc -1; +e8/+ec/+104/set
  match ours, +c0 is its own emb; our activation leaves it alone. The game
  activates later: Chara SetPartSet 0x27e7a0(chara, set) stores the set at
  chara +0x5a4, then 0x297910(res = chara +0x30, set, chara +0x370; only
  caller 0x27e7e6) finds the holder and calls 0x29d020(holder, chara
  parts): per slot 0..9 deactivate old (vt+0x20), activate new (vt+0x18),
  store; then activates every part in the holder's +0x50 list (node
  +0x10). So our early activation ran before the holders were ready.
  Fix (retest 14): activate stacked parts after 0x297910 - confirmed,
  colours mostly right; skin parts still green, some black/white angles.
* **CaC colour pass (static, 2026-10-26):** Chara vt[57] 0x27d890(chara,
  const char *name part, float4 *colour, int) is the only writer of the
  CaC colours into ramp units: for each of the 10 parts at chara +0x370
  (null skipped) it takes controller = part vt+0x30 (part +0xd0), unit =
  controller +0x10, model = 0x29aee0(part), builds a
  DynToonMaterialBlockMapper over the model's materials, and for every
  material whose name contains `name` (0x288450) calls unit vt+0x58 with
  its block index and writes the colour as 0..255 bytes; then the 30
  physics objects at +0x3c0 (those with +0x12c != 0) through 0x28c5b0.
  Callers: Chara vt[130-132] (0x2847a0 / 0x284960 / 0x284d40) loop the
  BCS PartColors (bcs +0x1c table, count +0xe; name +0, 0x50-byte colours
  at +0xc) and call [vt+0x1c8]; also 0x475f70 and the equip screen
  builder (0x6573d0, 0x657678, 0x6576f0). Only pointer to 0x27d890:
  vtable 0xf9bb00 slot 57 (`ptr.py`). Stacked parts aren't in +0x370, so
  their SKIN_ blocks keep the .dyt placeholder (green). Fix (retest 15,
  untested): hook slot 57, rerun the pass with only the stacked parts in
  the array (+0x3c0 emptied), restore. Dead end: CPU colour write at load
  (res +0xd8/+0xdc, Texture2D::Map - those are movie/QR menu code;
  CreateDynTRUCFromEmb users are lobby code 0x4b2xxx).

* **Retest 15 (2026-10-26):** skin parts right everywhere; black (menus)
  / white (battle) at some angles remained.
* **Per-part draw callbacks (static, 2026-10-26):** load 0x29e630 gives
  each part three material callbacks in its MaterialCallbackCaller at part
  +0x138 (vector of {functor*, int owned} at +0x140..+0x148; the functors
  are owned by the list at +0x1b8, freed by dtor 0x29e420): [0]
  `DynToonTextureSettingMCbFunctor` (+8 = part's RU controller; binds the
  ramp + SetShaderParameter(block index)), [1]
  `DynToonLightApplyingMCbFunctor<14>` (vt 0xf902f0; +8 light source,
  +0x10 light direction, load-time default 0xf5a9e0), [2]
  `SceneMaterialParameterApplyingMCbFunctor` (vt 0xf902b8, call 0x29dc40:
  `Scene::ApplyCharaSubBufferDrawParam(+0xc, ctx)` unless +0xc < 0; init
  -1). Getter 0x29dfb0(part, kind): kind 0 -> [1], kind 1 -> [2]. Writers,
  all looping only chara +0x370 (and physics +0x3c0): 0x289850 (chara
  update, from 0x27f876 / 0x28017b) sets [1] from
  `SceneDynToonLightManager::GetLightSource(chara +0x5b4)` (or the unlit
  one) and the chara matrix; Chara vt[133] 0x27cf10 sets [2] +0xc (+
  `SetCharaSubBufferDrawType`), vt[135] 0x284630 sets [2] +8; vt[134]
  0x282f70 reads +0xc. So stacked parts drew with the default light and
  another draw's sub-buffer = the angle bug. Block indices: only vt47's
  0x29e020 calls `AssignBlockIndexToMaterial` (0 for all) - not the cause.
  **Fix (retest 16, untested):** after activating a stacked part,
  `ShareCharaCallbacks` points its entries [1]/[2] at the shown part's
  functors (non-owning entries; same resource map, unloaded together).

## Equip-screen builder stacking (1.27 static, 2026-10-31; test 8, untested)
* Retest 19's holder rotation lost: the equipped's holder still took the
  slot for wig extras, and the resource path's key-10 wig drawn at screen
  entry stayed. Replaced by stacking on the builder's own Chara.
* Builder apply 0x65e290(builder): returns 1 untouched when +0x324 == 0,
  0 when ready check 0x65bc90 fails (resource + every holder of the current
  buffer ready); else frees the other buffer's holders and attaches the
  current buffer's (vector at +0x80 per buffer). Called by update 0x65d7b0
  at 0x65d844 (state 4, inside the Monitor lock +0x218) and 0x65d9b4
  (state 2). Builder Chara at +0xb8.
* Attach = Chara vt+0x368 (0x27eaa0): UnregisterModel(chara+0x38, slot),
  frees the slot's 3 physics objects (+0x3c0), chara+0x370[slot] = part,
  part vt+0x18, register pass 0x288930 (keys 0..9 only).
* Teardown 0x65b880 (from dtor 0x65b137, rebuild 0x65bb6f) deletes the
  Chara, then the resource, then the holders.
* Mod: hide the extras' holders from apply (move vector end back), then
  activate + RegisterModel their parts at keys 10+ on chara+0x38, share
  the shown part's callbacks; unregister keys 10..29 before each apply.
  Colours (`CharaColour`) are cached per Chara and replayed on them.

## Next RE steps
* 0x29d1f0: holder ready check? (called twice in 0x2997a0).
* Main draw: what render_pass `CommonSceneShadow::addObject(Skeleton*)` /
  the scene draw do with a Skeleton - map walk or GetModel(i) by key.
* vt47's `arg` (r12): what it is, and when vt47 runs relative to
  0x2999b0 (does it re-run on our late register?). vt47 (0x282760) gets
  each holder part with 0x29d2a0, its model with 0x29aee0, then calls
  0x29e020(part, model, r12) at 0x282792 / 0x28285e / 0x28299f - so it
  reaches preloaded holders, our stacked wigs included. (Retest 12: it
  never runs on these screens; who maps materials there is open.)
* Retest 11 (part activation vt+0x18 added): skin-coloured wig parts
  (Namekian antenna) green outside battle, black at some angles in
  previews, battle still the first wig's ramp. 612/1406/1410 on NMC are
  all Hair parts with flag 4 (DytRampsFromTextureEmb), whose skin ramp
  comes from the face's dyt - our activation likely builds the stacked
  part's ramp from its own emb instead. Check the retest 12 dumps first.
