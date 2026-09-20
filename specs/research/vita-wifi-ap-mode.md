# Research: vita-wifi-ap-mode

Date: 2026-09-16
Question: Is it feasible to enable Wi-Fi Access Point (AP/hotspot) mode on the PS Vita through homebrew (kernel plugin / driver / firmware work), so the Vita could host a Wi-Fi network for an Android phone to join — replacing the "shared router" requirement for wireless Android Auto?

## Findings

### TL;DR / verdict

**Not feasible in practice.** The *hardware and the generic Marvell firmware* support
soft-AP (uAP) mode, and the Vita kernel exposes enough low-level SDIO plumbing that a
from-scratch custom driver is *theoretically* possible — but the effort is a multi-month
kernel-driver port with a real chance of failure on firmware/RF calibration, and no
public homebrew has ever done it. The stock Vita Wi-Fi stack is STA + IBSS (ad-hoc)
only; there is no AP mode to unhook or re-enable.

**Note on the "no shared router" goal (updated after on-device testing):** the two
obvious substitutes for a shared router are both dead ends. Phone-as-hotspot is
**confirmed broken** — the phone's head unit server accepts the TCP connection but never
replies to the version request over the phone's own hotspot (Android tethering network
isolation); observed on-device as `st` stuck at 1 with `rx=0`. Vita-as-AP is not feasible
(see below). The only practical in-car answer is a **travel router** that both the phone
and Vita join as clients.

What follows separates source-backed **facts** from **hypotheses** and **speculation**.

---

### 1. The Marvell 88W8787/88W878S firmware & host-driver interface

**Facts (source-backed):**

- The Vita's Wi-Fi/Bluetooth/FM combo SoC is the **Marvell 88W878S-BKB2 "Avastar"**,
  part of the **88W8787 / SD8787** family, connected via **SDIO**. psdevwiki states:
  *"Module based on Marvell SD8787."* (psdevwiki `Wireless_communications`, retrieved
  via Wayback: web.archive.org snapshot of
  https://www.psdevwiki.com/vita/Wireless_communications).
- The upstream Linux driver for the SD8787 is **`mwifiex`** (SDIO transport:
  `mwifiex_sdio`), **not** `libertas_sdio`. `libertas_sdio`/`sd8787` targets the older
  8385/8686/8688 chips. The 88W8787 is a "fullmac" part handled by `mwifiex`.
  (linux-firmware `WHENCE`: *"Driver: mwifiex - Marvell Wi-Fi fullmac-type 802.11n/ac
  cards"*).
- The firmware for the SD8787 is **`mrvl/sd8787_uapsta.bin`, version W14.68.35.p66**.
  The `uapsta` suffix means a **single combined image supporting both uAP (micro/soft
  AP) and STA**. This is confirmed in linux-firmware `WHENCE` and the
  `linux-firmware/mrvl/` directory listing (which contains `sd8787_uapsta.bin` plus
  `sd8797_uapsta.bin`, `sd8897_uapsta.bin`, etc.).
- **Firmware is downloaded at runtime by the host driver over SDIO** — it is not
  permanently flashed on the chip. The host first loads a "helper" bootloader image
  (`mwifiex_prog_fw_w_helper` in `sdio.c`), then downloads the main firmware in blocks
  (`mwifiex_sdio_dnld_fw`). The firmware block header is
  `struct mwifiex_fw_header { u32 dnld_cmd; u32 base_addr; u32 data_length; u32 crc; }`
  (`fw.h`). There is **no cryptographic signature** in the download path — only a CRC.
  This means the *chip* does not authenticate the firmware; the host is free to upload
  any blob.
- **AP mode configuration is done entirely via host commands** (not on-chip registers).
  The relevant host-command opcodes in `mwifiex/fw.h` are:
  - `HostCmd_CMD_UAP_SYS_CONFIG` (0x00b0)
  - `HostCmd_CMD_UAP_BSS_START` (0x00b1)
  - `HostCmd_CMD_UAP_BSS_STOP` (0x00b2)
  - `HostCmd_CMD_UAP_STA_DEAUTH` (0x00b5)
  - `HostCmd_CMD_802_11_AD_HOC_START/JOIN/STOP` (0x002b / 0x002c / 0x0040) — ad-hoc/IBSS
  - `HostCmd_CMD_P2P_MODE_CFG` (0x00eb) — Wi-Fi Direct / P2P-GO
  - `HostCmd_CMD_FUNC_INIT` (0x00a9), `HostCmd_CMD_SET_BSS_MODE` (0x00f7),
    `HostCmd_CMD_ADD_NEW_STATION` (0x025f)
  - The uAP start sequence is `UAP_SYS_CONFIG` → `UAP_BSS_START`
    (`mwifiex/uap_cmd.c`: `mwifiex_uap_prepare_cmd` / `mwifiex_uap_set_channel`).
- The driver defines three BSS roles / driver modes: **`MWIFIEX_DRIVER_MODE_STA`,
  `MWIFIEX_DRIVER_MODE_UAP`, `MWIFIEX_DRIVER_MODE_P2P`** (`main.h`). A single firmware
  image supports all three.
- The SD8787 SDIO register map is documented in `mwifiex_sdio.c`
  (`struct mwifiex_sdio_card_reg mwifiex_reg_sd87xx`): `base_0_reg=0x40`,
  `base_1_reg=0x41`, `poll_reg=0x30`, `host_int_rsr_reg=0x01`,
  `host_int_mask_reg=0x02`, `host_int_status_reg=0x03`, `io_port_0/1/2_reg=0x78/0x79/0x7A`,
  `card_misc_cfg_reg=0x6c`, `func1_scratch_reg=0x60`. Data moves via SDIO CMD52
  (register access) and CMD53 (bulk data through the IO ports).
- **`uaputl`** is Marvell's userspace utility for configuring uAP; it talks to the
  mwifiex driver through a private ioctl (`UAP_IOCTL`). It is **not** a firmware
  builder. (Community mirrors: `virt2real/uaputl` "Marvel uAP utility",
  `leighbb/uaputl`.)

**Hypotheses (plausible, unverified):**

- The 88W878S is likely a board/package variant of the 88W8787 with the same core and
  firmware interface; "S" may denote a Sony-specific SKU. Not independently confirmed.
- The stock Marvell firmware is closed-source; there is no public toolchain to *build*
  88W8787 firmware. Only the prebuilt blobs ship. (No open firmware-builder was found in
  any search — this is a negative result, i.e. absence of evidence.)

---

### 2. The Vita's actual Wi-Fi firmware blobs and driver stack

**Facts (source-backed):**

- **The Vita's Marvell firmware lives inside a signed kernel module:
  `wlanbt_robin_img_ax.skprx`, with the raw firmware starting at offset 305 (0x131).**
  (psdevwiki `Wireless_communications`: *"Firmware in wlanbt_robin_img_ax.skprx starting
  at offset 305."*) `.skprx` is Sony's Secure Kernel PRX (SELF) format; the firmware is
  plain data embedded in that signed container.
- **`SceSdif` is the SD/SDIO/MMC controller module.** It owns four devices; **device
  index `2` (`SCE_SDIF_DEVICE_SDIO`) is "Wlan/Bt" (SDIO)** — i.e. the Marvell chip.
  (henkaku wiki `SceSdif`, retrieved via Wayback.)
- **`SceSdif` exports a kernel library `SceSdifForDriver` (NID 0x96D306FA)** that is
  exactly the low-level SDIO access a custom driver would need. Documented functions
  include:
  - `sceSdioReadForDriver` / `sceSdioWriteForDriver` (SDIO **CMD52** register access)
  - `sceSdioSetBlockLenForDriver`, `sceSdioChangeBusSpeedForDriver`
  - `wlan_bt_acquiredev`, `wlan_bt_release`
  - `wlan_bt_bulk_transfer` (SDIO **CMD53** bulk data)
  - `wlan_bt_cmd0` (chip reset), `wlan_bt_cmd0_cmd52_sdio`,
    `wlan_bt_initialize_custom_context1/2`, `wlan_bt_get_wlan_context`
  (henkaku wiki `SceSdif`.)
- **The user/kernel Wi-Fi control surface has no AP mode.** The documented pieces are:
  - `SceNetCtl` (user) — infrastructure **STA** connect/scan/state, plus **ad-hoc
    *query*** functions (`sceNetCtlAdhocGetState`, `sceNetCtlAdhocDisconnect`,
    `sceNetCtlAdhocGetPeerList`, `sceNetCtlAdhocGetInAddr`). There is **no**
    `*AdhocConnect`/`*AdhocCreate` and **no** hotspot/AP function.
    (vita-headers `psp2/net/netctl.h`.)
  - `SceWlan` (user, inside kernel module `SceWlanBt`) — only power/flight-mode
    control: `sceWlanGetConfiguration` / `sceWlanSetConfiguration` with flags
    `WLAN_ON/OFF`, `FLIGHT_MODE_ON/OFF` (and likely power-save 6/7).
    (henkaku wiki `SceWlanBt`.)
  - `SceWlanBtForDriver` (kernel, NID 0xFD94FCE9) — BT monitor + config
    (`sceWlanBtAttachMonitorForDriver`, `sceWlanBtSetConfigurationForDriver`, etc.).
    Nothing AP-related. (henkaku wiki `SceWlanBt`.)
- **Ad-hoc (IBSS) mode IS available to homebrew** via the PSP-style `SceNetAdhoc`
  libraries (matching, peer list, sockets). `libVitaAdhoc-Unity` demonstrates
  production use: *"Native Adhoc P2P networking … local WiFi multiplayer … without …
  a router."* (github `Brendonm17/libVitaAdhoc-Unity`.) This proves the Marvell chip in
  the Vita is operated in **at least STA + IBSS** modes by the stock stack.

**Hypotheses:**

- Whether the firmware blob in `wlanbt_robin_img_ax.skprx` also *contains* the uAP code
  (i.e. is a "uapsta"-style build) is **unknown** — it cannot be determined without
  extracting and comparing the blob against `sd8787_uapsta.bin`. Sony never exposed AP,
  so a STA+IBSS-only build is equally plausible.
- The stock `SceWlanDrv` (closed-source) is the only consumer of that blob and only
  issues STA/IBSS host commands; no undocumented AP command is known to exist in it.

---

### 3. Community precedent and prior art

**Facts (source-backed):**

- **No public PS Vita AP/hotspot homebrew exists.** Targeted searches (GitHub repo
  search for "vita wifi/hotspot/ap", web search) returned no project that has made the
  Vita act as a Wi-Fi access point. This is a negative result: absence of prior art.
- The closest prior art is **ad-hoc (IBSS)**, not AP:
  - `Brendonm17/libVitaAdhoc-Unity` — native `SceNetAdhoc` bridge for Vita homebrew
    (local P2P multiplayer without a router).
  - `shoui520/vita-save-sync` — save-data sync over WLAN **or ad-hoc**.
  - PSP games/adrenaline support ad-hoc multiplayer on the Vita (the PSP emulator drives
    the Vita's IBSS mode).
- The **PSP**'s Wi-Fi was reverse-engineered (its `wlan.prx` modules and ad-hoc), but
  that work does not transfer: the PSP used a different Marvell chip (88W8380 family,
  `libertas`/`pda` firmware) and the PSP *never* had AP mode either — it was STA + ad-hoc
  + infrastructure, same as the Vita. So there is no "closed console got AP mode"
  precedent to copy.
- **Marvell firmware tooling:** only prebuilt blobs and the closed Marvell Extranet
  distribution exist. No open firmware builder/modifier for Avastar/88W8787 firmware was
  found. `uaputl` (config utility) and the GPL `mwifiex` driver (full host protocol) are
  the only open artifacts.

**Speculation:**

- The Vita homebrew community has historically prioritized game/emulation and storage
  (`sd2vita`, `storagemgr`) over Wi-Fi, which is why nobody has attempted a uAP driver
  despite the SDIO access being theoretically reachable.

---

### 4. Feasibility paths and their blockers

**Path (a) — taiHEN kernel plugin that drives the Marvell chip directly (port mwifiex).**
*Likelihood: technically plausible but practically prohibitive.*

- How: a `.skprx` kernel plugin resolves `SceSdifForDriver` NIDs
  (`sceSdioReadForDriver`, `wlan_bt_bulk_transfer`, `wlan_bt_acquiredev`, `wlan_bt_cmd0`,
  …), resets the chip, downloads firmware (stock blob from `wlanbt_robin_img_ax.skprx`
  or `sd8787_uapsta.bin`), then issues `FUNC_INIT` → `UAP_SYS_CONFIG` → `UAP_BSS_START`,
  and runs a netdev + DHCP + data path to serve a network.
- This is the **only** path that can actually reach uAP, and the low-level plumbing
  (SDIO CMD52/CMD53 to device index 2) is *already exposed* by `SceSdifForDriver`, so a
  custom SD-controller driver is not required. The `mwifiex` GPL source is effectively
  the full host-side spec.
- **Hard blockers / risks:**
  1. **Device-ownership conflict.** `SceWlanDrv`/`SceWlanBt` already use `SceSdif`
     device index 2 and the OS continuously uses Wi-Fi. A plugin must shut down or
     replace the stock stack, which breaks `SceNetCtl` and risks destabilizing the OS.
  2. **Firmware/RF calibration.** If the stock blob lacks uAP, uploading generic
     `sd8787_uapsta.bin` is unproven on the 88W878S: Marvell fullmac firmware carries
     board-specific calibration (TX power, antenna/PA tables); `mwifiex` even loads a
     separate `cal_data` file (`request_firmware(&adapter->cal_data, …)` in `main.c`).
     Generic firmware may drive the Vita RF front-end incorrectly or not at all.
  3. **Bluetooth loss** — taking over the combo chip disables BT (not needed for AA
     audio, but a system regression).
  4. **Sheer scope** — porting `mwifiex_sdio` + a userspace AP/`hostapd`-like stack +
     DHCP + interrupt/data path on a bespoke embedded OS is a multi-month
     kernel-engineering effort with no community reference.

**Path (b) — Reuse the stock WLAN module, issue AP commands via a hook.**
*Likelihood: essentially nil.*

- There is no AP command in the documented surface (`SceWlan` = power only,
  `SceWlanBtForDriver` = BT monitor, `SceNetCtl` = STA + ad-hoc query). No evidence the
  closed `SceWlanDrv` contains a uAP path. Nothing to hook.

**Path (c) — Replace/patch the firmware blob on disk.**
*Likelihood: nil in isolation.*

- The blob lives in a signed SELF on the read-only `os0` partition; patching requires
  NAND writes. Even if patched, the stock driver would still only send STA/IBSS commands,
  so a blob swap alone cannot produce an AP. Only meaningful as an input to path (a).

**Path (d) — Other routes.**
- **Ad-hoc (IBSS) via `SceNetAdhoc`:** works on the Vita, but **Android phones cannot
  join IBSS/ad-hoc networks** through any normal UI/API (Android supports infrastructure
  and Wi-Fi Direct, not IBSS). Useless for AA.
- **Phone hotspot (phone hosts AP, Vita is a STA client):** already the practical,
  validated topology (`aa-wireless-bootstrap.md`). This is what actually replaces the
  shared router.
- **USB networking / wired AA:** out of scope here but a real fallback.

---

## Sources (URLs, docs, code repos)

- psdevwiki — PS Vita "Wireless communications" (chip ID, SD8787, firmware blob + offset):
  https://www.psdevwiki.com/vita/Wireless_communications
  (retrieved via Wayback: https://web.archive.org/web/2024/https://www.psdevwiki.com/vita/Wireless_communications )
- henkaku.xyz Vita Development Wiki — `SceSdif` (SDIO device index 2 = Wlan/Bt; SceSdifForDriver API):
  https://wiki.henkaku.xyz/vita/SceSdif
  (retrieved via Wayback: https://web.archive.org/web/2024/https://wiki.henkaku.xyz/vita/SceSdif )
- henkaku.xyz Vita Development Wiki — `SceWlanBt` (SceWlan power/flight; SceWlanBtForDriver):
  https://wiki.henkaku.xyz/vita/SceWlanBt
  (retrieved via Wayback: https://web.archive.org/web/2024/https://wiki.henkaku.xyz/vita/SceWlanBt )
- Linux kernel — mwifiex driver (host commands, uAP, SDIO interface, firmware download):
  - https://github.com/torvalds/linux/blob/master/drivers/net/wireless/marvell/mwifiex/fw.h (host-command opcodes)
  - https://github.com/torvalds/linux/blob/master/drivers/net/wireless/marvell/mwifiex/uap_cmd.c (UAP_SYS_CONFIG/BSS_START sequence)
  - https://github.com/torvalds/linux/blob/master/drivers/net/wireless/marvell/mwifiex/sdio.c (SD8787 register map, fw download)
  - https://github.com/torvalds/linux/blob/master/drivers/net/wireless/marvell/mwifiex/main.c (fw + cal_data loading)
  - https://github.com/torvalds/linux/blob/master/drivers/net/wireless/marvell/mwifiex/main.h (STA/UAP/P2P driver modes)
- linux-firmware — `WHENCE` (mwifiex fullmac; `mrvl/sd8787_uapsta.bin` W14.68.35.p66) and `mrvl/` listing:
  https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/WHENCE
  https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/tree/mrvl
- vitaSDK `vita-headers` — `SceNetCtl` (STA + ad-hoc query only), `SceNet`, `SceNetAdhoc`:
  https://github.com/vitasdk/vita-headers/blob/master/include/psp2/net/netctl.h
  https://github.com/vitasdk/vita-headers/blob/master/include/psp2/net/net.h
  https://github.com/vitasdk/vita-headers/blob/master/include/psp2/net/adhoc_matching.h
  https://github.com/vitasdk/vita-headers/blob/master/include/psp2/net/pspnet_adhoc.h
- `Brendonm17/libVitaAdhoc-Unity` (Vita ad-hoc/IBSS from homebrew, no router):
  https://github.com/Brendonm17/libVitaAdhoc-Unity
- `shoui520/vita-save-sync` (WLAN or ad-hoc):
  https://github.com/shoui520/vita-save-sync
- `uaputl` (Marvell uAP config utility, not a firmware builder):
  https://github.com/virt2real/uaputl , https://github.com/leighbb/uaputl
- Internal prior research: `specs/research/vita-hardware.md`, `specs/research/aa-wireless-bootstrap.md`

## Implications for PSVitaAuto

- **Do not plan the wireless-AA topology around Vita AP mode.** It is not achievable in
  any reasonable timeframe, and no public homebrew has done it.
- **Phone hotspot is also ruled out** (on-device test): the head unit server does not
  answer over the phone's own hotspot (tethering isolation; TCP connects but
  `VERSION_RESPONSE` never arrives). This contradicts the earlier untested assumption in
  `aa-wireless-bootstrap.md`.
- The only practical way to avoid a fixed shared router is a **travel router** (phone +
  Vita both join it as clients). The head unit server is LAN-reachable on the travel
  router's network, exactly like the shared-router case that already works.
- Ad-hoc (`SceNetAdhoc`) is a dead end for AA: Android phones cannot join IBSS networks.
- The only technically interesting but non-viable-for-v1 option is path (a) — a kernel
  plugin driving the Marvell chip via `SceSdifForDriver` + a ported mwifiex uAP stack.
  Treat it as explicitly out-of-scope research, not a plan item.
- No change to the input/video/audio/TLS plans: those are independent of the Wi-Fi
  topology.

## Remaining unknowns

- **Does the Vita's firmware blob (`wlanbt_robin_img_ax.skprx`, offset 305) contain the
  uAP code?** Unverifiable without extracting the blob and diffing it against
  `mrvl/sd8787_uapsta.bin` (string/segment comparison). This is the decisive unknown for
  path (a).
- **RF calibration compatibility:** whether generic `sd8787_uapsta.bin` (or the stock
  blob) drives the 88W878S RF front-end correctly on the Vita board — only answerable by
  a hardware experiment.
- **Stock-driver coexistence:** whether `SceWlanDrv`/`SceWlanBt` can be cleanly shut
  down and the chip reprogrammed without destabilizing the OS — only answerable by a
  kernel-plugin spike.
- Whether the `SceSdifForDriver` `wlan_bt_*` functions are sufficient (interrupts/data
  path) for a full mwifiex port, or whether additional undocumented SDIO hooks are
  required — only answerable by a spike on real hardware.

## Recommended next steps

1. **Adopt the shared-router / travel-router topology for v1.** Confirm the travel-router
   variant on hardware (phone + Vita both join the travel router; Vita connects to
   `phone-ip:5277`). Do not rely on phone hotspot (broken) or Vita AP (not feasible).
2. **Do not pursue AP mode as a feature.** Record this note as the feasibility verdict;
   update `specs/research/vita-hardware.md` "Remaining unknowns" (item: "whether any
   homebrew AP-mode wifi is feasible") to point here.
3. **(Optional, curiosity-only spike, not gating v1):** dump `wlanbt_robin_img_ax.skprx`
   from a firmware image, extract bytes at offset 305, and compare against
   `mrvl/sd8787_uapsta.bin` to settle the "does the stock blob contain uAP" question.
   This is the single cheapest piece of evidence to de-risk the only theoretically-viable
   path (a).

### What the feature spec can rely on (vs. needs a spike)

- **Can rely on (source-backed):** the chip is an SD8787-family Marvell fullmac part on
  SDIO; its generic firmware supports uAP; the host interface is fully documented by the
  GPL `mwifiex` driver; the Vita's stock Wi-Fi stack is STA + IBSS only; `SceSdifForDriver`
  exposes SDIO CMD52/CMD53 to the Wlan/Bt device; no public Vita AP homebrew exists.
- **Needs a spike (prototype experiment):** (i) whether the stock firmware blob contains
  uAP code; (ii) whether generic/stock firmware works on the Vita RF front-end;
  (iii) whether the stock Wi-Fi driver can be displaced without destabilizing the OS.
  All three are only answerable on real hardware with a kernel plugin.
