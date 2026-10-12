# Nouveau and its neighbours, vendored unmodified

These files are copied byte for byte from Linux **v6.12**, like the rest of
`third_party/linux/` (see `README.md` next to this file). They are the
nouveau driver as Linux's Kbuild builds it on x86 without ACPI, debugfs,
LEDs, the backlight, SVM or the Tegra platform driver
(`linuxkpi/nouveau/files.mk` lists the `.c` files), plus what it needs on top
of phase 2's DRM core:
- the DisplayPort, DSC, HDMI and SCDC helpers (`drivers/gpu/drm/display/`);
- the GPU scheduler, `drm_exec` and `drm_gpuvm`;
- `i2c-algo-bit` and the maple tree (GPL-2.0-or-later).

## Licenses

Nouveau's files carry MIT terms, as an SPDX line or as the license text.
A few other licenses appear:
- `drm_gpuvm` is `GPL-2.0-only OR MIT`, used here under MIT.
- The DisplayPort helpers carry the HPND terms.
- `include/nvrm/535.113.01/nvidia/kernel/inc/vgpu/rpc_global_enums.h` has no notice of its own. Its first line says it is an excerpt of NVIDIA's open-gpu-kernel-modules at tag 535.113.01. That repository licenses its files as MIT (its `COPYING`), and the original file there is `src/nvidia/kernel/inc/vgpu/rpc_global_enums.h`.

**Not copied.** These files carry no license notice at all, and in Linux that means GPL-2.0-only:
- 53 nouveau headers. Kestrel's Linux KPI writes its own versions under `linuxkpi/nouveau/`. That tree mirrors the driver's directories and comes first on the include path. The declarations match what the vendored `.c` files use; the text, comments and include guards are Kestrel's.
- `drivers/gpu/drm/display/drm_dp_mst_topology_internal.h`, which is GPL-2.0-only. It holds only three declarations, so the KPI writes it as `linuxkpi/include/drm_dp_mst_topology_internal.h`.

The headers written in the KPI:

- `dispnv50/atom.h`
- `dispnv50/base.h`
- `dispnv50/core.h`
- `dispnv50/curs.h`
- `dispnv50/disp.h`
- `dispnv50/head.h`
- `dispnv50/lut.h`
- `dispnv50/oimm.h`
- `dispnv50/ovly.h`
- `dispnv50/wimm.h`
- `dispnv50/wndw.h`
- `include/nvfw/acr.h`
- `include/nvfw/pmu.h`
- `include/nvfw/sec2.h`
- `include/nvif/clb069.h`
- `include/nvif/disp.h`
- `include/nvif/fifo.h`
- `include/nvif/if0008.h`
- `include/nvif/if000a.h`
- `include/nvif/if000b.h`
- `include/nvif/if000c.h`
- `include/nvif/if000d.h`
- `include/nvif/if500b.h`
- `include/nvif/if500d.h`
- `include/nvif/if900b.h`
- `include/nvif/if900d.h`
- `include/nvif/ifb00d.h`
- `include/nvif/ifc00d.h`
- `include/nvif/mem.h`
- `include/nvif/mmu.h`
- `include/nvif/push006c.h`
- `include/nvif/push206e.h`
- `include/nvif/push507c.h`
- `include/nvif/push906f.h`
- `include/nvif/pushc37b.h`
- `include/nvif/timer.h`
- `include/nvif/user.h`
- `include/nvif/vmm.h`
- `include/nvkm/core/falcon.h`
- `include/nvkm/core/oclass.h`
- `include/nvkm/subdev/fault.h`
- `include/nvkm/subdev/gsp.h`
- `nouveau_mem.h`
- `nouveau_svm.h`
- `nouveau_vmm.h`
- `nvkm/engine/fifo/runl.h`
- `nvkm/subdev/acr/priv.h`
- `nvkm/subdev/fault/priv.h`
- `nvkm/subdev/mmu/mem.h`
- `nvkm/subdev/mmu/umem.h`
- `nvkm/subdev/mmu/ummu.h`
- `nvkm/subdev/mmu/uvmm.h`
- `nvkm/subdev/mmu/vmm.h`

## Files

| File | License (SPDX or terms) | SHA-256 |
|---|---|---|
| `drivers/gpu/drm/display/drm_display_helper_mod.c` | MIT | `3b0935abf7d6d92312c54bb4e7f29bf3544666b4d80befec432eb256f275e42c` |
| `drivers/gpu/drm/display/drm_dp_helper.c` | HPND (license text) | `d3ca8955a877e78c63a426a3fa41851b860a65f93e2ed3b9e406882e941d7abf` |
| `drivers/gpu/drm/display/drm_dp_mst_topology.c` | HPND (license text) | `dff367bc38923fe62db7a9c4c25b49b026843b17e6a956eb4b9e10f88c1a382f` |
| `drivers/gpu/drm/display/drm_dp_dual_mode_helper.c` | MIT (license text) | `65ff2aef14985a44302136bba9be615b6326c7e7a83ffc8e3e96a3fa5cb957c6` |
| `drivers/gpu/drm/display/drm_dsc_helper.c` | MIT | `66a2b562e94231a22a48bdfd3ceef75092f50976618939eb05aab2272a2ff088` |
| `drivers/gpu/drm/display/drm_hdmi_helper.c` | MIT | `bac4c2b4369f2001edafce341dd4097176aae53a6a50b5c57a1dd2bb265d6c40` |
| `drivers/gpu/drm/display/drm_scdc_helper.c` | MIT (license text) | `0bf5ad4449b95e6c5b75b2714bcb5e947b47df2f7ea0f0e52be378c75e5423a1` |
| `drivers/gpu/drm/display/drm_dp_helper_internal.h` | MIT | `caf04f4863f2688450bd613b8f0526e4d498f80ef39904ae99e34a654bd28fa3` |
| `drivers/gpu/drm/drm_exec.c` | GPL-2.0 OR MIT | `d9f8216b0584c4d53d8ee388a6d9eef77afe02135698ed9bd7db42c68c46d5e4` |
| `drivers/gpu/drm/drm_gpuvm.c` | GPL-2.0-only OR MIT | `3283fa63bd5cbff93b5fec8af28b2a0f4fc0703db388cff539cd9319c04eb2a9` |
| `drivers/gpu/drm/scheduler/sched_main.c` | MIT (license text) | `fa3e62b76a545f8f34d9318fd679311e3a50b90adef667d503a95733f8ae6c13` |
| `drivers/gpu/drm/scheduler/sched_entity.c` | MIT (license text) | `e211bb1a0480340a7c9516fd1201df4d3afe0818699cd8d992ec69748c6d12bd` |
| `drivers/gpu/drm/scheduler/sched_fence.c` | MIT (license text) | `fc3d867308fdb248f2c5dcd860d7de060e918a90d4bde84fec95c700677d97ec` |
| `drivers/gpu/drm/scheduler/gpu_scheduler_trace.h` | MIT (license text) | `fdb72ca54a4e456891246b0048f37a669b08f07e58917269835cba9c41190d16` |
| `drivers/i2c/algos/i2c-algo-bit.c` | GPL-2.0+ | `e3508529296d155a89f8d29e4a91ecc16005961a1c09654fae4ea10c5cf82ea6` |
| `include/linux/i2c-algo-bit.h` | GPL-2.0+ | `a00e2d04ea903270c504fdcc32b5287e6b412d48f11251f51c8d91cdaa18701d` |
| `lib/maple_tree.c` | GPL-2.0+ | `d803068bad411aa338b3d501d9c47069687df77d6e8c0357f58d26a689f9e3cc` |
| `include/linux/maple_tree.h` | GPL-2.0+ | `a8504155b6ad0cd6872c520ddd38ac25b224967a24df8d651e8856cd78846c6e` |
| `include/uapi/drm/nouveau_drm.h` | MIT (license text) | `2758ed28465a923c5a0eb1c2d2a4af7b43a7fc52e98b38723924a3fae48ccc39` |
| `include/drm/display/drm_dp_dual_mode_helper.h` | MIT (license text) | `5409161cfbd0fc2bac2c1f523f232cc8fb9576a0e1fed75fe08a6050ddaff17d` |
| `include/drm/display/drm_dp_helper.h` | HPND (license text) | `c2e27e816c9cdacbe5e33a4a435ad180d6e060317b7699df68d4b1270c83ee7b` |
| `include/drm/display/drm_dp_mst_helper.h` | HPND (license text) | `7a8a861deb45c45e6ddf06065684e501720a08bcfde330fab12e1cf759515164` |
| `include/drm/display/drm_dsc.h` | MIT | `f57cf45be321cdf4def25491ef54e2adfef9627c18cebc53bd9e2acd0fd2a394` |
| `include/drm/display/drm_dsc_helper.h` | MIT | `dd0a8f30ee0c78d5ff61081f7dd928e85fd58436f6f41367e8d927ed36014f08` |
| `include/drm/display/drm_hdcp.h` | MIT | `d865ee282171ac2888d90dff2b3232558b03fd66286268dba399b34821080943` |
| `include/drm/display/drm_hdmi_helper.h` | MIT | `c67fb4f7f3c5dc2a1d4de26b9ea1a1816811d0f81d6f91c93468e5456881d289` |
| `include/drm/display/drm_scdc.h` | MIT (license text) | `e94fd10ddc89309ac635005b707f7e2ec4bad6914032dcfd307dd0e1a52b2d13` |
| `include/drm/display/drm_scdc_helper.h` | MIT (license text) | `06426dd67bc4c289ec70c5c5d81be26187e7725588d8eefe78523609708b7f8e` |
| `include/drm/i2c/ch7006.h` | MIT (license text) | `3957aa6ef7b7e6ace82e28efb3e0dbccc385febd941d330f5ad949732aa68fc8` |
| `include/drm/i2c/sil164.h` | MIT (license text) | `acda32a928616e5f17438e4a70832cdbfe7dfff53dc81bb945e80c60b81d3801` |
| `drivers/gpu/drm/nouveau/dispnv04/arb.c` | MIT (license text) | `280faaf41927dfb59f9ea8da2c22cb410713028c40ae91e839e1bb7419ffa76a` |
| `drivers/gpu/drm/nouveau/dispnv04/crtc.c` | MIT (license text) | `f2c5944cfd52b60e1c7b29c85a1e43336b97615756483e637df440be0c11bdbf` |
| `drivers/gpu/drm/nouveau/dispnv04/cursor.c` | MIT | `a07d5ea6e7522c3110a0580f83fad437e11324bd9b615b20f9ef9999821834b2` |
| `drivers/gpu/drm/nouveau/dispnv04/dac.c` | MIT (license text) | `b1f8da918f9fbec3bbcfe0d4145a28e55ecb8c6874d19ec21b8eb9470dcb2a72` |
| `drivers/gpu/drm/nouveau/dispnv04/dfp.c` | MIT (license text) | `aeae4c211be96acf03aec4a8a6603898970ba5d19ff1863672a17448c2f9fe90` |
| `drivers/gpu/drm/nouveau/dispnv04/disp.c` | MIT (license text) | `bbed85eef4d51711337f9a172dda2d831e3c6fec65897542a6dc84faddc46179` |
| `drivers/gpu/drm/nouveau/dispnv04/disp.h` | MIT | `e11c5da2e30869948db3726c230b6144c4c61be6bd82a208f5f7588173cf6cf4` |
| `drivers/gpu/drm/nouveau/dispnv04/hw.c` | MIT (license text) | `a4e1b4c955a1d43b06c58176d62c540070e459d71faeddd6077b6d7dbe9f0b50` |
| `drivers/gpu/drm/nouveau/dispnv04/hw.h` | MIT (license text) | `979d8f95088c592208e018db22df34670a1a2f06b35054112ea95b1d6bbfddd2` |
| `drivers/gpu/drm/nouveau/dispnv04/nvreg.h` | MIT (license text) | `c2c46c2f5a2d4e1039074d42e32e1354f8388ae17011e7360c95bd9b3ae91921` |
| `drivers/gpu/drm/nouveau/dispnv04/overlay.c` | MIT (license text) | `f4641c6a8bc50c716f390c79695bd2a917c7a0c2042fb7d943f557a0c7c59230` |
| `drivers/gpu/drm/nouveau/dispnv04/tvmodesnv17.c` | MIT (license text) | `4b13384cb4bd029b2d29110a28edbc80caf3808f12edd4abdc9d14160f5b7537` |
| `drivers/gpu/drm/nouveau/dispnv04/tvnv04.c` | MIT (license text) | `f999fafb7ba0235f41b1d28e0b3344302aca6c455c0b88ad1f4e5516eb223b17` |
| `drivers/gpu/drm/nouveau/dispnv04/tvnv17.c` | MIT (license text) | `22b04c45dc176fa33930211b537ad12ee339e6ef0f5ce14f4a811e90eb94aa27` |
| `drivers/gpu/drm/nouveau/dispnv04/tvnv17.h` | MIT (license text) | `66a7ea1b13539e6eaceea7e12ef1ac7310f8c33228083af9f7c230d09447a0e5` |
| `drivers/gpu/drm/nouveau/dispnv50/base.c` | MIT (license text) | `a52ca6360000653fe92a7704289f5d428c4de1178d61a8cc41a378c307cdf834` |
| `drivers/gpu/drm/nouveau/dispnv50/base507c.c` | MIT (license text) | `cf1f300628913f2f6d35fdce4fef059fb7add65ca18579d6507d0a126561bb06` |
| `drivers/gpu/drm/nouveau/dispnv50/base827c.c` | MIT (license text) | `ee1dc2c18bca1672d17907f5f7497a3160f2836424af57ab2576cfac775250dd` |
| `drivers/gpu/drm/nouveau/dispnv50/base907c.c` | MIT (license text) | `4df7822c2017b027e0ca65b1deb310b0fc6bdb1474961920f2ea0af7c9c6bd52` |
| `drivers/gpu/drm/nouveau/dispnv50/base917c.c` | MIT (license text) | `c5f51e9e087337686ea769794747d2c20386288e2534d1e4b0649fb601992d54` |
| `drivers/gpu/drm/nouveau/dispnv50/core.c` | MIT (license text) | `fe6e365c45d4c7265e8ea37907071e0c39414bc93e963ae4fc77ae0ca0f3e091` |
| `drivers/gpu/drm/nouveau/dispnv50/core507d.c` | MIT (license text) | `5b46030ff5360d40161ce511cbdf555398c2174788ccea9344db9bd8b866e601` |
| `drivers/gpu/drm/nouveau/dispnv50/core827d.c` | MIT (license text) | `8c220e8e0fe3cad98cc0cec8fc75ad033a88a0761de17bf648457e4a72b18fc3` |
| `drivers/gpu/drm/nouveau/dispnv50/core907d.c` | MIT (license text) | `18858260615437662a2d59b4bde8f6002ce34914a26f0b4089b239ef94a75941` |
| `drivers/gpu/drm/nouveau/dispnv50/core917d.c` | MIT (license text) | `492c2ad6479321a5b3c96de1f4bf4720e5a3790e4c67e8539a3fd864da8ba011` |
| `drivers/gpu/drm/nouveau/dispnv50/corec37d.c` | MIT (license text) | `0e4da808e861ce8be756dfc5d38504a9cfedf197b6e0463abe633d87d46f4b10` |
| `drivers/gpu/drm/nouveau/dispnv50/corec57d.c` | MIT (license text) | `d7417435979869c0cb954566b5154db76710a5e43cb4d41482f239af7df677ca` |
| `drivers/gpu/drm/nouveau/dispnv50/crc.h` | MIT | `6b68fd15124e5a68cd6a2d4e9345e0c534e317b498c3516f2d6af38a3a7137c6` |
| `drivers/gpu/drm/nouveau/dispnv50/crcc37d.h` | MIT | `98731e2e97f58e9b017c0ab146b87386db98b80cd4acef692e63a03ccdfea95b` |
| `drivers/gpu/drm/nouveau/dispnv50/curs.c` | MIT (license text) | `d4a72ca7e37514c3fd08d6c2a88bc89cd323355cecf0f64c50021c79b6d050f6` |
| `drivers/gpu/drm/nouveau/dispnv50/curs507a.c` | MIT (license text) | `089e354f71c79ef5cbb1faeeb00c4135fbc5fffc03d56530dc9993a9b2a037c4` |
| `drivers/gpu/drm/nouveau/dispnv50/curs907a.c` | MIT (license text) | `722bc1c7e2b30ed8f7dc949f980e0d91345b4c550c562781ab33074ced7c6c75` |
| `drivers/gpu/drm/nouveau/dispnv50/cursc37a.c` | MIT (license text) | `98a773704e0353427b4e2b37b1dac34a2499e990822408119bffdce6f765b894` |
| `drivers/gpu/drm/nouveau/dispnv50/dac507d.c` | MIT (license text) | `c8eecad0eea633333ade293c4c6bc4a8e6cdb0aad7888ece913d90a01b001e9b` |
| `drivers/gpu/drm/nouveau/dispnv50/dac907d.c` | MIT (license text) | `c8717be5fb494f07b3bea041677e6a1ddd45d4ed435b70621a5e6fa956bdd3e3` |
| `drivers/gpu/drm/nouveau/dispnv50/disp.c` | MIT (license text) | `83c808e598f3b791c39938b33c859abcbd16ba0283f713196817713a0e720093` |
| `drivers/gpu/drm/nouveau/dispnv50/handles.h` | MIT | `6dc6ee215ea5001c851a0a78dc36989bf326e04f22825ba2b01afaf639a0b5a8` |
| `drivers/gpu/drm/nouveau/dispnv50/head.c` | MIT (license text) | `93344b49b482c26089597eac71963f211ccaeaee67988eb4b0368c2dd4876150` |
| `drivers/gpu/drm/nouveau/dispnv50/head507d.c` | MIT (license text) | `d7a07d5f683d23bd4031e3648cdc40fbae5cb45f1dec573d7638eaaa4a671384` |
| `drivers/gpu/drm/nouveau/dispnv50/head827d.c` | MIT (license text) | `c172110b6f646d56ec819aa9a502bab8c629b8a90cd0537aab45ef2acf2e4582` |
| `drivers/gpu/drm/nouveau/dispnv50/head907d.c` | MIT (license text) | `2c8ff5508af5eb1946d1c9a566275a6b15bae06fc812d9997614ebfb8bdb8813` |
| `drivers/gpu/drm/nouveau/dispnv50/head917d.c` | MIT (license text) | `6f0645bbf9d8d4fc55f148622204105271b4a3200622aa105e630bc4072f1385` |
| `drivers/gpu/drm/nouveau/dispnv50/headc37d.c` | MIT (license text) | `ec91ee50eb67125b512d9f4dd83e99dee66b3671fccb7f6cd0d6574100a16e29` |
| `drivers/gpu/drm/nouveau/dispnv50/headc57d.c` | MIT (license text) | `44d52e037a69405c9693314029d3100dad3cdfa7c882285cf62425c6a0c4ba69` |
| `drivers/gpu/drm/nouveau/dispnv50/lut.c` | MIT (license text) | `7ef919874437b3d2f25e31c05941338d4bab78ca512551da231d8745e5729021` |
| `drivers/gpu/drm/nouveau/dispnv50/oimm.c` | MIT (license text) | `3aa9891f078f8157ac8108c2c7bd96de44658d588d694a82922bcaa1adbee6fe` |
| `drivers/gpu/drm/nouveau/dispnv50/oimm507b.c` | MIT (license text) | `e0b1543bc81abe78912d9ff95d54a3350d8680b0f39676e739d5f9a1caa7f10a` |
| `drivers/gpu/drm/nouveau/dispnv50/ovly.c` | MIT (license text) | `355a9463007ad93e6ba939e1556989dcaadc2e1c284a17da5982b09c4698e2ed` |
| `drivers/gpu/drm/nouveau/dispnv50/ovly507e.c` | MIT (license text) | `f9a3ce6d97587c7270c5adf68aed55c4d664fd2fe9ba242e6478ea9f10450932` |
| `drivers/gpu/drm/nouveau/dispnv50/ovly827e.c` | MIT (license text) | `be428fd3e3a0f3c43a90562174cc56e1485bec5835813a30d4041873587cc086` |
| `drivers/gpu/drm/nouveau/dispnv50/ovly907e.c` | MIT (license text) | `b4ae320b3961ae2dbfec9565fbcd681d172d9687e46ad306631154851d682823` |
| `drivers/gpu/drm/nouveau/dispnv50/ovly917e.c` | MIT (license text) | `42df4aec04a06df66e01c3497a2359c3785929bd2c7358445dfaa873b44e31f8` |
| `drivers/gpu/drm/nouveau/dispnv50/pior507d.c` | MIT (license text) | `687a56a9d631acde43fa4192c7ddd40a579dd766bfeb1404495419d56a4284bd` |
| `drivers/gpu/drm/nouveau/dispnv50/sor507d.c` | MIT (license text) | `23f0ffe9afcc48186bb9fb60132c6c3bd86d8006a24680a82de146ba9594ff60` |
| `drivers/gpu/drm/nouveau/dispnv50/sor907d.c` | MIT (license text) | `d1c956e443154e1a538ce5f5fe29efea091cb2be6df1cf7ec4dd2ff08ae5c2be` |
| `drivers/gpu/drm/nouveau/dispnv50/sorc37d.c` | MIT (license text) | `204b42cee9874102a98bc397c800d226b9d188ce9c466747bbed066b945b2c3a` |
| `drivers/gpu/drm/nouveau/dispnv50/wimm.c` | MIT (license text) | `dcd2a7a09ea0700b4eb7d4639140a9e7a23bb14a2b4cddca50b1e358331ae70f` |
| `drivers/gpu/drm/nouveau/dispnv50/wimmc37b.c` | MIT (license text) | `6f736cecb68f1b10cc569ed792b7452f3e35689081c9d6494b73d64ecd1f8b9b` |
| `drivers/gpu/drm/nouveau/dispnv50/wndw.c` | MIT (license text) | `3f2f5bfd4769707325a8d7d32dc1b4ef91d42ab5876aaf77ff02e441f93ff883` |
| `drivers/gpu/drm/nouveau/dispnv50/wndwc37e.c` | MIT (license text) | `06f81ffa4b2ee0c66486e68d281ea04ad9473c40695f556a23fbb7ab28375b04` |
| `drivers/gpu/drm/nouveau/dispnv50/wndwc57e.c` | MIT (license text) | `498f293150e76bc609826eeac37bb9a0dd9d33b541c6bf9884e42e196bb6ff92` |
| `drivers/gpu/drm/nouveau/dispnv50/wndwc67e.c` | MIT (license text) | `8ca6aa5ddad890ffc7db6d30d563998de7474a6a6a06d282ebe0920baa907f34` |
| `drivers/gpu/drm/nouveau/include/nvfw/flcn.h` | MIT | `d3c894670485f624a0efed951e2fe16546c976158b034ada566dcf33eb0273fb` |
| `drivers/gpu/drm/nouveau/include/nvfw/fw.h` | MIT | `29524e3b1586a674924d5019a2f694b5eb47737032e75c91b4ba2c443cf5e27a` |
| `drivers/gpu/drm/nouveau/include/nvfw/hs.h` | MIT | `d9d2e5d09de9af6f943a9a7a997a571585c63285a2cc7e43c77c8c41b2449580` |
| `drivers/gpu/drm/nouveau/include/nvfw/ls.h` | MIT | `f1611627263b9911a57d80efd905c13327c687ba8642c638d66161dfed5b44b2` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl0039.h` | MIT (license text) | `b48dbab58fb4a57ff69101ad8b4298878083f9dfcc5165f37ab218cbf60bc0c8` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl006c.h` | MIT (license text) | `b1ad3ee6536df427d73c4cb66a0122386f97dd4a08e0941850622e7910f8719e` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl006e.h` | MIT (license text) | `864a062f31c269075043590dcf6caa996dbf3db8e78c0eaf4fb7497e929f0947` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl176e.h` | MIT | `9aa63dfd49c1f7d29eb2f13ed5c54cbedd56fc5464ca35ed775d8fd0a5e95efa` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl206e.h` | MIT (license text) | `c19092b4c3a65bbbec1d821d1b1bd7fda79f432b226f18385d363376d23eebab` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl502d.h` | MIT (license text) | `c536d40435893d0ca2beb4ec864be66382acd74006265b112717fe154fa2bdf0` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl5039.h` | MIT (license text) | `052d35d744f9c4c65622c4a08117c5dfeb5eedaad9781388295e1c842b4e7854` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl507a.h` | MIT (license text) | `6b01e72636e899c74bf6539c16a3318e11dfd9dd3d898cf97b974cc4918a52fb` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl507c.h` | MIT (license text) | `dd5abaae462eeac2108b704091d78d0860a86698a9857e23b4bc84b9cafd47c5` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl507d.h` | MIT (license text) | `e509ad453500ed3abaa118e587d585d2cecb2c98be63dbdb8d6b2ca0def1f5b3` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl507e.h` | MIT (license text) | `5823dac07d6b6be0590a602b389f10233cd28e83b730de456a8dc08b8a0ff219` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl826f.h` | MIT (license text) | `903c34b35b70f6155feb7287c74ffc688f340e94a530a2d8c3d61a468631eadd` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl827c.h` | MIT (license text) | `3ca80d602bd3660e344a86402b7befe6d9225602d3e4a8f57597237e0a078033` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl827d.h` | MIT (license text) | `1f399e31f44a807af749a9957dfb48f9fd90fa038e3b020bdefece121a6a45e4` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl827e.h` | MIT (license text) | `c44f4a3a286d5e95f9c4a8e095e97a7cf99f5535f61e3dfc3b40ad592a77b271` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl837d.h` | MIT (license text) | `ee91d43dc5f2a33545ad2fb0dbd2a5cd7da8ac16b7d81b280f497486990c1958` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl887d.h` | MIT (license text) | `7507a9ad5b57583e6d096221ed7e8f40e59bbf676bee6fd6e66eb98b9e1c3b11` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl902d.h` | MIT (license text) | `a9e7be73142be16341732880a9066a864de6ee4d39e574804d5a47bafd01c6bf` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl9039.h` | MIT (license text) | `fbdb4e961d90b6b037e29d661d8bdb36043481e6f22a302e079c224ab5e3e828` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl906f.h` | MIT (license text) | `1175fcb508625f593c939d68f7fefa12bec22c7b285cfd836ee23c1e4c6cb220` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl907c.h` | MIT (license text) | `eb45ed47ffb368e7449c65c0a525776f52a66ba09580180694ea4aeeecdb8302` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl907d.h` | MIT (license text) | `758799576b60d93cefaee681a991516d78b0aa57ef681963b02ab4c68fe9c351` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl907e.h` | MIT (license text) | `4679266fbab1f4e04113c9d6d30ff3129c1bcc62817135a519a270b4fdd5a27a` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cl917d.h` | MIT (license text) | `7eedbe66dea8e432af2771d4bd7ab321f019602e34e54c6525d73e947e3652ce` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/cla0b5.h` | MIT (license text) | `e2549e4c91db4db712df0f4c5d94bf5b788fd20f97474d2a1271f4d00999df39` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/clc37a.h` | MIT (license text) | `2e2e032499fd9ce3f360f22f62d313d6abf5f1cb72768cce75c04df7af87730c` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/clc37b.h` | MIT (license text) | `0edcb98beb8e3465722abf7acfb2e64b3a5091f044d56d544f50c261f215e94c` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/clc37d.h` | MIT (license text) | `68731fa6048f998c97d0f32381af9b73b35c776fe67d74736dcc9920b393a0e4` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/clc37e.h` | MIT (license text) | `656805bbe64ec4b195ffdc321e2f515ce509079af6848511ec2c75e84994548d` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/clc57d.h` | MIT (license text) | `3000096def4e707a9e8df16268cfeb991dde284fd2de7e2139b7384daf604cce` |
| `drivers/gpu/drm/nouveau/include/nvhw/class/clc57e.h` | MIT (license text) | `c8db68b061526f22d1c12cb0fad26f1852f73ec971f3eadcf4ad3093804246d7` |
| `drivers/gpu/drm/nouveau/include/nvhw/drf.h` | MIT (license text) | `0c2187b2824a1fb0d259cc04b9c1458160ec5ef390bc67f4afe9e54f815f4ca4` |
| `drivers/gpu/drm/nouveau/include/nvif/cl0002.h` | MIT | `f1287da965212a5d9aa34feadb55d1db76fb7f48b308d8e06a180cbf97233079` |
| `drivers/gpu/drm/nouveau/include/nvif/cl0046.h` | MIT | `f8b35695bf811d56ca32422e51142006fcaafdbd63e232559730e95b4fb97579` |
| `drivers/gpu/drm/nouveau/include/nvif/cl0080.h` | MIT | `95d1e18f249f21aab3198b5636bf4b4385a5c47a5309d82098615a503eaefef9` |
| `drivers/gpu/drm/nouveau/include/nvif/cl9097.h` | MIT | `ee95b74d16f0393aeeb675007579d3c917d542c8970fd7dda7d8b66bea375fe3` |
| `drivers/gpu/drm/nouveau/include/nvif/class.h` | MIT | `a7b577320db6031e3d0ce7beaa0f2b4deffada2cfc9eb9b209fa25e52f2a6cc9` |
| `drivers/gpu/drm/nouveau/include/nvif/client.h` | MIT | `86a00b463395b89742536ffdd3ad687451e4d98187a1ce8a7ee905ef5583b412` |
| `drivers/gpu/drm/nouveau/include/nvif/conn.h` | MIT | `d9c2919ac5ed90cb8fa55d0072cb640d614b31b9ece3de266ead88f8c0d3239d` |
| `drivers/gpu/drm/nouveau/include/nvif/device.h` | MIT | `a369da18f065ceafa9aefe3b6d3f21aa09305ff00ca8218d5e6b7eb6d5e8a5be` |
| `drivers/gpu/drm/nouveau/include/nvif/driver.h` | MIT | `aa3dee5417078c1de5223666bcc0ad6249757a2b9b07e44624d927089e8820f6` |
| `drivers/gpu/drm/nouveau/include/nvif/event.h` | MIT | `e5b65abd588c0cedbbab2fbd29bc03f1acb27109b62c5c8515981dda3b9a0318` |
| `drivers/gpu/drm/nouveau/include/nvif/head.h` | MIT | `68bedfefb799844c93d8f6911baad955bcd34e5f89f50a0ebc4537cc0ca3b995` |
| `drivers/gpu/drm/nouveau/include/nvif/if0000.h` | MIT | `012e0a0e01cd1aa16f6600e0171698809e0fe976bdba43404c4d12aea7c4be30` |
| `drivers/gpu/drm/nouveau/include/nvif/if0001.h` | MIT | `a53e7b33416c7a2b78fb8c97208c595d7ce90db7a315d6bd5921c581af9f50b4` |
| `drivers/gpu/drm/nouveau/include/nvif/if0004.h` | MIT | `2499bafc062ccc6ba0aabc8aeaab7f6d6669e8a875e2f3d3c4981cc331059927` |
| `drivers/gpu/drm/nouveau/include/nvif/if0005.h` | MIT | `8852a228d0dcf81c0beaef44e91aa546c6b2c6bfa3f84305da3975965a8ad6e1` |
| `drivers/gpu/drm/nouveau/include/nvif/if000e.h` | MIT | `7409f5490a532779dfe6b47721557d87eb7873aeee351db53fadec306e783f35` |
| `drivers/gpu/drm/nouveau/include/nvif/if0010.h` | MIT | `67309e29072d55811635b014a7c7ae52342a20cf741e9c0bb4ddf44eaa47c3a9` |
| `drivers/gpu/drm/nouveau/include/nvif/if0011.h` | MIT | `6e96e534b7f5a03a2f94bbd9c4424a3afb54969ba096134e7fdbfcb5cdfc8f42` |
| `drivers/gpu/drm/nouveau/include/nvif/if0012.h` | MIT | `3fbd43d6b7e1968a99b8679653ab4bb392e4e60272e2c3904862bd703322d50e` |
| `drivers/gpu/drm/nouveau/include/nvif/if0013.h` | MIT | `ebb612883c266e6519c00d078d929456c132ef01c1a6fe19706d19b619368e82` |
| `drivers/gpu/drm/nouveau/include/nvif/if0014.h` | MIT | `180f7789d82483c3b92bce23cdb98326354ad6bfde349623a6bf3a345092b796` |
| `drivers/gpu/drm/nouveau/include/nvif/if0020.h` | MIT | `bd52119521bf1ec03d1ca73efdf3391267aa630e91b28bc8b6d4544345d391f1` |
| `drivers/gpu/drm/nouveau/include/nvif/if0021.h` | MIT | `53cb12d82811a928d5d95e802a133924e008341de7aab0b5d8f1196aa0b7be87` |
| `drivers/gpu/drm/nouveau/include/nvif/ioctl.h` | MIT | `c281c6675c09a16478019aa789d8dc81312f19b0d341ee6b6e185e1f71cbc8bc` |
| `drivers/gpu/drm/nouveau/include/nvif/object.h` | MIT | `f6456997895317181ebd6f72a486d7338801202fda900379e633f6025c83a4c2` |
| `drivers/gpu/drm/nouveau/include/nvif/os.h` | MIT | `fc4b32d441ad5d4217fc8794c785ad2bb9c082ece42d28d45f6bddcd29ccf7d6` |
| `drivers/gpu/drm/nouveau/include/nvif/outp.h` | MIT | `fab0236f185c9684d94c593dfba0b11f9e39872e487f27c73ce77a39be788aeb` |
| `drivers/gpu/drm/nouveau/include/nvif/parent.h` | MIT | `253472c8192fa227889a51039a59152f2f640af71fece7c3372c3066ab89ee0d` |
| `drivers/gpu/drm/nouveau/include/nvif/printf.h` | MIT | `a0e4a2785a48f4160d7724938f2c62a9383801ccf52fe9cba5ab71fdd9d2991e` |
| `drivers/gpu/drm/nouveau/include/nvif/push.h` | MIT (license text) | `1c65685fd70f78dc9d55cd44bfcbadb09ca662f239b11aee868e2f4a26c52fe2` |
| `drivers/gpu/drm/nouveau/include/nvif/unpack.h` | MIT | `5235c40469b74a8caa60ce8fdabe8790897bae33c0ea3a904aa8a0401050643c` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/client.h` | MIT | `8c0420f2dae982efe9c2ac69745aa2844a4fa7ad9b033d89af6dc4447d21c37d` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/debug.h` | MIT | `e5ecca403d4b92ee5706b882e74e67fd25ded5e5ee8fb6b3c6e94625b1f323ef` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/device.h` | MIT | `792adc9b1f29efe9351577d454c96d5c2cb6152b260ff76b472535e9a26a47e1` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/engine.h` | MIT | `5f15595e921bf30daaf1bdffa2463f764fde088c50db8f86fd8bb9c103ebaffc` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/enum.h` | MIT | `ae75dfd22f73b24773104cc4df954f56af0a4c05d560229e7d823626c3af5495` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/event.h` | MIT | `6af2bb3a0da94b5cb96a97000784cf86f0145297703a70aa99068360f606d27e` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/firmware.h` | MIT | `6d6c2e117dcdad0bcb1f7e94ec56b8a596a4613b975efed3f841eb497c81bac8` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/gpuobj.h` | MIT | `c0450df34a4fb831b7f1694d4cb42907953f6c769b0b7fc2e4aaa47cfaa8af57` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/intr.h` | MIT | `fb67dc932de61e526ddee606fe518b2c0f21083989700583c982c1c8f9ec9ea9` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/ioctl.h` | MIT | `af9329422a1c9ea7a87e25132ae8bd87fcb868839b954b8f72f80fdaf7470ab6` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/layout.h` | MIT | `9e590d65860d1b247e6c97a812bd38c58ddca1fa562b42acf814a5aaf62aaec7` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/memory.h` | MIT | `dbf6854812e3a1154f429b7a0d2e9ed628f9e34b4942c090d9953f760631c76c` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/mm.h` | MIT | `1d387c90046e1605fa6619b0dbb9ca61bdc7bbf839c615dce39a48138eb9a4a7` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/object.h` | MIT | `2ab1ed484e09834a56b2e78ec3a103d1b068fee3e2684e238e89cd8b17522490` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/oproxy.h` | MIT | `e98cc3168796caae307e3635a0769f5981cab444c1ba6309670c843ce619f72b` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/option.h` | MIT | `e2d31be8a4336807cb59204271355c5a2c3ffff933a85d1c66a24d843f2a2e39` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/os.h` | MIT | `188e8870e8132bbfc7591d4de6373b37d8529472ac45ac5ca120b842ed80480f` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/pci.h` | MIT | `9ded42cf0a3a43a29c7c708673ac19290e00c19da1ce797e6320188e13e9bf68` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/ramht.h` | MIT | `44c98c32ef3ff1086a70c01658286674517a064b5b80a4c262bc4beaf73ef3e1` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/subdev.h` | MIT | `848b51b239bf1731b9f7c0eea47073c01281b2364b938ed6518de24074a72773` |
| `drivers/gpu/drm/nouveau/include/nvkm/core/tegra.h` | MIT | `1814317c28f292d78da344f5b5e50f26210437168776663c3b4f2d2d8c010012` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/bsp.h` | MIT | `81b4bd06c013d0dcefd605061fc54b889672e761b9a27bff94d6675594ff147f` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/ce.h` | MIT | `deabcdc27dc46f19e392862b48ec9d63d9f034d7229e14578883f4502283780e` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/cipher.h` | MIT | `85b4047ff257692d15a7d331b2d206cd769b57fddbc50d25897de145f3ea4fb0` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/disp.h` | MIT | `82b8734ceaa4face269211d74512b3a0c9e1e66fdc3700570557c508338f1ef6` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/dma.h` | MIT | `9f5c43509e890de633ccad40e24bb9ba1d976c660709f24f41a28dfe494f78d5` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/falcon.h` | MIT | `f6bc13c54ad465439f6a52fc9960f2aa65a3a6cd24e772e1c612080cb4c1e4bd` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/fifo.h` | MIT | `87b483b4f882f7623caf45d4f9ed2988d9172c427f014f18de327e1c62c83bfc` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/gr.h` | MIT | `c2a9ddf3dcbfb128350597c963c6906f6343af02f3b5df023838d0e2b9d14728` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/mpeg.h` | MIT | `1643c0f08183cecdc3cec327846879be9066ff85ebf9db1c605282a1ae73e395` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/msenc.h` | MIT | `742e637d16d9b056e0bade8cfda8a2a0422df5a766d145c8bd87fa36bf7734af` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/mspdec.h` | MIT | `0577eb0e1f1b613d0bcdeac94c811c892673cb61b80287f15f2a1bc00f06e6d8` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/msppp.h` | MIT | `591eaa5a6146911234d20c09c643bc48928d5466db312b31c31f797c4bd82978` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/msvld.h` | MIT | `92d5fbbceef64ab44e5f59ddd48183936fe3fc0fbde83c6406d39649a5e36a72` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/nvdec.h` | MIT | `522ee336f3fd07782c34b967e3a6a452f3c5c48a693e72a44b8756384c8c96cb` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/nvenc.h` | MIT | `4404d2e947d4a74b5e59985a6ba037b1ecb792168fdb974d01ce224f9720ab37` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/nvjpg.h` | MIT | `a953c124ebc68d4ccfb3874e274df2c3b297e7a2f064ec9f566bc5c88aeb0404` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/ofa.h` | MIT | `c12750966b9f65bbca2ed362c4d97ad11cb9f121b6a719e3d544eb277c24f3b5` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/sec.h` | MIT | `82fe3cecbad7d6b1ab7c75add58ccb00995caf8d80302461548b0a4dfefdb715` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/sec2.h` | MIT | `fd5263188692a6b49a5f4b58efae02f38c17d3aa8586eaaff960f0a3064f3cb9` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/sw.h` | MIT | `c624a59268c45839d51c9f09d2bcf1cd1efb9ddcf88d944bb404d290c281bd1a` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/vic.h` | MIT | `93da890071790aafa7b7bdca0b8cebb83ee2295db58c4c57b8f7f987d8814224` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/vp.h` | MIT | `761260a661eab5dc9e578d3044c18030bc0e59f59c606a0c0af6b7c4cb069f65` |
| `drivers/gpu/drm/nouveau/include/nvkm/engine/xtensa.h` | MIT | `41cd0b9072c5e6051716226915a1037640538e189edb76cc138aab78e11fb80d` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/acr.h` | MIT | `d14c80e4de3cae0ef3be2b9b233a85149f79d8b9b5b2b0687d45c1aaae637ad0` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bar.h` | MIT | `6784ad3dc1137866a042fc376b935fd30e26bfd47d3d8aabfeb691d11434b4ac` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios.h` | MIT | `feb03bc21ff16331d83ae06a86cd811d473ffeda38994af598f7f19223174680` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/M0203.h` | MIT | `23f32ce2578f0cd2802518fa69cd244c22cd48da8638bfc94af490bdf7a7e208` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/M0205.h` | MIT | `6331526f7d17984851df0328ad5b121d245277b3af8f3b86cb38f2947718be9f` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/M0209.h` | MIT | `1fdec20459cc38abe9ad39905f1670262e0a8ecd354506a12a8b8e76fb5e82cc` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/P0260.h` | MIT | `949c5a57fd5e6e4bdf9affd1842dceef3af10d7ad36da8f628eaeb8cb134b636` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/bit.h` | MIT | `299f0fceadd0d6a477986bb09d86b9a1e623499b93fd1a3cff278d070044a953` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/bmp.h` | MIT | `a1868cd927a18af0511eb7a5b71170c137738e86891b21db87628c43b5391d75` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/boost.h` | MIT | `31e859a4ec2323f5ef445832b7453e2e991a04e561f1a0bde28869bbee62b8cc` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/conn.h` | MIT | `048bffd8fcd8054eface62687e1c7a7ba5c5f00d943957d898640d303e03aab4` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/cstep.h` | MIT | `7f24ffda62be483d5709df9eb4ac81a6dd325f3a87297c4a55a6bad8306504e5` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/dcb.h` | MIT | `21f79209fdc57aa82367ca9f8b17fb0a3deaa3b10d2baf9f6ed80824d0767c1e` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/disp.h` | MIT | `3c63bd8cdd3c53af353a2726e442c8ae95725185a15a53054dc2c3722b9e4bf0` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/dp.h` | MIT | `7f4d2dd161fb326abb6e9d0f1b4947339b9d457a7aac3d5b3bbdb863dbdc978b` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/extdev.h` | MIT | `80ef3dc056f8acb2e2a686b3e03c205b8fc9c6c4982bf604c47ed0a82efe5b2f` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/fan.h` | MIT | `76a2ea8739d3cb789e2dcaf3b848bf4fa77ccc1b21b4c45605ab19059dccbf3d` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/gpio.h` | MIT | `cde7fc5a0b4b3469bd9a0d7967e6dc1cf3b236f96f53d917a53f23f520c8e744` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/i2c.h` | MIT | `350ed8dacb7b2a182b1f4166c54a7687153002255a0ec55b87f7c01343856ebd` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/iccsense.h` | MIT | `583aa21fc17e71e86f247cf4ac8fb91441c5b5d63156554af651411443827373` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/image.h` | MIT | `45a6762602990f6ea2e2dcf5971af9d8c510186243f00e32f76606e08621bfb4` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/init.h` | MIT | `129a035f991c6a11b8c43f31933ffcbbf2f64e34b1466980115bb19a9fd57a61` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/mxm.h` | MIT | `459b21b915d8ea32eaf1ebbe759e5b9561e3c5e352851fa01bbd4285c3c37518` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/npde.h` | MIT | `1b2de1fda0cc2dd807ff5bd40da3132b3ab8ac44d5dbf884408b6bf2c5217502` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/pcir.h` | MIT | `e6a3201504212417800440b9722f526904d321ea780d5784b09b06003702f80f` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/perf.h` | MIT | `20f2389cbd06bada6c48bde2622f7832b7816b2514f352a4e561241e2c46376b` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/pll.h` | MIT | `225cb0b77f61ef603ba5c84b181d23c33ad7494ed79ae3221c140a0b940e1f09` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/pmu.h` | MIT | `5dd55eee7ce647af50073cf688bd33bfa48b4371e787af322a8ba26bd3850b4a` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/power_budget.h` | MIT | `c215a6cae179d0297cc21eb78d1870ebea625fa0f688b0f7ba1a43a710c875d8` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/ramcfg.h` | MIT | `a2cf64ee538b1dfc92c5ce785bcf34c456029a978da02a6e762546f93204dc24` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/rammap.h` | MIT | `7e890a86cc275785fd56224e20c059ce3e5af8dbdc402b36358f05a47b84f8ef` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/therm.h` | MIT | `7be73c31b30dc10dbdf570a12e9fe29dca854b1d5f0392eb041c9b38d649251b` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/timing.h` | MIT | `dbbdf5c28560f5576b7037033243148c8794576710d629b29e8ccb4ebd92226e` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/vmap.h` | MIT | `b5f7defc1ba000f17ad2f86bc7cbedbc61b47378432ccb9c6ae77cd920884ebe` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/volt.h` | MIT | `2af9fff6d5686af3144a2a322bbf925dc504b831ac0b15ed0d9f884303a3269e` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/vpstate.h` | MIT | `b224dda03152f568c01e30249888b99ce023048b564cfd0c377de55f0aac35cb` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bios/xpio.h` | MIT | `22f077b9485de890124e0d1c18f375795ea17665cab1dba94bc294699b649214` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/bus.h` | MIT | `cc3faad0f5ed45d13c4b3f4082fdbc41bc93291844a3519ef8fe55da792704ee` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/clk.h` | MIT | `62fe0c604ab0ed880fcf928dc489972158f47064af3c51554849a59efa03b97b` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/devinit.h` | MIT | `56495fea0bf6ac5291293a771a1b88d438ba160f66b7cca47c2f8371e76a8cc9` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/fb.h` | MIT | `93b4c71848e4fb4929de1dc7d916726749ef284a7ec58b313befe7d22b20010b` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/fuse.h` | MIT | `767bb42fc67f1a6e6d5f780e9ae834828f707bd352cfa23e2216b845d485c58c` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/gpio.h` | MIT | `13529839289f71f2cdd9964604d3317e5b5dde8be8d7142dff09418fcb83d94d` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/i2c.h` | MIT | `371ef5bfcd35da3c6b7d8b14b8e218bf42e63c5731199f665a1412a38fbe7737` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/iccsense.h` | MIT | `571b126f7a5fb3bb9a32b2aeb6c04d5625507dbd106bac071ce50206c3f6db74` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/instmem.h` | MIT | `b0cb58a2b3640289d45152f64455b89fa10897c35b468988404e5f3f47b66af7` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/ltc.h` | MIT | `baf12f23f3c2b897b936e38031469811703e86ccf78cd91c82eb5c0ede7fa122` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/mc.h` | MIT | `8e77c0903f461660e79b879d278c3ea8884c0f2eac602fb6e1b40208e89efe38` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/mmu.h` | MIT | `f989645cc92aa1cea905b2c3dbfcf27bd6f50c626929d8cddcea120ca6846663` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/mxm.h` | MIT | `746876329bacf51efad027fa5ee389bb2d5578561e1175660428faa7658471b6` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/pci.h` | MIT | `046c9dc1562403c82a9f5502977d6a74b533848f7dcf9600a5d39d861c3c289c` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/pmu.h` | MIT | `065cda93bb40e80bdb0a2ecb64f7783a62b4a8479bf700758042fc2004524f10` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/privring.h` | MIT | `7ccc2bea3b4c6d34f65b9d3476df8f1d54ab91838b1989e849838715498f2d7d` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/therm.h` | MIT | `d2640a336365d0420588ed74378bf5a05546b0096e4b36416ff98d838173bcd8` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/timer.h` | MIT | `e796bba81e12250df51377f50e79da61a97df8d91a2ddaa4e6847db1093008fd` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/top.h` | MIT | `9b2edb63c132ecf91dc778fed8ae4ab6f608ce1453ff07556c75ecf895a06b52` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/vfn.h` | MIT | `b1fa02f6c4cc3d3eadfc7debd7d37f1bcf6902e526ea1a85e0d2540c5cb18cda` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/vga.h` | MIT | `2dc5b1e522bc4c6158cbcf32f039e27a8b28920199345149a7babbf4338a956b` |
| `drivers/gpu/drm/nouveau/include/nvkm/subdev/volt.h` | MIT | `285f3c2798d64134ab53b3bb2ff9bd34d7be731ed618e7561e8b67ac889fa59c` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/alloc/alloc_channel.h` | MIT | `95425076a80e5f4a6dc499598521b0b1e645407d24ebb36a0037c9a8b0f39530` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/class/cl0000.h` | MIT | `a8c3b7f968c74544ef694b6be714cbeb63486cbfd945264ba58fde1c719ddc61` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/class/cl0005.h` | MIT | `a0f4c985fc0e2932f6794b9af6180694de5e82163fde38c8a34312b1f483f84e` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/class/cl0080.h` | MIT | `55c5a793c906e652942b6f252b5743e354cdd5cfeffa63b805e111f9235f873b` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/class/cl2080.h` | MIT | `298735c08a6cbccf0a8318f35d20b7ed73230f5a73918b89b14107cb658abaf6` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/class/cl2080_notification.h` | MIT | `032979ea379c279564abd6e208e9f66a80f142321362d71e8018b5fbf41a1bb9` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/class/cl84a0.h` | MIT | `5f77d962b426763e7d670efd632e2def50e2fb10d67380d6486540f679f5f13e` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/class/cl90f1.h` | MIT | `ace43e2a6a38cd425614299bf05c44a40346f6195d0c2f937ff916f5f96a01c2` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/class/clc0b5sw.h` | MIT | `f3d85a0caed22eaca155baa232bfa50e13c1fc0bd728f1d506a0cec0b023302b` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl0073/ctrl0073common.h` | MIT | `7a65aa68dcdd75c4428324a9e9f9b679a97dd4946cc6ae2e33c3000a3da193a6` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl0073/ctrl0073dfp.h` | MIT | `308d2d8ac04feefb52608adf76d8ce1556f0ce9e35d67b36fd77e8af002f6352` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl0073/ctrl0073dp.h` | MIT | `908b24703f088b3c2eca2ef71f4ac1fa37c1ef4c69bb99aba99d5366e87c7c45` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl0073/ctrl0073specific.h` | MIT | `d994e4a40826f5e8764624bc171fd05453d8adfdd438fb0d78308e186d8ffc4b` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl0073/ctrl0073system.h` | MIT | `f51fb21cabe78b7af95c98dae91ba04bcb99a559d5caaf6703ec1ab8146dc966` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl0080/ctrl0080fifo.h` | MIT | `2801f093f6ce93f9bd95846ec25c78c849a3185f60b0f869b5002aa98d9b4927` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl0080/ctrl0080gpu.h` | MIT | `ddcb9ba3a12b576b791173285a319cef3f9057e040f0216b75f2ea18cf99b1ac` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl0080/ctrl0080gr.h` | MIT | `0c12a23f4ed03798c6c92beffb5da99b157a62515c6d5521ab5ace0e95f3be8d` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl2080/ctrl2080bios.h` | MIT | `080a4858408a11ad291814ea088a7ceb04e2bbea0124eb0f4bee0535c38ab43e` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl2080/ctrl2080ce.h` | MIT | `5cd818e9c080f7f3c57a3742d74110443b8dd28f4f13a622d3467aac1f8e87d7` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl2080/ctrl2080event.h` | MIT | `efc7f317a62a8935af69cdbf783aa6456fcdf302356009bd38fc9f00bfbf0267` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl2080/ctrl2080fb.h` | MIT | `508cd23d318606a998ff6f52f90ca96be8a4a5eec0b5974cdef4ad929554ec1e` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl2080/ctrl2080fifo.h` | MIT | `4b8f21059f3d03429bee07b327ae0216ef3f4d87105b6aab51313272718687f9` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl2080/ctrl2080gpu.h` | MIT | `5229bd1e27f06bedc293097c19626b9e3e9ddea829a3576140ce4a034c47bb8b` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl2080/ctrl2080gr.h` | MIT | `296fd2ccab9ebfa85ccef0ad87e6d5cee96e033d0cd4c0c3fb81a474bdb98654` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl2080/ctrl2080internal.h` | MIT | `fbb28d2f414b0cd1f77e06d8b1d701d0f9b21268466ed4ef4369092cb6f0e6a1` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrl90f1.h` | MIT | `b06baf39336114e01c2372b365f2b55944f4478c37f2717fae0973b614710dc4` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/ctrl/ctrla06f/ctrla06fgpfifo.h` | MIT | `ed6e5ec434304f000f616ca3f8fb407c8d4a7abf873b574bb50568f0cbd0f3c6` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/nvlimits.h` | MIT | `8a70e48baff7b96a985b34dcace70635962d9c4b81081020777ccfdae730844d` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/sdk/nvidia/inc/nvos.h` | MIT | `b72da1112f866bfdef179c4160861ffd8de25bf3a61e5ee3c2c15c93ffa2f24c` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/shared/msgq/inc/msgq/msgq_priv.h` | MIT | `ec08d2069e7648d5707175ece13da03baf442ef32fc5e38d0575b74f96baf970` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/common/uproc/os/common/include/libos_init_args.h` | MIT | `65dd1b0360d842cbb17807f9f9a7b1fac7e101d6f13a1a42e07987bb9ca3582a` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/arch/nvalloc/common/inc/gsp/gsp_fw_sr_meta.h` | MIT | `f9a8e96235354f5d64f9fba1044273765ec8d8c2bed6d1ef2fd59716c61d90f2` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/arch/nvalloc/common/inc/gsp/gsp_fw_wpr_meta.h` | MIT | `3dfed687d7dd09c87538921e67ed8d283b76551d1c78926580b32d36a9ea1b01` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/arch/nvalloc/common/inc/rmRiscvUcode.h` | MIT | `d820c3fdc72e2fa65b797304cd7a376dce385b7bfe59c02ba34780e5192aadd6` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/arch/nvalloc/common/inc/rmgspseq.h` | MIT | `1393679aa69a6034905ded494294a3fb6c7d9913eefacb892b32ceb87809bd0f` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/generated/g_allclasses.h` | MIT | `d5798f883d129e7ff988ce1773bcd58b17b0bacb693d9e6269d23f922490802b` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/generated/g_chipset_nvoc.h` | MIT | `48985f4344dcdaee1cc842cb65c1d9f027b6d3cbbbb37f8532fe6a1209864b71` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/generated/g_fbsr_nvoc.h` | MIT | `fffded4c67666e5ba5b38c37de12b81e051025ff51f2846154b8bc3d43309ae7` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/generated/g_gpu_nvoc.h` | MIT | `bb5b14f52846509046848c61ab8397d544574b1bc7c2014ff750244542666fcb` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/generated/g_kernel_channel_nvoc.h` | MIT | `e928a86c8956532627111482f8658d8e1a828b953ebd24ee3ebbea291a1b9434` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/generated/g_kernel_fifo_nvoc.h` | MIT | `678da5976d684bf550dfc874e590887af373157e501e87f5180ff4d745e2b11a` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/generated/g_mem_desc_nvoc.h` | MIT | `ec64bc529087c9f1e966eb74cdcba0eff2601c2c44364614caca6f966fa70bcf` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/generated/g_os_nvoc.h` | MIT | `b1d92a35d569df0aaf346fab9a73077dd87427f6b275e321ee099b1ce705c8ec` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/generated/g_rpc-structures.h` | MIT | `14c81295cabdfc733e079d81b6a1e1c0343d3fb31767c0af268ddc04df8655ea` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/generated/g_sdk-structures.h` | MIT | `3a4af88b6c833b72aacdda3d3d0a3ede93a2089776f30b385e880a883f453383` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/inc/kernel/gpu/gpu_acpi_data.h` | MIT | `473e867fe98cbbb87a6897d4d31acfa1fba09a7d99c4d475b1d98d07c9c14a74` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/inc/kernel/gpu/gpu_engine_type.h` | MIT | `a61becf53ce0c369f17fda5b102d547825a6554bc710ca964893ef6458cc9443` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/inc/kernel/gpu/gsp/gsp_fw_heap.h` | MIT | `832d83e0c89602956fa46d952bff5748c585758b5cf0146b0b260d5314df6ea2` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/inc/kernel/gpu/gsp/gsp_init_args.h` | MIT | `a344b767e349666e3ef099b9e298b9f93dcdcb3cfb4003115d96469854a85f5d` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/inc/kernel/gpu/gsp/gsp_static_config.h` | MIT | `386937ee6235d0db0daa7ca381f0b1d240e53ae45e261cad4c7d5f0cdce3928e` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/inc/kernel/gpu/intr/engine_idx.h` | MIT | `f0bd1e67c478d3c0965dec1b263365c0fcc3713e453c26aa5e78a3540c199903` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/inc/kernel/gpu/nvbitmask.h` | MIT | `aff06d77f333c4883fc27da8d1cb704e72b741b5c4ae82a21d9de641f88fb9aa` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/inc/kernel/os/nv_memory_type.h` | MIT | `97449dd717dc9723233028e51c947b1620570be2c9f815dc7af93c39a98aba8a` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/kernel/inc/vgpu/rpc_global_enums.h` | MIT (NVIDIA open-gpu-kernel-modules 535.113.01, see below) | `0c6f4be1a8c8963db2c92ea3664819df61965c9879df63609af311df025b33aa` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/kernel/inc/vgpu/rpc_headers.h` | MIT | `aec71ca9ec91bd17fd4bbd950fe8a8c749eee96501959e8fe9f0a21c5e7ee366` |
| `drivers/gpu/drm/nouveau/include/nvrm/535.113.01/nvidia/kernel/inc/vgpu/sdk-structures.h` | MIT | `9fcaf6548683a04d3729e312a14ac22d2cf1973c0fa04c093acc3c637bf962e4` |
| `drivers/gpu/drm/nouveau/include/nvrm/nvtypes.h` | MIT | `10648d22499c9d3f508fb4b56ff159bb7bff44ba454cd666a9d3a067bcb09a98` |
| `drivers/gpu/drm/nouveau/nouveau_abi16.c` | MIT (license text) | `a936db5a7a979312fba3f779e23bbe4cd1fe3262cf4bd64b3b3ca2e65e778228` |
| `drivers/gpu/drm/nouveau/nouveau_abi16.h` | MIT | `9bf57132af4ddf328d95bef7d9c8d85d971e149654bac83083e984d221d07649` |
| `drivers/gpu/drm/nouveau/nouveau_acpi.h` | MIT | `9c701939a8ecce8904d6212d76d0f4fc8e249b2af463e654e66c533b7ecdafa4` |
| `drivers/gpu/drm/nouveau/nouveau_bios.c` | MIT (license text) | `a963f45d45c3fa174055481ee64e4164b3b7cab47973cacbe594271345b8c805` |
| `drivers/gpu/drm/nouveau/nouveau_bios.h` | MIT (license text) | `4acdb6ee4723ab9e3fb8b3c121668d7e9fff798e9b828b7795f0ecd7bf6e75e4` |
| `drivers/gpu/drm/nouveau/nouveau_bo.c` | MIT (license text) | `024d32cd95f58cd227e074e156d1c82066f578d74b63e1de8b71ad61cd6184e6` |
| `drivers/gpu/drm/nouveau/nouveau_bo.h` | MIT | `6b877657da352e535a4dceb3a5e99b49834fcf997cc9848bdc62b3eeab871ea2` |
| `drivers/gpu/drm/nouveau/nouveau_bo0039.c` | MIT (license text) | `01e2595cbf6006c794c829f1aea8539227b022345a9f7dd33729e6163012585a` |
| `drivers/gpu/drm/nouveau/nouveau_bo5039.c` | MIT (license text) | `cefeab15d66ffd55452568bbbc5646d6f68d622c30cfd7c886784d1e3c791fcb` |
| `drivers/gpu/drm/nouveau/nouveau_bo74c1.c` | MIT (license text) | `534191826727b132bc5b23543f99c4cb1f959ac39d5b3acd7fca2a46064f630a` |
| `drivers/gpu/drm/nouveau/nouveau_bo85b5.c` | MIT (license text) | `4c4d5e4eea495659365fc3ab1961ded7d2b34219f5d5949bdbd2975b017cc509` |
| `drivers/gpu/drm/nouveau/nouveau_bo9039.c` | MIT (license text) | `6fd9fcf725409a089d0e30a23ab18be3ef2fd3d60dabec2107228c941234e086` |
| `drivers/gpu/drm/nouveau/nouveau_bo90b5.c` | MIT (license text) | `1aa218afce8b7c684367ea95125ac50a31a4b78a089ec28da066091d69936aee` |
| `drivers/gpu/drm/nouveau/nouveau_boa0b5.c` | MIT (license text) | `c47d569e47c848f190131c4487bfea451c0582a3f43443069920cc577db60500` |
| `drivers/gpu/drm/nouveau/nouveau_chan.c` | MIT (license text) | `068d2f6d47845951b6538c21cfbd8a3e34748575f4ae7bef61d02021ece7cd80` |
| `drivers/gpu/drm/nouveau/nouveau_chan.h` | MIT | `6990f70d3f35c14af417cd986c6fd4439d2e80ac88e4ffa0bc7d8f4aa3ad3566` |
| `drivers/gpu/drm/nouveau/nouveau_connector.c` | MIT (license text) | `4a188e789a83312cbd5bb8206edf58bb8d32f2d1eba2be25a7a7dab3290bdbcb` |
| `drivers/gpu/drm/nouveau/nouveau_connector.h` | MIT (license text) | `21bd19ed2b98dc05e121cb23af0653b908cd2cce3cef3cad58fd55f371cbbf6a` |
| `drivers/gpu/drm/nouveau/nouveau_crtc.h` | MIT (license text) | `03f2daac9d065adda54a708e5cee21fbbbcdbd70a294249243c113427df1097f` |
| `drivers/gpu/drm/nouveau/nouveau_debugfs.h` | MIT | `a81a5cda066cc2fe14d2f79b8a24d76acf75e38e111d994c6ddb1f6c0280835f` |
| `drivers/gpu/drm/nouveau/nouveau_display.c` | MIT (license text) | `832934f56581e31eebfd31837cfc0de8d57bb9fb469ae26cba171597aad90833` |
| `drivers/gpu/drm/nouveau/nouveau_display.h` | MIT | `d6a20e9427adbbdd7294d5dd470dffe89fbf216aada3c8a1a9ee110e9c1277ba` |
| `drivers/gpu/drm/nouveau/nouveau_dma.c` | MIT (license text) | `2bf585c916cde08ef2b99370c8d63e5b19d49b716c8591a476382fe17f2c7d11` |
| `drivers/gpu/drm/nouveau/nouveau_dma.h` | MIT (license text) | `4669de6f7b60c3479a027185e4f150ad2ff2464535a76ac273057ce084510a8f` |
| `drivers/gpu/drm/nouveau/nouveau_dmem.h` | MIT (license text) | `2a4ee262540fa39821bda8b3c3aa323d6d3a952b85fbd857a1b5b5f271d986e7` |
| `drivers/gpu/drm/nouveau/nouveau_dp.c` | MIT (license text) | `adcd545b64d4d9f827dd82d027eaa8c1998b0aa089c34211542edde8288c17ac` |
| `drivers/gpu/drm/nouveau/nouveau_drm.c` | MIT (license text) | `8238dab54f6d82bd745262a0cb72ef0f3f56da5ffe938466bd39dde2ca6f6990` |
| `drivers/gpu/drm/nouveau/nouveau_drv.h` | MIT | `6b8eacd78f95326666b34f5ff7cc9c9386a61a07d2d1b0bc7d0fe33f5f9ff96a` |
| `drivers/gpu/drm/nouveau/nouveau_encoder.h` | MIT (license text) | `182b05b7183818f5e1255f7ebdb4a9d853a7e527ee923e950df2933327cd0d84` |
| `drivers/gpu/drm/nouveau/nouveau_exec.c` | MIT | `cc81c23cf3d3ebcafd9f377c0eab4456e46b9c68ad2a6cfda9ac10d7dbc13b51` |
| `drivers/gpu/drm/nouveau/nouveau_exec.h` | MIT | `c3e7ba5423d8d49f40331e1529d9750ed0f24d20c1ddef950631f5bf39de2115` |
| `drivers/gpu/drm/nouveau/nouveau_fence.c` | MIT (license text) | `9c301d4a6d8f1d56feed5e1500d0ca764ad668bd6c340b88ef42e221c1bcaee5` |
| `drivers/gpu/drm/nouveau/nouveau_fence.h` | MIT | `9f221813e03f2cc25e0f0bb8a73555e395f9488e362dd90815e27e056db4042c` |
| `drivers/gpu/drm/nouveau/nouveau_gem.c` | MIT (license text) | `4358cfc9b57cd7d1ac1b96e19dc45de86dd446f2eb632aef5efdc0f4b533a704` |
| `drivers/gpu/drm/nouveau/nouveau_gem.h` | MIT | `603fbe90587e8d5baeb927dc3be76b618ddc64bbca1445966a15184c4c1209e2` |
| `drivers/gpu/drm/nouveau/nouveau_hwmon.c` | MIT (license text) | `b814422960408e5ec5a626c1c2ea83121360663e13922b23ea9450a18c753ee4` |
| `drivers/gpu/drm/nouveau/nouveau_hwmon.h` | MIT (license text) | `fd2f2efcd7cfbd5aaa05c3ff9dc919e27cbbb2c7f690cf320c49fc0bcce14bb7` |
| `drivers/gpu/drm/nouveau/nouveau_ioctl.h` | MIT | `fa1e4196af59801d062cd9c5c9775dcd9395079713b028d9ab61c3f2b0bc5fb7` |
| `drivers/gpu/drm/nouveau/nouveau_led.h` | MIT (license text) | `8da0827741b921d5a44ec0044e14431e72ddb848ede849693530651c99845a5f` |
| `drivers/gpu/drm/nouveau/nouveau_mem.c` | MIT (license text) | `b5f2914dcaa4db939e196a3472b6480c5fa301a59f60e703e20591efd8eb4654` |
| `drivers/gpu/drm/nouveau/nouveau_nvif.c` | MIT (license text) | `47c06ea701147dd611e16d9bada80e1495878fe43c3f3abff68ead0c4c812d80` |
| `drivers/gpu/drm/nouveau/nouveau_platform.h` | MIT (license text) | `a2de6e2ed1e602d2e61f01b5ce4ed46a9af7442b7f6413d7c22b049e142caa29` |
| `drivers/gpu/drm/nouveau/nouveau_prime.c` | MIT (license text) | `91d3cffa1c95729209aa13c799e456b753b592f85288ae251bad5dcacdf453b8` |
| `drivers/gpu/drm/nouveau/nouveau_reg.h` | MIT | `626ee17aee59c24ad59df19a88bc24d7fa13f7895ec51bbbca37b0b7acacd933` |
| `drivers/gpu/drm/nouveau/nouveau_sched.c` | MIT | `4539d9d8930cd75f109c4f7f91fbb48258ecdf57774fa1564dea7e303fd1a8ce` |
| `drivers/gpu/drm/nouveau/nouveau_sched.h` | MIT | `a86dac854657d04df453503cc02e99beff641aaeeb73587622639386c4263440` |
| `drivers/gpu/drm/nouveau/nouveau_sgdma.c` | MIT | `63141e645747be2501af56aca278bcbb00d503e7dd112e4fcead85180bb323a1` |
| `drivers/gpu/drm/nouveau/nouveau_ttm.c` | GPL-2.0 OR MIT | `42987b06601b36e736fba3804c528583ca1f1a79936ad76da691e5ebb9c729a5` |
| `drivers/gpu/drm/nouveau/nouveau_ttm.h` | MIT | `f61192e44f8595cdee08da23dc5de1a27ffc0270db3e5e6cddfbcab6c7aa1480` |
| `drivers/gpu/drm/nouveau/nouveau_uvmm.c` | MIT | `2f6f2c8d8404cebabe035780e94947f7a73fe3072d5d3df8d6fa2266498bab48` |
| `drivers/gpu/drm/nouveau/nouveau_uvmm.h` | MIT | `b0303bf42ef1857b115cbaf866162e6a8a9b5b25459d0b2cf9fa9da384039ce3` |
| `drivers/gpu/drm/nouveau/nouveau_vga.c` | MIT | `0550c22ba219065350469438aab53211f280a229c90840a357843e29148be06b` |
| `drivers/gpu/drm/nouveau/nouveau_vga.h` | MIT | `412a4db0cb56259adfc79962ad4f580162154e1930b6f067596b6cb8ab6e2ce2` |
| `drivers/gpu/drm/nouveau/nouveau_vmm.c` | MIT (license text) | `a8c0ec5f62464ec3dd8a100cf69f67c9b6f7d9ccce5ba2681db75efda030b9ba` |
| `drivers/gpu/drm/nouveau/nv04_fence.c` | MIT (license text) | `6810765eca878463b22fc002703c70de348a12025b301c24338706c813ad34ae` |
| `drivers/gpu/drm/nouveau/nv10_fence.c` | MIT (license text) | `2312a2ceeaba2a9cc3f073571da2c62782d7ea0132f9d629e06d6753073562db` |
| `drivers/gpu/drm/nouveau/nv10_fence.h` | MIT | `7e34b4ea8ef4f6a0e24ce5b723b1ebe0ac3f1be3b36c881a78dc3bdf4c1f8ffd` |
| `drivers/gpu/drm/nouveau/nv17_fence.c` | MIT (license text) | `f01bf7f09fadac41300d0f9a39783d923e86d029fc5476f2eb53f64026cea5a7` |
| `drivers/gpu/drm/nouveau/nv50_display.h` | MIT (license text) | `f0a508d31569a25d812ff6d4ec4c58d4d5e1e776335a67ae30ca190a889879c6` |
| `drivers/gpu/drm/nouveau/nv50_fence.c` | MIT (license text) | `f3638cc2d6a9469a77fba844af331339116bfd70149ebce41c84ab6be3dd1361` |
| `drivers/gpu/drm/nouveau/nv84_fence.c` | MIT (license text) | `8810743e079a0f15cea9b2b5eec02c468e8d13c30b2997b4082593fbee819b4f` |
| `drivers/gpu/drm/nouveau/nvc0_fence.c` | MIT (license text) | `067978f8c29dd9eaa12d8c3f258b657e4f5c789a8a61c4fefa126430b8be0edd` |
| `drivers/gpu/drm/nouveau/nvif/client.c` | MIT (license text) | `0f132ae3396497d81bdc3bf85db7054229822fbbbe18ef254b9f70f13d15d2d2` |
| `drivers/gpu/drm/nouveau/nvif/conn.c` | MIT (license text) | `dd5d40ec9f960ecc7ec19e084c49271ddcf1f1deeffc375535455b9e546f553c` |
| `drivers/gpu/drm/nouveau/nvif/device.c` | MIT (license text) | `44b50bafe876c97a03c06a591dfca1deb216f281e1273c05f74f06d29332ebe4` |
| `drivers/gpu/drm/nouveau/nvif/disp.c` | MIT (license text) | `b79d0f8416edafb521e4d6b9d0863ab2b33cd845102753f61e3f4ad41402cc85` |
| `drivers/gpu/drm/nouveau/nvif/driver.c` | MIT (license text) | `ad066b36807700f6355246a3cc2815719e5b52e354a6ce1eda8dfb67d7b9545c` |
| `drivers/gpu/drm/nouveau/nvif/event.c` | MIT (license text) | `5efeac6f779dfafeef39870a6c96096e2ee595651b963598e32c33695b54330c` |
| `drivers/gpu/drm/nouveau/nvif/fifo.c` | MIT (license text) | `bdffac228ad274081553d3a6902feccccd66d77cb390ddcede3f299de99e589b` |
| `drivers/gpu/drm/nouveau/nvif/head.c` | MIT (license text) | `41d470e7b97376336b001754060e9169522423dd16b5de85856650659b4b2f69` |
| `drivers/gpu/drm/nouveau/nvif/mem.c` | MIT (license text) | `71ea38d38f3430f55c6bcce6b534a68d84380b30b06d1ed51928619a0536fcad` |
| `drivers/gpu/drm/nouveau/nvif/mmu.c` | MIT (license text) | `6c4626689e398073b4b9056a2ed5abc575e6fa79f89745174e280dbb81ce9b07` |
| `drivers/gpu/drm/nouveau/nvif/object.c` | MIT (license text) | `2574b0fa8f2d06faa66e3c19f7ae0433e00f3c08c66969a893da56f4ac83d32b` |
| `drivers/gpu/drm/nouveau/nvif/outp.c` | MIT (license text) | `c7998b841402638e873e6cd502b3758e92b92ce022841c345383f3b572e5eee4` |
| `drivers/gpu/drm/nouveau/nvif/timer.c` | MIT (license text) | `bd6ab4c481c88b8b9c48ac026774de2d5db13fec36c6211626b3e28a11a2c61e` |
| `drivers/gpu/drm/nouveau/nvif/user.c` | MIT (license text) | `7d80349e4e4bfe260a3507503a79422865bebc8c4377528a19868829c27a46b4` |
| `drivers/gpu/drm/nouveau/nvif/userc361.c` | MIT (license text) | `50acac4e13f9f9a1516977c3b5b67e5ea4937104011bf99c81382635ad287d3e` |
| `drivers/gpu/drm/nouveau/nvif/vmm.c` | MIT (license text) | `db8d393a3e72a342c67a524d9d9872656fc9e5159723ae465395ff62bb126db9` |
| `drivers/gpu/drm/nouveau/nvkm/core/client.c` | MIT (license text) | `7631b25858fae807e040b85395f54ee919503d87af90243d5910862483481fb6` |
| `drivers/gpu/drm/nouveau/nvkm/core/engine.c` | MIT (license text) | `f214fe8455ce9969bc7a79a749a0361a8c1740e340c62a20713623880ea53e15` |
| `drivers/gpu/drm/nouveau/nvkm/core/enum.c` | MIT (license text) | `001542c1e8651cced36351d73af2d1b66c49e32590cd36f8fc7ff43373a07937` |
| `drivers/gpu/drm/nouveau/nvkm/core/event.c` | MIT (license text) | `8d81cd673058be1d221bd15aea87d9aa0c30ba7b8d4936564b8eb3707c138ec3` |
| `drivers/gpu/drm/nouveau/nvkm/core/firmware.c` | MIT (license text) | `ee0c4a1260716796bffa336655870abe643e11f31a35b8d04c74e8b7f5204bc8` |
| `drivers/gpu/drm/nouveau/nvkm/core/gpuobj.c` | MIT (license text) | `7a6107e0424bc5606e2b26e0882b2ec154516a9dc1c24703755fff297b754c0a` |
| `drivers/gpu/drm/nouveau/nvkm/core/intr.c` | MIT (license text) | `4d895a728451ca546a05e0ac6f1a6811d17b0011fe5e89ecce50bd2dd33302ef` |
| `drivers/gpu/drm/nouveau/nvkm/core/ioctl.c` | MIT (license text) | `b7485478afb16a42083ce22bac47dfceb76a834616bd2e523ff1babf94362ace` |
| `drivers/gpu/drm/nouveau/nvkm/core/memory.c` | MIT (license text) | `6b9e64de84c6d91a8eac576679dff989a4745304f61e2ebac43bfdf913cc532e` |
| `drivers/gpu/drm/nouveau/nvkm/core/mm.c` | MIT (license text) | `42f17e8c36f4a681422af57869dafa2e376a8f8a8212ef3123977b88cc287e33` |
| `drivers/gpu/drm/nouveau/nvkm/core/object.c` | MIT (license text) | `91d74a188cf9979fc9fa77492a0ab91c1aa3b2c5c30679d19b99cfe7aab54131` |
| `drivers/gpu/drm/nouveau/nvkm/core/oproxy.c` | MIT (license text) | `0b9083d7a96813e0645b10b08376070b0b30ef78cf2684349f37dbbd68c2af75` |
| `drivers/gpu/drm/nouveau/nvkm/core/option.c` | MIT (license text) | `641bf408a3287c47580b7847c9ff710a4ccaab63a205d4aed647f71e8556a1a8` |
| `drivers/gpu/drm/nouveau/nvkm/core/ramht.c` | MIT (license text) | `b8b4ed50eed3db4d34368ef519f7e21fbde833c778bfde07e5755326d13ab31b` |
| `drivers/gpu/drm/nouveau/nvkm/core/subdev.c` | MIT (license text) | `df4404934f5412e60e0f1af9c2c27d4ab23f83c73a4705689a11da7711249ec1` |
| `drivers/gpu/drm/nouveau/nvkm/core/uevent.c` | MIT (license text) | `c7b635d2c2b08689e1a698d3ce986268faf6aaa806e4ad86802ec6c7c60a447e` |
| `drivers/gpu/drm/nouveau/nvkm/engine/bsp/g84.c` | MIT (license text) | `2be07db26a341b3bf2f953b5c94562b6e2292e5a70d4579aaa5aa9b91be2b407` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/fuc/gf100.fuc3.h` | MIT | `07eea83220f46334716a034c043d2032841a574fd1248d20203c67a616c817a1` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/fuc/gt215.fuc3.h` | MIT | `d3735a1e868daacc09dcd5ba2e8caa62328532e539d458d2644cce5cde6510b1` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/ga100.c` | MIT (license text) | `c4f753884c703b90caebeee30e13ecba6fcf1b6e5b01850d580277dc15c99c61` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/ga102.c` | MIT (license text) | `a00e6a8d02c0e08b1a88900f2e057ffc289f3ef0d353606153a2320a68413eae` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/gf100.c` | MIT (license text) | `7a795d548e2f13101b899532a129e5ef992551e184f4518e36af0857198b30cf` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/gk104.c` | MIT (license text) | `a13c79278f977f79218c2be9de81f029995cbb208153b6f6c5b45e51e3f4a4dd` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/gm107.c` | MIT (license text) | `ff1999a48575861b38b68a08c52c1f0cae6a23e7528911eb495c67ffe3192fe8` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/gm200.c` | MIT (license text) | `197f623c059c38559d8ea8438996a9ab400a0949db8c05a3de1bdcda3f285d1c` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/gp100.c` | MIT (license text) | `26f99df5f53ce89eb597f1878af20cfa4880bb29323fbc9fad8c05e51114745a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/gp102.c` | MIT (license text) | `8d8875cc039b9f71b1f6e2f2e9adad7db178c587cb9b3cf91cff408ae04cce00` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/gt215.c` | MIT (license text) | `ec591a6519ab4b453d325996b2a72598d0ef7679f94b010ae0962b88e3c3896a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/gv100.c` | MIT (license text) | `381201b2076bba6fd616e9250dcb7713114532924fec8c96f3623c910c1fbe8a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/priv.h` | MIT | `c930f85bf50ec546853d410d5af762e43fc6b41fcfc9ef7eab0e89da76622fe8` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/r535.c` | MIT (license text) | `30efb2f7986941620aa73c7afae8223895b889af3a9e385a721759fa826d6694` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ce/tu102.c` | MIT (license text) | `12fe4332ec04a2d1e8163d2ba00c233baf979afbe1b3a0a02efb074b21345f46` |
| `drivers/gpu/drm/nouveau/nvkm/engine/cipher/g84.c` | MIT (license text) | `3cc276b5b1acf8fe2e501d915019c0804439f0068b90ee6b72ed9f5d7937ba14` |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/acpi.c` | MIT (license text) | `8e8f752d93c8c91eef765e077f1ac56bc8e71d82672820a943f465f359169201` |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/acpi.h` | MIT | `db60b408e2d7dbde821d1d367243790e90914bdecde22ec0342f5b42123e4920` |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/base.c` | MIT (license text) | `ac7eb1bbffb41b43b336cc59eaca86b7317632a0b353805b4359af78b70ef7ab` |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/ctrl.c` | MIT (license text) | `e0ff9e85bd2840f5bf60327a52dfd14e621b1cd9acc196876b9a455731a30fad` |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/ctrl.h` | MIT | `ddcbe1bee2c95414786adec7e312d3f9894bb2fd6196e9c187b6bdbdb8d239c0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/pci.c` | MIT (license text) | `9249cbc98b9c6dd446993e74b9072d1cef206a9d3673970e6f509b6251e96a9c` |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/priv.h` | MIT | `bcb95ec3d10ab0773d97e19f16a6dd407d2e507c4fa20e141d4453e1b84418b8` |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/tegra.c` | MIT (license text) | `f1a4d59668aefcff84854cc808b9a86dc2d13bf7c8844461212ecb638af56d92` |
| `drivers/gpu/drm/nouveau/nvkm/engine/device/user.c` | MIT (license text) | `886a4956d24ba28480a15db6e4c9841a1dbc74dfe10278e001d882bd1cdc53b7` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/ad102.c` | MIT (license text) | `ebb05d3b8eab0f7e1c6e2d20b138eed635a0414676c01cff51adc5d6903a9942` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/base.c` | MIT (license text) | `4f91304b01e6a398be6f13af47b8d10137c6c7a2500508980dbf8c27be54d4d2` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/chan.c` | MIT (license text) | `b02fa76ca72e90ede7917b1ebdf46e550fc79ddcb6e92f12325b3f00c112bf63` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/chan.h` | MIT | `07e9521afd4c5438f67972d1102af22449e966b6ecc70b2c3b92b0ddcf8513bb` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/conn.c` | MIT (license text) | `f1089b03af0916029b28852d074823c14b0f45b2605069e4fbbe0e281bce8586` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/conn.h` | MIT | `2348f3c96f7bba4ef15aa5b319e63ec9fc7ea39cc655e959e6ef861f9fb9d2dd` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/dp.c` | MIT (license text) | `10b6ec784025e92b6a3c16eec525bcf74a422b28771e16f99caf596fde25e2f1` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/dp.h` | MIT | `89bbd5c663b8b467405702831a75c70e837a34b343b7740bf1930e260e2c69d8` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/g84.c` | MIT (license text) | `601d2de3c54f53b417bf80e37f38b0da2621bbf1517f9848c5df6b0b45139518` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/g94.c` | MIT (license text) | `0514281c89ddfa823de910f8376a409b6c8829293e467c384b623884d70adf69` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/ga102.c` | MIT (license text) | `3a7bb3aa30c548cabf9078d5295e772b81fc7b66d27326db2451b8aae5c35cc1` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/gf119.c` | MIT (license text) | `b00c2a325e0783dd7948694fda03520f46ed9eb656c16b04c6f4c47d32271522` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/gk104.c` | MIT (license text) | `9958f5634833d56f662900c3d7788acaf69cd424669a8a79ddc902c6ac018729` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/gk110.c` | MIT (license text) | `2aa8c079f7532f98b057174d207674ca4872360899e29826b67fd660a07c4b60` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/gm107.c` | MIT (license text) | `572bb29283a26d176b80292365945838cb38ae74760a91fb736dabdc56987fd0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/gm200.c` | MIT (license text) | `64775b566fcb1f385dd4e80258655ca19b166892a851ac0c07444d467183ecc5` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/gp100.c` | MIT (license text) | `eaaaa30268af18c68210d352cc6daeacf7f78ac664b3e3b5d712d40fdea41c18` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/gp102.c` | MIT (license text) | `db4c97d4d67e96af8ea0d1d35c6f9f920d3d55fd6e2a1c99cd1d1e7e894cd1b6` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/gt200.c` | MIT (license text) | `88897c95e29a6dd54962a3a46638161211b7f819781a39e741e9e2668c306974` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/gt215.c` | MIT (license text) | `55d980462f05ac27b7dae62f228e7d5afa6acd84a6bb803a44e524d741131f11` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/gv100.c` | MIT (license text) | `2edf15722fdc66191038e3921bb6b0cef1c8e442c10d858284db05e3052a66e6` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/hdmi.c` | MIT | `2938dda7b4ddc99e45c90628aeba0c465ff416671f184731990957a492b3e138` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/hdmi.h` | MIT | `0f9ca59f66f6903984fcfacff9e0080242e1a291bd554fc2f843881ac44b9ca7` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/head.c` | MIT (license text) | `dbba3db01f48bc1ad58597b05a772cf289b7e2841af1aefeac03c58770f8b493` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/head.h` | MIT | `50a732ca013a201b9ea56eee7fed6d68653c069f9f73d0727e8b064c86678336` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/ior.c` | MIT (license text) | `fb97c2ccb29c15eb468396495192db8d6bbfbab1e5f94cd53547a298348fea89` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/ior.h` | MIT | `60d3ff79b4382b51200a30685f832b5604621aa0db05852cb9f04a0049595fb4` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/mcp77.c` | MIT (license text) | `e10c4bb43d6d1e4a9ef3266d1be079da320528b5554fe23de8d77c69933883fe` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/mcp89.c` | MIT (license text) | `23f6f018398eeb21bfe5b269de0ec93e18f8384415c8a19f1fee23edcb027a75` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/nv04.c` | MIT (license text) | `3d76312509b9fc861df875b60ee58bdb4afd7155a96a825e30a33b09ae241ca3` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/nv50.c` | MIT (license text) | `fd20c951fa04189d98f1038a41c5774c7b1b95baf151f92894eab826029cae95` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/outp.c` | MIT (license text) | `d6b0b8ed515bb91f2491b3a21721894b6a5327d40daf4ff2aa4c84258ef52162` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/outp.h` | MIT | `0f1ddeaba480b7ded8bb8c506a9fd1ac722d44c9a6a924b1797eded4dc2f3c23` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/priv.h` | MIT | `0c28fa8b8a0b7f5ed29b905c239b752aa54c0fdee439b14064f28daa0421fb35` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/r535.c` | MIT (license text) | `b577e1052a0e4ad37f4ce4570dfaf87456dc583e2668692a6b82df3daafdc9ca` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/tu102.c` | MIT (license text) | `40f1682301caeb41b67f299031f1703107826dcdadc5c9ec06f271413267cff5` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/uconn.c` | MIT (license text) | `64d0a106c0f4e3d552adf0f1c568681a101c186c3135ab622abbffcc65f69ee9` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/udisp.c` | MIT (license text) | `8a3263ec09ba9702087edafdf3da253b7d47a4d3ac10badf5ec7aee3981cb792` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/uhead.c` | MIT (license text) | `674b175546122e3c0df0ce153e06abc60ba895e59b7833e15d43581e181d74c0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/uoutp.c` | MIT (license text) | `f3c53933cac888c46d57ebbaf84ed53d433e8a20c3293bf82a7001f4ff361ab8` |
| `drivers/gpu/drm/nouveau/nvkm/engine/disp/vga.c` | MIT (license text) | `cb733c8757392b92bb16d3ede8deeb9434d78e03fb452b36d128601c755fa711` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/base.c` | MIT (license text) | `0010bba3861e3739778f0d39ffa5ff0babd527ac189fa7e8b5537d906a4851dc` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/gf100.c` | MIT (license text) | `ec56507d98e89e2442e396781b8cfbb43238cf3b4ded5e392552c9c9c6e6254c` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/gf119.c` | MIT (license text) | `956b14254ad39d4d100662a0b3d6a6d3bb9e2dfcdab9f55ca7a9f97f9b521b39` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/gv100.c` | MIT (license text) | `977c7bf51c295e82623e9b7ef538bd1395a3c4676e0f1dc639b5191df76aede0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/nv04.c` | MIT (license text) | `55fe76da4a0ae2ec7e2c96ed679739efe3ae1fae00257e9b9d870cae5c67cbf1` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/nv50.c` | MIT (license text) | `28b8a23b8f0c84cef676f951fd995a98cfb7a7c9206a9e067b233b48627eee52` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/priv.h` | MIT | `0b1aaa9a37981a4afa82b02cbfde81ae6e1914f5a7e0a9bea1ebe8feb051cc47` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/user.c` | MIT (license text) | `bb3d4bd107b2b5d554f37e9453240f03afe9363da57347e93bb53882b4f830f3` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/user.h` | MIT | `dc26b8eb805aa977589248729f4360c6c4c784f7459697dee3954c1ea2eb3e7a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/usergf100.c` | MIT (license text) | `fbfe880cb332aa7ac389480dbcf91dd4a2645a0a199f42bc54c68f39ed74ab1b` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/usergf119.c` | MIT (license text) | `fdf14d174d3ef34101912c58c85e3c9043eba42e936b48458104fe28a6234648` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/usergv100.c` | MIT (license text) | `c8edbc9e2c9f52d435b52a3abdbf202a02e0e90dd7584641b99fbe2820e12539` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/usernv04.c` | MIT (license text) | `9a688dd7734062617fed3857545abe61b24661714604ba56ff0677d7329c067d` |
| `drivers/gpu/drm/nouveau/nvkm/engine/dma/usernv50.c` | MIT (license text) | `1df3db0e50ff3aac6007c42f445d3e20dc9d707cbda579963614f9efa6b4b82e` |
| `drivers/gpu/drm/nouveau/nvkm/engine/falcon.c` | MIT (license text) | `c0fe6b9a3d585c851f026c3555eab672e6689e3bd8572e4340833442b8c969f8` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/base.c` | MIT (license text) | `d97c42d94685b974e0bd1b3c572623fb858242a324c38c866730d7a1f700c3a3` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/cgrp.c` | MIT (license text) | `75077d8560b3071d5b9242c84f62ae9577698d56d3fccef7fc91b5d2dba7d0e8` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/cgrp.h` | MIT | `f85bfb6029ff4beb64b4c29cf6553d7c60da58254c0379ac47d67686fb2c0572` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/chan.c` | MIT (license text) | `a85c3ed9aee6f578523a5a427d2214453d71d7ec2960684a07f3ee2157840bc6` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/chan.h` | MIT | `62a3dd6edcf42efe0b2b7d6808251de6c6a6514190dd2890f3706e4073757ac0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/chid.c` | MIT (license text) | `e12a65b763452940c396cacc336c07f07752c33b4266bedb18506e4b4af5895a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/chid.h` | MIT | `754f8e9e9b6699c1050c65ac7c50b73a10cdede5019b655aab9ef17ba5295ce0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/g84.c` | MIT (license text) | `57cc028e617d1971dab40d1f9667c8eba8f8dd83b73f46f157a326b70c0411fd` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/g98.c` | MIT (license text) | `85987c2d233593b4786014096f28319bf9e1afdcb39f01260c5c55ece005c2a8` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/ga100.c` | MIT (license text) | `27211489571b6364cd8fdf2482cce128af069fc0996cddbe917ac2bfd4977af2` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/ga102.c` | MIT (license text) | `46b62c5495f5ade0bdf20ce5f1626423a9b9c9d442bc944a3149544a8b0aa70b` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/gf100.c` | MIT (license text) | `049a0ff8c473d28c40b97f6482c47f974338f3df2660e8bb2dd70bfb29e49371` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/gk104.c` | MIT (license text) | `d9ce2a7495358c75e5cefae2ceae099d3e161ee960cba2415ba24b5c08374dae` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/gk110.c` | MIT (license text) | `581d835a89d4be2a4f1a4383efb56dc02b81f66d055020f744654968b724a9cf` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/gk208.c` | MIT (license text) | `bcefcc8f6fc1afdb4749b18d22e88d7937ab31da58dc3b96eb1614dc032317d7` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/gk20a.c` | MIT (license text) | `5771312b641fb4bb75af692d29e61ce05841bad342fae371607431ae5ce0d577` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/gm107.c` | MIT (license text) | `5f931394bbf314d65119713936468c5e5d9bd7181f6ea622721014a4ffa4e621` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/gm200.c` | MIT (license text) | `3c2a61d966f4c23abc102397d183d9ad7e7733370ff37bc307fa17ee2e96e256` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/gp100.c` | MIT (license text) | `ca5530729efbadbfd428c92cd29441946ec566e5b913a1af4fc98c970eec4327` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/gv100.c` | MIT (license text) | `48b321b44d5551edd2e752a599fcda28284de6cb63ed9bc8ef9e3191e90c3ad0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/nv04.c` | MIT (license text) | `669b754f4f3f59328f67add3c223020e8ae02059058acc206db5b1162330d45c` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/nv10.c` | MIT (license text) | `877ea4399892860be90980b32e13d45eaad0ec3c05dbe2a5f9b76cf4a051bc27` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/nv17.c` | MIT (license text) | `8eb0faf3435e4d7c2ca81cc9675a64c08df49ad8970e4c6b483e98a46e0ca97c` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/nv40.c` | MIT (license text) | `3c2262d2b2a36deaef27417551023091c94f887436823c02375b9ff957cf8f44` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/nv50.c` | MIT (license text) | `15f3770ed12fa370763aa3279815e951160c3fa4270ca40aa68e5361cc3e079c` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/priv.h` | MIT | `89511c3b790d87686b9be0cfdff499e9f0699c2f7015fe2b476345a83ce671e7` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/r535.c` | MIT (license text) | `97dee259f3124e9244ed7224cc4c3202a4f94b52dcdd9ff9d3c7a4ad50df40e2` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/regsnv04.h` | MIT | `4fb02eb785d33f033621069601f7ec2298eef9feef5f09a01fe97078fd7cda72` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/runl.c` | MIT (license text) | `ff0b89d720769d341363739e0073187bab39635f272f30b4daa3f899163de51b` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/runq.c` | MIT (license text) | `d40d279031e915e515e49933136caef7a6c43ba70ad2204ac45d25150b355fee` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/runq.h` | MIT | `0c44820978928eb6feb932eefde7f9bbe770c2613ed62216f4fa769e5cb53b4f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/tu102.c` | MIT (license text) | `1eb9989e922e627fc6bf56b90f373075683de122d2da1be0371e70eb6dae0fca` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/ucgrp.c` | MIT (license text) | `c076f587da2a94ab3dcaaf30830512d16d67ccab14939aeafc6c6a87fd6fbee4` |
| `drivers/gpu/drm/nouveau/nvkm/engine/fifo/uchan.c` | MIT (license text) | `da085333c6aaa1789d8df8656ead119d851b7e2e5f0f43ca668221e3d37d2fef` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ad102.c` | MIT (license text) | `82314cebe7b06da511c77e57e1eefceeb2c8c18dfe2a28c4969d6eda6bcc1796` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/base.c` | MIT (license text) | `16251e0b8a1bba2281f694a94a1ccd7279ddda2b138fc8fca74055a6bb1609a2` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxga102.c` | MIT (license text) | `85f8197b7beae7fd5e266964b761e2132d622cb8a19cfe19fa7928328fccff97` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgf100.c` | MIT (license text) | `f22a88255ddcaf1089965903a172f79dee6103a1650b431c355d402689c71b6f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgf100.h` | MIT | `c06917126ea1f6c2d910757de7301a3809c641081a3ae7a079537617cee666d2` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgf104.c` | MIT (license text) | `ecdd54cd10ca74dfa568f427c32fbb7444b5eefdb396e006a59308f24e83c7a1` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgf108.c` | MIT (license text) | `c06cd3ae36dbbe57e44cff12fc513cb7eb5b63f3204126f308aaceebcdd9d989` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgf110.c` | MIT (license text) | `5a03337fe84416035594b627b888659bd23e1c32941eda572af5e8dba19a51c1` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgf117.c` | MIT (license text) | `f8869c05203618ac55e762d13d31fcd0c93e6bbae600a6898423ce062860033c` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgf119.c` | MIT (license text) | `0b101fca41fc239266bb4dd8222fbb0e4f3163c692ae662dfcfe0179cb79fe39` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgk104.c` | MIT (license text) | `c2f9b1ff6d1aae759f5d4129ebd588d5e749a580547c296432e249c5e8df04d7` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgk110.c` | MIT (license text) | `0d113681135d06ed4e8db876f2d083ca470edf6fa46cb2f335dab826b8058cd6` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgk110b.c` | MIT (license text) | `b3a5c224521d8211fc631dbabc5cae1d43aed7a5fea4b9604be491d881a4e14a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgk208.c` | MIT (license text) | `671151b6cda10e1a6590a3776b937caa234fa2113c97613950dba21fb51024f6` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgk20a.c` | MIT (license text) | `ada24d7fe1d56044c1a8291d43de9332e7632588ac915f1dc47806790cbc9cba` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgm107.c` | MIT (license text) | `fa39406ede888ade53875c9f9cd1388d442d4f5197b9bc6cdab43836097feb7f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgm200.c` | MIT (license text) | `e34e03c3ac5ef57c1b2d30deedb9e7106cc0a78df7d4e7f25e05d554da21dbe0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgm20b.c` | MIT (license text) | `8f841e422cc1cf4116201e526d7d297df6059b7370919568e1262106e71513aa` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgp100.c` | MIT (license text) | `c76ab47b0ad6862bb87f587908ceacda334737d990a9385833bf369faba44b0b` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgp102.c` | MIT (license text) | `7c91270691f0967a691f2166dca0d36d353d987eb915d36366e5f532a9061d4f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgp104.c` | MIT (license text) | `fc31ddccb006d204f499ea0bea9ba13ae7493dcc9366ae7baf43e1a0bf38f237` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgp107.c` | MIT (license text) | `2224b95f2d4b849fea94ec7406825c82512e5c669bcfa279ee1d5d251ee39c67` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxgv100.c` | MIT (license text) | `42ea0ad8aadae1fb67852645b843db46bbb9d6a28bf079104efa962d2d8842e3` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxnv40.c` | MIT (license text) | `5f9b8044490a0d5c96eb52330000aa1260183505f3e8990602059c6c59dafd15` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxnv40.h` | MIT | `0143c6024fe279d7c5f4b26ee876a4a697a2f386e79d8bf8df285d3ef50b906f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxnv50.c` | MIT (license text) | `c991ac7edb54f342d59915a72b8bd6e0cb0ea623e08ea39e21c22c7198ec74d1` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ctxtu102.c` | MIT (license text) | `ef61ece43f05b21a3b2f976ae5fda0b8f866a101e65126d33798dfc085717006` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/gpcgf100.fuc3.h` | MIT | `dcc8fbee6ff47da0a413719753033ccaf087cb215de11cf8ce268b4d4f295045` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/gpcgf117.fuc3.h` | MIT | `d72717a8cccb7f40f6d1eca970f8715027eb8ee858ec7737bf67b872448ea684` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/gpcgk104.fuc3.h` | MIT | `8474328f1496f51cfb6a7c45ba863b60b3fb7eca74ac4cbcf537d967387b3531` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/gpcgk110.fuc3.h` | MIT | `6c07071db8ddbc5dd638edcfa389bd4d0b718c87607d7b2e4ca87703e9196c93` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/gpcgk208.fuc5.h` | MIT | `951f78cfa94e3aee96123d6933ea25b8f3addbe727f235375a0a3eff6d31c8b0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/gpcgm107.fuc5.h` | MIT | `9a25b7c9d0bb1c40887e76f2883f4d5d397462603d055124d80960622e062812` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/hubgf100.fuc3.h` | MIT | `608c7a1c63d12ebeb56839cdab28dfd96eba0170a17f1eb9b509859561f31ec7` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/hubgf117.fuc3.h` | MIT | `76cfe716b9976d3837553ce4f139704555a3508680ed1bc04fe32614915437f8` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/hubgk104.fuc3.h` | MIT | `93b6d541e025a5e66079b1307f783313a3e59703eef1bbf7703ec280bc8566e6` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/hubgk110.fuc3.h` | MIT | `87e615b8b79d9d5f96530fe847aad51dd46cdbbbe39e00c3b3860b6ae802226f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/hubgk208.fuc5.h` | MIT | `a85e8918462f1f5c1e85d3b0619f80aa3c42af2aec74b809acda2495ae20e32e` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/hubgm107.fuc5.h` | MIT | `e79f3007e309f078335d8514b4cc5d50a5c23ffa8e6dbb082eae9e0f86f7acc4` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/fuc/os.h` | MIT | `4c69991407e82306dec7364d5fcc67c08bedbaf32b0e80d592c7665588db336a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/g84.c` | MIT (license text) | `1f3e4b09260b7e71e22fb10f1bdd0ad43a2c5cc938e25b4b28b1c2f616e5f865` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/ga102.c` | MIT (license text) | `477601aa16207720deb2530939aab1bb60e643546a5090e6fbbe7620786af436` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gf100.c` | MIT (license text) | `bf6f0d5d5578ba2c0eafeb7d57864f124624f5e34978fc9eb82f181e84b73e2d` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gf100.h` | MIT (license text) | `513d4fc9b4b63b7b6cf9b6780a5135cd8d757bd1339755c58e548054ffc353c0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gf104.c` | MIT (license text) | `ffb1bf8be90d8cba821c3cc76bca1f1fe1ce0ed258336fa5835af9f0765ab26f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gf108.c` | MIT (license text) | `703873852db1ca971049823a1b99c5d5b15e0b328112cf5f292b8a6a353d60a0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gf110.c` | MIT (license text) | `4e4e23c4405c61ae73dc93b80f168c355c0e9f4c448645b1f81d34350cb1af56` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gf117.c` | MIT (license text) | `fcf619c5071def99c3ce0011d560117b0ac88430a4fe60841a85e8c176594d33` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gf119.c` | MIT (license text) | `a5c30dc78e69a7c72a9e44b3511c3a950267ddcb57aa3a8ce9101657f7836641` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gk104.c` | MIT (license text) | `71b413d2d0a51e364d6b5bd93f3a59df555995ba603b185a4cf0b98a20f3ae34` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gk104.h` | MIT (license text) | `27d1f998eaffdcc31e66f03fd58d144a7c7614be85d6fa866b69f8ae4b1a78ff` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gk110.c` | MIT (license text) | `3af47fec0c4c2aa379e51b14fe43588734181324b55f20be6d5c6ce816f4d693` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gk110b.c` | MIT (license text) | `e194704daa4a9fd44f607d1e9095cb9454ebdf0271e43239be9ebd14cb094a32` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gk208.c` | MIT (license text) | `7523dbd732013be07a81da51d3fc367125abe8625dd93caf789d0a88d7569f09` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gk20a.c` | MIT (license text) | `ca22a0ae7652fc0de509075401b4f7ac567d7f14aac4638c6aa189b1f966a5fc` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gm107.c` | MIT (license text) | `82e3f794c8a8f4e5b83ce1c8bada71b080126ba9b04dd2da2f10f9f522a48877` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gm200.c` | MIT (license text) | `45e36f384792c84078e47909927b9c77ccc0d48cf2b30b7b6cc936ab06d84e18` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gm20b.c` | MIT (license text) | `a44ececd0003e413d68266058e31bf2d2f40a504905689333839bc391f997ca8` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gp100.c` | MIT (license text) | `62fba72abdfb7d7c190436ad753862aa344d832cce31ac2778930fa9420de375` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gp102.c` | MIT (license text) | `772f718646262b11d25ce7dc9f57cd6eca5336d351ff4d15114842a8e2c06dc5` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gp104.c` | MIT (license text) | `9465bfed4d64a00d20eb5f27a0b03fa4109256c5a3e07f8f4c1ffa42ec71aa45` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gp107.c` | MIT (license text) | `e840b1523fdc2564706552573bf44dbc0ffcb8c00e86628ef3582e6246c0df33` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gp108.c` | MIT (license text) | `5f2b8b5a2f3bd75137916d90d7641a073734c3c5795fd3cd77f41ed9681f9134` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gp10b.c` | MIT (license text) | `9a84ae0d9a726beb7f3875d8cca85d4a33c6f70e7c0cd42837f449a038c35419` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gt200.c` | MIT (license text) | `a29212af90e114b099ba2b5f7ac147854293a95542a0c8f9bcd0b5133fab368e` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gt215.c` | MIT (license text) | `a1e926893b7bcf2a873127041f964e9194b9a1ec30dd98fbc1e211689820a446` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/gv100.c` | MIT (license text) | `9e84bddcb9ca7a0c4f3d2b7e3f4bb7f9ce164230afe274aa45db29b964db920e` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/mcp79.c` | MIT (license text) | `c1195d82436a43057821680718016a3e055465b7280da51dbb6eae59c3c42843` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/mcp89.c` | MIT (license text) | `e45c0a714539e67502c3b84a35b4335b27da7d9f19b9e377296366a08788ca09` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv04.c` | MIT (license text) | `206b5cf0f4e2eb7e5ddcc17debc37e43b31b7451d97f754ff63fa67fe1bb7761` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv10.c` | MIT (license text) | `e83c9416cc9046e505889a2446cf481f3fc7c757d82792a219d3b2f2bc70176e` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv10.h` | MIT | `63779a8748043bd57441cfc362aaa5f0021e719af28033967798cee24c3cca7d` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv15.c` | MIT (license text) | `021f6cc3c2aa7eedfe606858ab932aac7cbaa059876ff2eaa51d459aa0e3f94e` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv17.c` | MIT (license text) | `e1c664df764d9fbae31621e301eb1a143d9fb7be8f72b6f74fd157aa88ed8c4e` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv20.c` | MIT | `e6b548e5bcd17d7f008b0fa99103497cb205a14bd5e2ef768b02b6acfef2c530` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv20.h` | MIT | `d559cdbf240096a0e42790967b4cc9bd6f68ac94acbb138e87263faf7cccb6e5` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv25.c` | MIT | `98eefe1154db45ed7ceae4d43bb3dd8a04e1f495dc37326c4e2214f2bd9f703f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv2a.c` | MIT | `f9a890499b657b43687b1eb0732324d93d5d0b0722d12c67d1059839be8c9a6c` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv30.c` | MIT | `bd63868e8e48cb2f1901a541f4d3ef4f75f4ee750f7059bec323f50c68836b5f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv34.c` | MIT | `21da93d5f6350cb90fa243ed682e41e2a22d219f8f877d710c30b0b57e41c653` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv35.c` | MIT | `53a20167abc86c6e1ad3c5a48de3afb6aec0cc07a0384bcded81703c0c948306` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv40.c` | MIT (license text) | `f5017c8772e37ae869a5f47d9d0d4f3b0746c425b3493a5ea3c071c37fa68a07` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv40.h` | MIT | `52dff8b70d85872f5a15cf594f4ac5daac031b16700d6e701d31de4cb355414f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv44.c` | MIT (license text) | `551d1fd5e41bb0d66352d2402abfd7251244c8f9c74615e0a0790b9e12c20569` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv50.c` | MIT (license text) | `952c1efb3ce07a282b1132a4911a6707c733fcd9bb93e77fac5eee5848bfa2b4` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/nv50.h` | MIT | `39969e2f40c3931a8c63a4f8e781c8554f333b464382568003c67966156872d7` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/priv.h` | MIT | `c7a972a3419e11409a8245e1b5c40871885f4218f00dc58496923b6215cf4dec` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/r535.c` | MIT (license text) | `201300c373b0f83f7bddbeec108abc7b5c7be65cad955e88cca2592860e37524` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/regs.h` | MIT | `c762742d0c7978327ff356955ce3d185703c2920d147886c4180e2e5586694dd` |
| `drivers/gpu/drm/nouveau/nvkm/engine/gr/tu102.c` | MIT (license text) | `0d962c1ec3de1445cfdd7aa5545dd4fe9ed0fe527abf217f41efc0780ca2040a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mpeg/g84.c` | MIT (license text) | `ca9d22903cef0bd03d4a791717b9b3e0f94614f9057800683e7dd7c800d44344` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mpeg/nv31.c` | MIT (license text) | `28254c0031779ea9d4e0c3a7f6ff2c0f1229fdbddc53bbfa433d5c35ebcc0b0b` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mpeg/nv31.h` | MIT | `0b59b1fcc0529f5a125fc713d82b40cfefc085e16acf2dab98d782f18c38226c` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mpeg/nv40.c` | MIT (license text) | `d85090bd42f9b003fcc65712e08cd48a912612f318d9479a4ccf9ca9b213466c` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mpeg/nv44.c` | MIT (license text) | `c15ef38ab501f7ad0d12448cf269e9c29d7770eb2948bce1f9d90f4f4cc545ef` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mpeg/nv50.c` | MIT (license text) | `4568a76e3bbe258b0216d08cc3237fe14b9f4f61c99ab28bb9401cb953dabf48` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mpeg/priv.h` | MIT | `b4fabf9eb94907b9e9fa983bed3510e0af0a2400570496174502786d195da4ef` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mspdec/base.c` | MIT (license text) | `49a34df89db117c055b5ed913a74adefe49fff41ccaccb62a2bae02d44dc375a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mspdec/g98.c` | MIT (license text) | `995039bc1bcb39caa2b6ef54ad10d21ab9eb22758126d2f375904014bcdf4b7f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mspdec/gf100.c` | MIT (license text) | `c7fd57b404fefcc80984c912b857972e51a3cfbbd899e81d054a4aa93a9cd61d` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mspdec/gk104.c` | MIT (license text) | `a3820ba9136b938ed1ce5d896a2ea30810f1bba0f325d7c47cb30f613102e58d` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mspdec/gt215.c` | MIT (license text) | `da7889f98f1c96efbfc9a361bfc848d5c6920578da88a0a752f623c3f2693655` |
| `drivers/gpu/drm/nouveau/nvkm/engine/mspdec/priv.h` | MIT | `451c60cd8f6d4cd9be87dc18da9676083f02cec5133ba96575fd1b2e5b53fa27` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msppp/base.c` | MIT (license text) | `dc2151991841086830e4db37fa496448647bc06035cc950932450f453f02496a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msppp/g98.c` | MIT (license text) | `239506999549419574200f7f8b0d81d0fa5c027c6cb52fb9b8f00b98586e14d9` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msppp/gf100.c` | MIT (license text) | `20adfd0d0db1b9c598d8374bb1cdb34aad434bd3862b33122cd046ede830051f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msppp/gt215.c` | MIT (license text) | `bcafcc95abe39b26c522a158b4922860774c0516964fbb97fdd2c68ec6d8433c` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msppp/priv.h` | MIT | `b0214d6d6d9fb24cd82069c59114aef12b25f8427cef7d51b25a332b8a7426ae` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msvld/base.c` | MIT (license text) | `133f4bc0625fa91486a520d844edcf355fcfa0ce5fd799578ede82507206bf80` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msvld/g98.c` | MIT (license text) | `c7510f3d76de292f64d72a8111721ce03bb6a4c363da133a45d3e19d11b2c621` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msvld/gf100.c` | MIT (license text) | `f29f99f12007a936d47c4162507d50e932447fc661f7f436aecae826a81bafb3` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msvld/gk104.c` | MIT (license text) | `00c2455c1b20ce48ea7abcdaddf224398ccd0a347caca47968203e7fbbd885a4` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msvld/gt215.c` | MIT (license text) | `8da10cefb44082a25f92e327b51fa5266b78c21440ebe9de1dc57c611a412cb5` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msvld/mcp89.c` | MIT (license text) | `75cb1ec084b15b1867d074dd3caf6c05dd9f3d62e8d6cc011ab0c43b5be69bc9` |
| `drivers/gpu/drm/nouveau/nvkm/engine/msvld/priv.h` | MIT | `51ac2c7e583c6efcff80b19e9048a41c11bf7a74fe0366e8443c8a946bc8302a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvdec/ad102.c` | MIT (license text) | `b8c1116f05c2bb9fb59d6c492f67ab4a0ae9d105bae24be7f556a1077cb226c3` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvdec/base.c` | MIT (license text) | `295351b4b35a2f68892a7e36e94d9dc6daf3887180bb28c26dd1c074af4d7826` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvdec/ga100.c` | MIT (license text) | `0aef7fc903fa585f30507f3b18ec6e4033ca9de7b111159f0f07a9fe1fca9c79` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvdec/ga102.c` | MIT (license text) | `80bf6b0ff6de0f9da2c7aa55d82981b3be69a684ec09aaa9471f7e9871272698` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvdec/gm107.c` | MIT (license text) | `ed65c0393474222e04b870cee9c62bcc5df8a88069b000c37a629731b4e442f7` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvdec/priv.h` | MIT | `2411b2df46084920a20285f34bc07a64604075b8c0d9c398afc2c158e65b8e22` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvdec/r535.c` | MIT (license text) | `0947d1272ce971779f544aa45daa8820976a4838180030ed60bf87a675a3f052` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvdec/tu102.c` | MIT (license text) | `3a86328d731f511347d889e9cae6f50b45cd16bea791a3c0c4a06f71d3464d8a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvenc/ad102.c` | MIT (license text) | `24abf2a908449492638bd03ae0d8019b052e35a151e401a77c9f18bb589ec012` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvenc/base.c` | MIT (license text) | `003e387d3a52d95814438bd028d2e00514d32d6943d90de5135a00cd6f77a903` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvenc/ga102.c` | MIT (license text) | `86978d9317b7ce2e93a294bb2f48a59a4cd4accb4278a605deaa4f620fde78aa` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvenc/gm107.c` | MIT (license text) | `c844115e054bfee8555b0b031a7c5dc5773704b586b6e4db5f1818bdceb44b40` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvenc/priv.h` | MIT | `87ca1b4484c3b8eb577b8e34cbb60f24751ffd7761fdbd138e3e9ee0f0b270c6` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvenc/r535.c` | MIT (license text) | `90dc65ea52911bd4aa7bd2d2bb49c1af392ba9b2d3260cfdf428ffe065cd7463` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvenc/tu102.c` | MIT (license text) | `01657a52355982be692e781ec9a738c51fec617c380d06421c466540edd81e86` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvjpg/ad102.c` | MIT (license text) | `64850600a643d4ed5e88e2eb931bd332c179f1232841a7f0f5293ee22c916e2b` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvjpg/ga100.c` | MIT (license text) | `e2be4693c8108c30732419ca0f781ef85929ac58c6317ad754796ca4d496b03f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvjpg/priv.h` | MIT | `dfdeb8499a41f3eae66a36c65b2109556f37134ab5b86cb91571a30158b691a0` |
| `drivers/gpu/drm/nouveau/nvkm/engine/nvjpg/r535.c` | MIT (license text) | `f355d4a097344f17bb2a973d5789990b10f59619454971d7dde596ea0eb04086` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ofa/ad102.c` | MIT (license text) | `e41e36d37a90d0fabf168d734588b9164e87028949118940c97fdeca32f381f2` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ofa/ga100.c` | MIT (license text) | `6b87fe32d34f5ce96dd6fdb56a27d256db39c2b048958a825e459130777d9bcc` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ofa/ga102.c` | MIT (license text) | `104cac3e09596577b41c556222bdc92c24ce257720b7c33a4af6797f782c9469` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ofa/priv.h` | MIT | `5a7e98a71446b9224e68f003391927f548274ee8492d51b033512dfc1c29803a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/ofa/r535.c` | MIT (license text) | `1f91724106bb946c587d73c5e48d3878bc93957dc10742412334317292ea236a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sec/fuc/g98.fuc0s.h` | MIT | `c382007248de377ef984fcd6885a69c1eb278189148dbff29a1fb16982a0c49a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sec/g98.c` | MIT (license text) | `e7b16ea88191c42ca6224800e64b8ec6089231370c2259734579adbeb177f2c9` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sec2/base.c` | MIT (license text) | `9d2c099ed6c8f346472f043c94f87e6a236f982a8f58355a7fc020fe23753184` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sec2/ga102.c` | MIT (license text) | `f8c9388cb4bc65cfa7ba6a2a5bcf00753447c58876cf58d2cc4d19e86dc91c07` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sec2/gp102.c` | MIT (license text) | `f4b92c72ed90e7aa828d271012611f6b83df8555a578756f269a0ad10a378050` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sec2/gp108.c` | MIT (license text) | `b289ca1aa4cb731547444277d6838a91df8a847332353bef0fe4f6b3d59f96da` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sec2/priv.h` | MIT | `f6714f47efcdef169710520257bf76b99b4b55344ef47dd4f1d64d75ccabc394` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sec2/r535.c` | MIT (license text) | `ac7c78d1adcdcb41d60ce7b6b13ce3919eb584deae7db696179616e5c4f5b52a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sec2/tu102.c` | MIT (license text) | `c9f42fde8bf7a961c98b5b327f8f287056c2d34af21a5d96fc5333e07f529149` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sw/base.c` | MIT (license text) | `0f8aa4ef5ce0c95f0f8071f4497871c195e1d1ebbad8278f2e08dd50a385a4c9` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sw/chan.c` | MIT (license text) | `8743322690c45bd59913d27f65587b45a09dd107da5acf63c02af503bed032ce` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sw/chan.h` | MIT | `4314b3bae4261c7fbb6b85a2962f72955cfd3251e593e059e3dcaaab4074bb99` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sw/gf100.c` | MIT (license text) | `dba714efe0bd7e3a7b2a9c7f5bc11fcf8f42160169ef04a4daa7f11f7cf8b045` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sw/nv04.c` | MIT (license text) | `c4489db5d0aa50a10cf2e1c73dd55ae2998588a2d6146a42babd7bb56d01be1f` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sw/nv10.c` | MIT (license text) | `e25b0cfe440188e4787b4f72eb9bcbfa76e6cc296ee1e3ce4d90ef9b9cef8a06` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sw/nv50.c` | MIT (license text) | `9eb21854ba308a658e7b1376f1bfbedc916435a414436fbfde0e915a4a5e0c7a` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sw/nv50.h` | MIT | `92617b768090ee40c6751e042c3461414269d15bd3d1ee4ce0ca8b11d4d14753` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sw/nvsw.c` | MIT (license text) | `9a592c36a5b532660d40333488b331b7f6df7f32ca19bbb755eabab8078d9291` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sw/nvsw.h` | MIT | `8b8098bcde7676507002ea329ed97755bb446cf28b08fc927588a34f7576b7ba` |
| `drivers/gpu/drm/nouveau/nvkm/engine/sw/priv.h` | MIT | `5f05f88037fa3e435cd5a394cd11dc3bcc213b5e17556bb10eafadfc3553c399` |
| `drivers/gpu/drm/nouveau/nvkm/engine/vp/g84.c` | MIT (license text) | `c0c4443fab3cbc90ccb1e66190bc5d382a5e45012ba10adccc05e11162791761` |
| `drivers/gpu/drm/nouveau/nvkm/engine/xtensa.c` | MIT (license text) | `868c3ed43c4784b233166bfb94793d5f9ba3b9f5544a64c3f9931b346b5cd582` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/base.c` | MIT (license text) | `ca82bd574209c1f5c759b9e683e527ff207cd48287ad911e416c844043f66706` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/cmdq.c` | MIT (license text) | `33e6d76cc92ab96f9f6ad4f729ae69c98e8644fd2981d5b3f334433881f335f0` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/fw.c` | MIT (license text) | `4b03f824b75668002389a6ee3e208160c9b3b3eb459cc40460bbed0f6898d667` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/ga100.c` | MIT (license text) | `208ac31899fd5da07d40e4bfabff29a8a51ff03dd9661e2a019e307e8a977aa5` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/ga102.c` | MIT (license text) | `de6389b846990c377a9efbf3005f925ba626ae0e7a6bc8c20a26cfc73ce710e4` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/gm200.c` | MIT (license text) | `5ca3214734eecbca9124526f45fe2fa710e4b9700d38c58a47e40fb2edc750c6` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/gp102.c` | MIT (license text) | `cb167e631f63d042de874cf621010675719ebdb44176c97e5281ea42a2ef666d` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/msgq.c` | MIT (license text) | `9af805320b16975a7ba666db4e0d53d110864190eab7345701f91d43f08b9cc6` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/priv.h` | MIT | `2ed53cea023672f1c684d922a1691ccfb78aceeaab038ddd7c58283af3a876ce` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/qmgr.c` | MIT (license text) | `b91de8cf69367f7044e438ed1955943f9717e16f28f192cb9e789a03754c0577` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/qmgr.h` | MIT | `76f6266bdffa7e679fd28f1bcba429f677361a93d4687fa327f31e84af49c839` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/tu102.c` | MIT (license text) | `95aa412aea719267c1a2f83e35e0742232d1268739270c7033ec3b6c3334fb0d` |
| `drivers/gpu/drm/nouveau/nvkm/falcon/v1.c` | MIT (license text) | `e70866998b34433a30b7b03a5f614332ac2902cc252bb15aecc3f0cd827bf9c9` |
| `drivers/gpu/drm/nouveau/nvkm/nvfw/acr.c` | MIT (license text) | `ef5802facfd0534bdb40a9e47424d2c4b25f62abe84ac0ed4d7deccab544f9e3` |
| `drivers/gpu/drm/nouveau/nvkm/nvfw/flcn.c` | MIT (license text) | `23f4215b45d030d7c2a76e390a2a53ffc0633db550e2c49e6fd0e667be556f10` |
| `drivers/gpu/drm/nouveau/nvkm/nvfw/fw.c` | MIT (license text) | `73af90046f4634b263a8df9183f7f828af524adaa13365f18e6cbfd244841816` |
| `drivers/gpu/drm/nouveau/nvkm/nvfw/hs.c` | MIT (license text) | `81038270ab41abaafc5860f9b66c843e78ce7be7dd93cd6f4b759e1f56bd5a6e` |
| `drivers/gpu/drm/nouveau/nvkm/nvfw/ls.c` | MIT (license text) | `da93ae366ae9f422d66e323eb68d949b79a0c7f43814efe402de6f6225bf64ea` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/acr/base.c` | MIT (license text) | `8250a0fcd05e44fb4da1368648ef8e2183768be293341767c660f19a3177883d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/acr/ga100.c` | MIT (license text) | `b84741a2f3be6eb3eaac2f94727ef1f949dad81f54f8cad2e5969f38cbe753d8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/acr/ga102.c` | MIT (license text) | `892bfc831e8a5df1b628bc6d891e9f765d3ab3684a6027e1234051ee44fb3277` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/acr/gm200.c` | MIT (license text) | `e3eb342d116930173c65db020da642eca92e4953fd012976676b98c19af34c62` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/acr/gm20b.c` | MIT (license text) | `67c29cf8665d0d2ea1b33cd6928cf158504d6229be28531ed07f7ef83f051aff` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/acr/gp102.c` | MIT (license text) | `9f7c39455b8656e99223c736056bbb84bc8bb0691cf87b6053168cd9bf3c5811` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/acr/gp108.c` | MIT (license text) | `8cfec4d916d949cb61f121beee2aad6c8d3ce165fff7df821b6dad2d10fd8c8f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/acr/gp10b.c` | MIT (license text) | `3768f5986e0f16a4a68e079e62cd138887cada2a20aeaf590370955577e82ac0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/acr/gv100.c` | MIT (license text) | `ea87983f841428baf98765c621fbb76c748f72d5911144edd56bf111f1fae610` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/acr/lsfw.c` | MIT (license text) | `6003b059c360f223a726d6ad4d1f525a19d0407ecf812491081d0136f5c1a5ae` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/acr/tu102.c` | MIT (license text) | `997deb9c688bd843b7ef200e459e94ed44fd8dfa0648730696fee83f8394c921` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/base.c` | MIT (license text) | `3c45f56750f8676dbc278c2c126ce9caa94efb5f4aedfc08041b929e569f0834` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/g84.c` | MIT (license text) | `28fb3702c0ab41c1ab4ee5840f57d3c809b93ae9a779b2a57115164426c0f6ae` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/gf100.c` | MIT (license text) | `25ec34ce7c71da1ada414114af4f5d9045c5b6b5c2d75917edd86f4566b802c8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/gf100.h` | MIT | `007893d4f6ea7c9ad6c4c050bc1a2efbba65033b4b99e56dcaf818ea6222676a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/gk20a.c` | MIT (license text) | `ddc1a3fb7878908c9424537625304fa88785f22ba656111e076eab94464656b2` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/gm107.c` | MIT (license text) | `6e13827f31298f07030aebb74b8fb254ba593c97ffd4fe5d491e02a496f45ede` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/gm20b.c` | MIT (license text) | `8e2ab6112d322fc725408debf60ee38f5894ad39ded9ce1f7935b73f72d1be31` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/nv50.c` | MIT (license text) | `43d8c6bc63129f187d92464c6f57d201b3cd0ec3795006d26c042cbf780cedf4` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/nv50.h` | MIT | `f5372d87bab5cf29d5eba7edb15e7166fe49d597e443b409255e6f7702ec2e90` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/priv.h` | MIT | `1a33b6ccccc884358e165a338ef03d10a4216843eba90c7dc34e6faf46b84647` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/r535.c` | MIT (license text) | `e8e28cd40198a73f7880fe1da87b2412737a9de9cf75672d8150dd7c5d6aaf47` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bar/tu102.c` | MIT (license text) | `db825c7127ddd41ce87dafe6eca08b9fadb3c2b158521fbb1175c52ed2b649d0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/M0203.c` | MIT (license text) | `f1138c6ae8616ab554f2390d2ea2e251a0b0c9732a3f1977e74345380c699789` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/M0205.c` | MIT (license text) | `d2b47983ef1d80d868699f97253753b8691a3fc1a28a27ce1d7d50061d02c488` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/M0209.c` | MIT (license text) | `fab3e4e0d32670272578996c8a427e988592e1da615fa22ea3634a543884968b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/P0260.c` | MIT (license text) | `77431001d0e98d8e578b3759aa201e9c07c14fa8aa819829dbbd61c4ea283d11` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/base.c` | MIT (license text) | `46ed30acc887a81f9f7f6e1e13ebaf1eb89f61d4e4f9461b3bf88eb65e722541` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/bit.c` | MIT (license text) | `955da1bbd3288f978830d2d713406cbfe2929627a36769354fac8515495a9af8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/boost.c` | MIT (license text) | `bd703c5ea521dcfd66b4b0cb9958713f25cf48f17271ca50c2d313779e25a92c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/conn.c` | MIT (license text) | `8b658d114d554ac9623b5ef59a2d8ff22d87905b7c364f711a6d387634bf625d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/cstep.c` | MIT (license text) | `27c000d772640d3b518d90a08bd0462a02ed53f172a16131b7fcfa7b434198e7` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/dcb.c` | MIT (license text) | `328e5f6d7220e4ca45f1ceb2b3d1443d5e41a513673c5411ae199b7939586a0e` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/disp.c` | MIT (license text) | `e5694d268bc5129d7c749358f77803ebf459c806083e3d774d61c838d6c3d9c0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/dp.c` | MIT (license text) | `aef6d1636516ec647c9f109eb7915a364169a70e0ac39f1ea988ee12443c06e3` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/extdev.c` | MIT (license text) | `3920f74f69fa22c4b0bf534e8d0ef1e8bee91f8e6d24a1219efcae1f6774faf1` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/fan.c` | MIT (license text) | `94a4bde55b1b34f2b0939fc46479dcda4bc2ed8a0fa756d63386edf0eb55fb7e` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/gpio.c` | MIT (license text) | `00408520241fe398517e2e3f99c8d52bf81f186c992375934dfd8ba684b61260` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/i2c.c` | MIT (license text) | `4d51892b5c771f01770ce092071f463422dff53e047e648ee7cca49d828e46aa` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/iccsense.c` | MIT (license text) | `fa11336dc07961d37dc43d5ecf1ac2c655e3fc9c10f77f6d75428431e9e36619` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/image.c` | MIT (license text) | `52da790d1fe46c8a47cb933558b0991e334fe8dda8a303550e396fded0f5423d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/init.c` | MIT (license text) | `7af9d95f2fc4b61bb6320c7198505b19b2e7f88cbf3952f1770518f863c1811b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/mxm.c` | MIT (license text) | `439081e369f1acd97a1c525dfc997b885200a1e01722c2f026690712efa661ea` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/npde.c` | MIT (license text) | `d74ca5ea0b063a378097bf50e41fdc61bcc4966f7aaa8c0f0e7fac8eccfb2e16` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/pcir.c` | MIT (license text) | `c588c19ce5a8e2970f0f38afca2e3f730a4da415b4342f6ff02fc829460539cb` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/perf.c` | MIT (license text) | `2855a0ad5f6b3fd19616843a128e2d1fbd4d67a567ce34a4acff5521b8f5f197` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/pll.c` | MIT (license text) | `31b051120e80bcb546c5b1ab43abf10a05c9c5e029cf316223b828237ad12da4` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/pmu.c` | MIT (license text) | `3f55b85a1e01cb1dc4e59b0d8d68bf6c9f2625046c5d251f283b3905ce41df41` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/power_budget.c` | MIT (license text) | `5fdd1849bc7128273438ba2b3aa6bbed65856958b29a92922e1e884e021e097f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/priv.h` | MIT | `c4337896db4ea2f0f85730463a7d8251e26bc2d5f9401e9b252f8890ffdd988c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/ramcfg.c` | MIT (license text) | `7e727829ac5db539c20d1584b87dafd068be82ea5f769ece0fcb7ff733a35435` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/rammap.c` | MIT (license text) | `c44ca2d5f2dede2835593ae8d59b072c5a21bd4aacb540a1cbfbd69bda3aae83` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/shadow.c` | MIT (license text) | `5677d72bb76103162f48bc0436a5ae7af443abeb4408dfb1619631cdf19804e6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/shadowacpi.c` | MIT (license text) | `dee10f98990d157123ba9713972eb9c0724ea93774792baa9bd1ad82c0b60992` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/shadowof.c` | MIT (license text) | `57170359acd540848f88d6f67fa73ee7888d70f8d48d302f7ca6c7d5663cd7d8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/shadowpci.c` | MIT (license text) | `e5515f2123afb408338db6a59f82d2d96297afe50a561926c0cc110a1be20743` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/shadowramin.c` | MIT (license text) | `5993608771cf9fd77db71b2983b00b5ec1e69ef6e534005bd6b2cfea47d31ffc` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/shadowrom.c` | MIT (license text) | `20d4fe363b323165a95914a42f2082d2abf5d24be778f92fd10e04430731bb3a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/therm.c` | MIT (license text) | `a499d4db2544525af0876f1d0715a1ffedec855c7db96043ff1c6393907316cf` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/timing.c` | MIT (license text) | `5cae01b85f9c7251d94cf046f075f5624de8b19b91037c328abbd944ac186ce7` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/vmap.c` | MIT (license text) | `e50fdd9ebe2df8f34aa4100784909851f8db056f152dfb1143db241cde61703b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/volt.c` | MIT (license text) | `456c0bb7ee58a1bb54bd3ded9fedb2cfd2f96f7a00fc28b025d5e8fff89ed644` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/vpstate.c` | MIT (license text) | `2795afaaa6f870a809a40d54cab5feb2c2b07c5cd74cfe4286446bb69354acba` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bios/xpio.c` | MIT (license text) | `db4565fb6e4e3d2dd8e8a106e596df24942eddff9c9b21945c330874a995e16e` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bus/base.c` | MIT (license text) | `378d928ca5541ea67328c3e9b8b4c649429772a1353986009793ddd08c42ab32` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bus/g94.c` | MIT (license text) | `c73c61574277761409f44dffe0f45c01a556c7ed68d80b638a344187f15e43dd` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bus/gf100.c` | MIT (license text) | `da4bead5e1f4c469eed257b8b9e443c15163c2a7d6bbee5ad563745370c2d336` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bus/hwsq.c` | MIT (license text) | `5a6e9ff50e363beadb171b1e436cec2521047fafa67c1d338e5fd7e79f83cdf9` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bus/hwsq.h` | MIT | `38aef930dc4dfcf9c739ecb47134aa609161bd7823628da20e6a5ad20591b690` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bus/nv04.c` | MIT (license text) | `6b15e207f9717fa5d47f60b2dfff75702114c9381c5a3da915a9abbc4ff93eff` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bus/nv31.c` | MIT (license text) | `e2354e16c8fa5ffc8ec019571b7c9326e7d6ea92f3beb4b4226df5eb177ec8bc` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bus/nv50.c` | MIT (license text) | `bf887f20d1633e6809d5046bd2edab81a98ce612c5e7331af88b41a68486f9f7` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/bus/priv.h` | MIT | `01a3c6aec6a1faeff15d5998c63892461f0cd91a7c53960c8269044c2c5a9373` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/base.c` | MIT (license text) | `0e8aa2119985fa0bfad19341f45efdf2fcdebd041ffb924c6d08c0eeace4fc72` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/g84.c` | MIT (license text) | `239cfd4cd0f9900b4b7326ce9134e4a860742634c2a833c886cbe763c01bf1b1` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/gf100.c` | MIT (license text) | `9dc3667bcfdc5ddf9c6fa3241271de495775772e9da0cdfb87fe5b7fabb74f9b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/gk104.c` | MIT (license text) | `af9876d0a2f0d3d70212f70def2fc1b4ced8122ff444d70b17610549cf283af7` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/gk20a.c` | MIT (license text) | `81b4396c452a89f1d786b2faf2f5fe34743ad4dd71d9d6283607b1c07c4b6109` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/gk20a.h` | MIT (license text) | `34a061868fdc8921f66f056263f8fbdfaee655fecb587b4507014d04dd1e8494` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/gm20b.c` | MIT (license text) | `e347f08bd04cf4fa21cc1fa106efccc4d1b01643bbfdd48b678b8225bed58fec` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/gt215.c` | MIT (license text) | `9db89679364bbe144eeae54f8a3b74dea741f24ba72f14323b62ee73027bd5f9` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/gt215.h` | MIT | `100784d9e292b419e94cf2b7f159449de88c971fa4a47538ffd52b8ea4d7751b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/mcp77.c` | MIT (license text) | `a21bbf84b04b6f6e8cf7456f6e70d230c5502cae71ecdc37b0bdeed3f83f3040` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/nv04.c` | MIT (license text) | `f5384f4f3ebc12d611379dd5757f67908de2e5b04d8353b9c8222828390247d9` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/nv40.c` | MIT (license text) | `d6ee2fd432ad699a8acf2819e715789422ff1adad167bd2f69717fbfd03d9a98` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/nv50.c` | MIT (license text) | `9105147364a08277b354378c6b072cfb4234f92d0cb92228b2916975f959de6b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/nv50.h` | MIT | `10d1f83b7fbba411ca883a467677b8c2beea5d656dc3161f27c1d0ab36ba3cd2` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/pll.h` | MIT | `d4959459d743235e59db927a15a3b40b0867424f259d4d06fd2978b1b5c6524a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/pllgt215.c` | MIT (license text) | `71cc116d01df5ca95223afc0c07b634594e412c1bd9aa5b2e4348773ff3b2950` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/pllnv04.c` | MIT (license text) | `2e5f1b4105753a16f38b1f708b59d29653891330ad3ec517652a2deda83b55f3` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/priv.h` | MIT | `0e7a0f4032cb79923b6574ec55571ebff29784ff1b74f037bb82534d32496b9d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/clk/seq.h` | MIT | `32cfd8e47b0ab9ad8249c2a819cb1b4fc7a1ee50a114120d187638b4896fa66a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/base.c` | MIT (license text) | `dbb6601b52047d86b7d3a410951708957254a59c034df56640ff125e0bb2abdf` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/fbmem.h` | MIT (license text) | `aacd6cead6a8b6716ab8c3d99adad6c27fa68c6ae95ed427dd742e0c8405351d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/g84.c` | MIT (license text) | `b2d91cb872be0307f713a7bd280d8d262daa461b1b5548cb2eb8aff2cfd780e6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/g98.c` | MIT (license text) | `febfa5717c1faf3bad48f8bd962ac3c48d8d14941131e28e7bd922fa199c8e56` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/ga100.c` | MIT (license text) | `1dfcc505c8d12630edcdaf33c7db18134aea2396e5593f95a1eb885282b344c1` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/gf100.c` | MIT (license text) | `260e57fd22b56e7f765b26ef014900e8e2e43e0fa9aac750734ffa7e9b0872be` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/gm107.c` | MIT (license text) | `b3e922f9b2c6cb720e473303be2c9f74090015b35a0493969c697a93083a0bcc` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/gm200.c` | MIT (license text) | `f3104d5d4119d0e6935e688e83396d04e7381d21971af462ca17f9df447bf192` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/gt215.c` | MIT (license text) | `18925c2ab67fcd7162b386788ceda34f22f89c90f7939f61be2a4c634ea683f0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/gv100.c` | MIT (license text) | `dcae1e7da333437224132fd457dc1d0b1cd191eefe688065b8957770c5fe08ea` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/mcp89.c` | MIT (license text) | `6e1331e067b4d7b569059442c24a2ec70faf5f8d97e789619c887f337634ba12` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/nv04.c` | MIT (license text) | `2cc13bb52d29ae20fa922298956084678d6303dd03843f01a8429bea98cb9444` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/nv04.h` | MIT | `37812f5605f609c6a847e75db068a8deeefba2257b58d611d2090c2120c879d0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/nv05.c` | MIT (license text) | `0e783132f8406a935878780be4fded688eb2c1bdfb73e83604b7a01e6fb1249a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/nv10.c` | MIT (license text) | `ce1eb60542ddebad877b14a36348e5b7a7a08978941975eac90b22f674213663` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/nv1a.c` | MIT (license text) | `4a561ebcbbba3c069ad4ff8c71c4d2ec9bf8309394f8ffe59b8f43ac28add0a8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/nv20.c` | MIT (license text) | `014e270ff390639118f9b00b66c6c5864f6935e854721c92038b57fb749eef3e` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/nv50.c` | MIT (license text) | `cb2fee19959d160d2390bf467476e3ac12f0dc7fb6d25d5699e627d0f2403335` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/nv50.h` | MIT | `cfd63de80cc2d4f63bd8ce629b8642645816f4d993104465459dc9dd8eccd0d2` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/priv.h` | MIT | `15a002196bbca73c967d9cce7ff705895ab5693f1042df36cd9608697f9625b6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/r535.c` | MIT (license text) | `0cfbe5d4ff12eb7955fa84f302cb2f1584157f91b09b6bb2ad6b899a8d9f858f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/devinit/tu102.c` | MIT (license text) | `4615b4adcfd9d662f362e2fefc14ebc7f06c7719a6110c6d4a42c1a19117993c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fault/base.c` | MIT (license text) | `5dc90eebe49b2c20d67e4591641dec0ae62382ce5c225849ba1b4306dfa70d0f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fault/gp100.c` | MIT (license text) | `b72440a15e1c17e8267d2f7c2fd55c4897a107a801f572ca64b9f1b6f72eb8ea` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fault/gp10b.c` | MIT (license text) | `88f1d834353084c5ac5a62263c894919db6284cf6b4ed3f3ae699c4bb8bc5a4a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fault/gv100.c` | MIT (license text) | `10062d3bcb370a39dfa56f4fca675b125de7c03550316813654acdc794febd8d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fault/tu102.c` | MIT (license text) | `40e170ca5767bcd3cb9fc1204979b65e876ad7831d1f450230c9a91acec9e74f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fault/user.c` | MIT (license text) | `c9d1707e83cb9445361514fec586009ed34e551ade1ec7401c998bc79815dc67` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/base.c` | MIT (license text) | `0c0e5748c7502a266137703ec53a5614ce8897d066fd614416ba7b0cbca83f76` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/g84.c` | MIT (license text) | `83f9dc9766954ca117ef4f9e433339aa77b2ce4fd16b56c3f8f3b681cddd75f0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ga100.c` | MIT (license text) | `57bb0ab42a876b2cbed4a418de12739ee7c253fde1d98dc26c115d9d902b97df` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ga102.c` | MIT (license text) | `a13cf345e9e4a6f90a2039873646f94675fff24e5b777164c9de940cc2ee36d6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gddr3.c` | MIT (license text) | `c5a672f84644e007cb93762ae34c56194d79fda4a4932f970def86eeda9c0a7f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gddr5.c` | MIT (license text) | `c8afce98e206ebe8352f90529b805a9ea157ebdd3e976b7afc3170be8dcd331c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gf100.c` | MIT (license text) | `b0d9f8d6fa7f6b7b0c86e7c1773c8c4b77fecc4d0c23a68463be488c50dc95cf` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gf100.h` | MIT | `d6d95e60bf8b566b41651724935f8ef827c9c7e1abc8d74fa4c5143b2b84ddef` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gf108.c` | MIT (license text) | `dca0d7fb2bc69bd4b30076706e0a9f94e52babd01bdadd187c6dac1a5b6701da` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gk104.c` | MIT (license text) | `30cfcd859f64e348ad6ec2c639ed06b40e8a13d5b16a25074f1188eb774e8b03` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gk104.h` | MIT (license text) | `e407dca6d8879fc6648bbe03d13f65274fe36b6e77ab1aee1a8cc6661aa11ab4` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gk110.c` | MIT (license text) | `b63023562a128a6a2e6c641c1b870b2240fd419c7def1f86214a33883b77b3be` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gk20a.c` | MIT (license text) | `5debba8052c20a3b7c84348780922ada145b88ded6f3b7f2e1919a61c302ab29` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gm107.c` | MIT (license text) | `a248be31e56789aa24cbd546f4664aa49be4fd625920a2d99e1474764f8d6a9e` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gm200.c` | MIT (license text) | `f538ddc71c8d187de0cefb36d538f9b7324641aa195631bfe658d7063bcab9c0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gm20b.c` | MIT (license text) | `22db4ad83d5af28b47183d5842ec1e0acaa7c53ab6c610fe629c216f50d46119` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gp100.c` | MIT (license text) | `7b1e208cac78d3e78b98fd8434965b534e1453aeea300fc21792afc2ac590545` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gp102.c` | MIT (license text) | `188b2b89134544fe538baa0722daacd5a5a92c23b997b91e100ec31a6e939a59` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gp10b.c` | MIT (license text) | `6b348aed9c149d160c218d050e6829ea65c278895e8572b3c856b8c1493d2c1f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gt215.c` | MIT (license text) | `83351a29760d4d07d27dbbce81d028a410a55418e3b4d2b68d9dcbd84bb79211` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/gv100.c` | MIT (license text) | `c44075b58962fd21bcc6edc91d1773d15bf1a523d839bb4911554534be7fe77d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/mcp77.c` | MIT (license text) | `a62d99f825756ed1209f96213de010fb136966b9a2b0c4331f11008b4a2c7adb` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/mcp89.c` | MIT (license text) | `d8596a232dc56d0bc637a9c16e6f7f8e0bb6eb9e12a401624086eb27b28dc3f0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv04.c` | MIT (license text) | `0f7547fd32c621e794d4b0928aea0c18c1ed9f85c7c7d6ace8ff283436a17633` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv10.c` | MIT (license text) | `371aa90b7d8ea58dd2eb0e86712f11b6171585bf33cf2f15701356a26fd63f46` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv1a.c` | MIT (license text) | `d0dfaa8a754de7351ab521ecd977fd9004d489f84ffdb410cf033342e4825ab3` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv20.c` | MIT (license text) | `2625a23d6c3ae4bbd8ac9b96d1aedb15da385f45dc5e57c5d827c1209a5316e5` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv25.c` | MIT (license text) | `406188c4168f6c60da8392e76cc5538417cdca249cc36c97f1e95e5af5035880` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv30.c` | MIT (license text) | `3643ce659c8cd23c15c0180b0762788dbd37714eb0e0863f9304887816158e33` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv35.c` | MIT (license text) | `93151abed9eed7456ac3375f5fbf6561bd603e87315fcf797499074485e3fa93` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv36.c` | MIT (license text) | `d031f27859099c13823fc153119a6a45e07ff79e63382ca90f6e55b0eb01a49c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv40.c` | MIT (license text) | `7a44dd7fab1500e0112571783ea8f3192fb29479b7f1f5f809218d99e9d576ad` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv41.c` | MIT (license text) | `9b75d29abb5f7749a67631cb0cb3a7d00b76811786ae73cba69ddf2830c0fb0f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv44.c` | MIT (license text) | `a62e7c692ef9c50e776a42377211bc736df5b43f9fd930ab420eabfd0feded0c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv46.c` | MIT (license text) | `5b14e692f228f828ae6c2266f060dd83b9aac3ac5b6cb33524a92f3759cc5a8c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv47.c` | MIT (license text) | `0d486c1eded797f77ed9831ce214757768ec28d4b0ec8667f3321fa3db56450d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv49.c` | MIT (license text) | `d15b1071f373975c63750e24282b2e2c9ea59f90e00b3f88cd58debe32fee432` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv4e.c` | MIT (license text) | `2666b69a19f062fb7000979a89d2eaf6370bed2da4d0ff2466a3f9d33ba6e1d6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv50.c` | MIT (license text) | `b49687c35287d58407483ac0c8774cd1d1ce5ff965d2ae096ec19d6621224f38` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/nv50.h` | MIT | `2b5e45f499c465ad1bff5709219626fa0b173f2c741c808a69d79fb4a212bbe3` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/priv.h` | MIT | `e374fd623d910e62a19357def6d5f86222ecac04c6f25879e9f3178a58495a13` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/r535.c` | MIT (license text) | `8d8c31fa93751a2950d1ea7136f04b05527b11864d1cc4ad38dd44e44f947723` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ram.c` | MIT (license text) | `a0d8b25ed1e8ed7b731bfb54d1db9fd4de5506750c1fb2274c951a21a4df3a8d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ram.h` | MIT | `91e70223a13f09b54be5709f6f87a8be435ac6408fef96d2dfd6557ff85015c3` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramfuc.h` | MIT | `9ff7f56afbac1741123f70f1c3b4af58aee55dbefc9dabb2b83015baf5aa34c8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramgf100.c` | MIT (license text) | `770a57692580c709223bd235dd07ee13899ff1c6ad29751c52dedef42383a57f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramgf108.c` | MIT (license text) | `1a59d714e34b6977ba4e551216810721e234f9e0653f9c36563fd022007471d2` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramgk104.c` | MIT (license text) | `99aa20855f0029918873fe0e598b341a91ad5049834df4b98033f34012fffa45` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramgm107.c` | MIT (license text) | `0a35f6ceaa6d61f8215447e580234b6e01ef8ab079853d93532eb572404ddd35` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramgm200.c` | MIT (license text) | `da8f6b4debc67f7414d0830b4fc6244dae63dc63270f2e7fe49e8d27839309dd` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramgp100.c` | MIT (license text) | `2ba623f4498ce023dd69a8801bcf694c35b7ac22c3a4b965805a8096fc4b396a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramgp102.c` | MIT | `f97378987e3a15d2db7752396c2e5713f156303bb7f6bde08d5d83a5ad3d7918` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramgt215.c` | MIT (license text) | `93fd9ff367fadde53a97342485ebf65ddcc3ff7760874806c7459fef83b384f8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/rammcp77.c` | MIT (license text) | `23abfb24da52ec2cb18faf7e292d932cbbe09ede6ce67867418878cfeec75908` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramnv04.c` | MIT (license text) | `a460003d80efe37d615b83c53a24235288bb045803018235dc2bff31246a0776` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramnv10.c` | MIT (license text) | `818b06e5dd87f2915f2eafa1ee3ae9549fb1135ac3c2ed410acb20197bc4e812` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramnv1a.c` | MIT (license text) | `18bc38592868449a7d13bf86260bc8f66489a3240a8b0ecd4600374c85fa3a24` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramnv20.c` | MIT (license text) | `f01952de9405054486646b2116adf2999e5daf5b8ffc3800ffc05b5269f584fd` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramnv40.c` | MIT (license text) | `8ada5ec66ac3fddd2c8412fde095c49279b7cc974fb7d6ec9b25e00c7726963c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramnv40.h` | MIT | `ce615b1c515d3d3edfb773aba18b6142c1dc760a8466e87322677bce320e11e2` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramnv41.c` | MIT (license text) | `3cee54c49fcc414434611bea21150e579aec0ac0ba4f27a04263ed523eeee176` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramnv44.c` | MIT (license text) | `c56434b81d13fa320f9fcd75ce793928bc818e8ea589bc5796f7a9d632bee58f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramnv49.c` | MIT (license text) | `9564ff0dd6cec55d8473c27627663c0d79777c83c9c55fdb60a8e8c74c12f9f9` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramnv4e.c` | MIT (license text) | `8ba3e983d1e0df0d411f146c54fce4380ff354fc63eff832803a65eb386b75f6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramnv50.c` | MIT (license text) | `913fd8deeae9eff46f7f58367adee225c9a656c22232589baf2932de8c3282e5` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/ramseq.h` | MIT | `3eadd4869784281a91eafdd4b550fccab07a036b791a3caae742df6a20406b21` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/regsnv04.h` | MIT | `47ae5d5dec8a4d634ce6b2e81b0ba722177d514bcb03f7e18d31fd6989d4690f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/sddr2.c` | MIT (license text) | `f6d55ff63808f7a3b2181e23097f3f47603f466d595fc8d4a5380b5f37d69f37` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/sddr3.c` | MIT (license text) | `84de61ab6a6af2d66683f0797cc9228760f117f107d5add352c2c6839d94b8e9` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fb/tu102.c` | MIT (license text) | `7853313980448891be82ce13c6dcb438e78f052dda64be69ae354944146ab161` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fuse/base.c` | MIT (license text) | `f94ee76d41d8f4faf616d38af909779269ed84b7a71738481d3421c5c786ec43` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fuse/gf100.c` | MIT (license text) | `1ef5395be6d65aebe99cb35c6850de814df178713cee8bfcf21e57cdfd05b0d7` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fuse/gm107.c` | MIT (license text) | `d8f5389ac73f4433457b13784011edd66c2ad6eabb98a2c06fe17fb7c0ec4a5a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fuse/nv50.c` | MIT (license text) | `4507e9acbef5e8419ed11e44e4aa781b1f5470c7fd9e3774f63a160c7703c08f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/fuse/priv.h` | MIT | `2f0382f25f8c2b6b1fa8bd91028eb4ff3bfa167499c2214f76d3eeaa4f6e253a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gpio/base.c` | MIT (license text) | `24db00a248f0f0e7e5e0a7a88b1551e04a18e56d02ecef825b93cff407bb1a94` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gpio/g94.c` | MIT (license text) | `7f1a5ece832e0c24fa9b1b31322ee6e0a5d3a47753bc8be98f0ff0fd2f5603ad` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gpio/ga102.c` | MIT (license text) | `85a8e7f51d53514a6d83a6fa7a9cbb847e8924301efe4f01674435260eaa321d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gpio/gf119.c` | MIT (license text) | `8c82fe870d0c38c2b741b6b009c9b7f6bf7b47321bd9d53edbe9c4b6908d7ae9` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gpio/gk104.c` | MIT (license text) | `a10922b3f25de92b49c38dc299c031bab92b683161dfde852ad19d396e29bf91` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gpio/nv10.c` | MIT (license text) | `547c203d548bcc705ea0df50bef2844e84faf6df4a369ba83b8793740f97b4a7` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gpio/nv50.c` | MIT (license text) | `76b3adab2e632b65ae75d00fdda2630c1964677caffd8d122d2590d2d100698a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gpio/priv.h` | MIT | `b478e04ef6485eb30c5f8a4a0bb6c05e7b1faa040601ee972fd01611c7ef78d1` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/ad102.c` | MIT (license text) | `a808946930ac4308351ef2f1c7dcdc6d8eda5585916e1a5408b6841d948927b0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/base.c` | MIT (license text) | `248ce2e65360fb37dfb7036c271bdf90f6bf9ff11db514470da89c7c185aea4f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/fwsec.c` | MIT (license text) | `982b707407a925b09cecc0bc1fce71d257078af8d30952559316ec0669f26420` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/ga100.c` | MIT (license text) | `55f51c494947eb8949b7e2aaaee1d3b21aed62d5028dd5fa4615687051eb0802` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/ga102.c` | MIT (license text) | `e62fa82881119c2a31ae3c3e9b7b0e9c2129edcdbd4dacfe5f14d69a84056e06` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/gv100.c` | MIT (license text) | `cd1bd0e8076c13269e97322bfd01308febc7dedcd3ff99c1dffdde8f38e00959` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/priv.h` | MIT | `8db6e067037f627588fdbaf4980249305c64b0b752e5b75d09e80fc4dbceb78f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/r535.c` | MIT (license text) | `a4e9d67ff4f2b65c2e532597769746ab0167855a5ef5c30d8a6e955b1088f5a4` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/tu102.c` | MIT (license text) | `734d509f9e3f364429b62ad3d70fc04b055e18b0f7693809e2bbff98dfe83d43` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/gsp/tu116.c` | MIT (license text) | `cc4790f47e060a84438db41891c4f015665d3d3cac81b4c545ff413571d3f5fd` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/anx9805.c` | MIT (license text) | `0fa5b36e88c5fad9cd1ee02d3b4eb8c37779fb2583180b2af5f910ec05d75655` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/aux.c` | MIT (license text) | `efe0811a23803d316d82ab143d2a50b767087211d31911fb1da9262ffa25a8b0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/aux.h` | MIT | `2ef9c73c695bd389fe66047686310f2678ec4e20b42f97609f278f56ccf52859` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/auxg94.c` | MIT (license text) | `8757eeb5bddbea878e8faf57b83c9354ef228ee07ad9eab453f59a7dc986cb68` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/auxgf119.c` | MIT (license text) | `4bf390e81898b62c7275927265658cf2be920935245e45462504306883bcb436` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/auxgm200.c` | MIT (license text) | `8ebf4088a302b6ff9fba5ae1ed117ce5fe5c25745ceac78a94115da79b4259fd` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/base.c` | MIT (license text) | `6ce5d5f805b35ef7234093d3dfbb74d74519c324cacca88497b980aeca190d93` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/bit.c` | MIT (license text) | `b7086513e88715c1c629fecbdf3387836860527663039c7146fdbf5cd255944e` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/bus.c` | MIT (license text) | `a4c6250334178c953fe77c951ee25199b8e652af584bd24f800738d5e8a9614f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/bus.h` | MIT | `5a41e4e51d233830d0833cb2fb7b94ea0fe3b3b15c8878a8ba16eab9a1c96a69` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/busgf119.c` | MIT (license text) | `24cdf28deab20486c48b4702c473375b6d91e4e6a0286475e5f9f48ca88a17f7` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/busnv04.c` | MIT (license text) | `444af053b15a0ee0d6d73b1c508ecbe024c544a6d61d2ec2fd2cb473c242ec1f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/busnv4e.c` | MIT (license text) | `c16f64768ddc9755e3452f076fe6060f89ebe8ee03751b6f4ae197aa642f0999` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/busnv50.c` | MIT (license text) | `252efd46c3f029780952f3415ea5682a4a4bc7a3f90fce4e313dbe0133d3f932` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/g94.c` | MIT (license text) | `1d19c8f0415f7ec287a14e0539d0a9bc7e16f7fc51f19a98ceeb06a0d23a4f9e` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/gf117.c` | MIT (license text) | `0bbcbf02f296a940b798cce1c47de3df47daef10b2391e3b553cbcef8ee8e543` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/gf119.c` | MIT (license text) | `302a19a01bb1c16c57f2e9de82794c85f1a5af8c3a639ea30a8440444472e294` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/gk104.c` | MIT (license text) | `eef05ffd3972d31e5291d97262bca66685c16d2e0dad6b1f58b875a1c3903901` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/gk110.c` | MIT (license text) | `a43c7562119b983791781dbb2be51314e9586633260350f1d50c660a88e40c06` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/gm200.c` | MIT (license text) | `d18de441a9c3e5a35b667acb4071c317d299567b153169e3d7909ee896cb584c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/nv04.c` | MIT (license text) | `3284f779e06a11957d6a46c8c8cf9acfe861ffb3c9c019d7e9d722bd43e48bd0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/nv4e.c` | MIT (license text) | `2293e50e57c92684b1e2a214d2bbeed22d1dc069564b089cf8a110a04339ccba` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/nv50.c` | MIT (license text) | `9617d3429ae08ebcdaf178ac295e19573deaf160b51105ab415bf9463b38a57b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/pad.c` | MIT (license text) | `1eb962cf9488f29e051ff4c86c697731d07c9e5b3e90b2b4a1f7a641721bfdd3` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/pad.h` | MIT | `369cc265195b6e695d3460aa4f279229985ea79b689d0017d6b6f8177417ed37` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/padg94.c` | MIT (license text) | `47680e51b2bd1f34473a898eed4f862ef930afafa7433cae214fa5645da9cdd3` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/padgf119.c` | MIT (license text) | `494eeea1b2b4418d2fee0e1e300739eacd1954d9d7c8459328fc370c9bf247fd` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/padgm200.c` | MIT (license text) | `5bdf6b53f95044eeb2a4f8ec93f225a217a157096485f0ad2a445f27ef3a0228` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/padnv04.c` | MIT (license text) | `970a2c48db69c7e1e425dca324cb833df0578ec61a40abc2d1fcee1f98247097` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/padnv4e.c` | MIT (license text) | `54eab5f31d7079d8c21afb4e15920bb16cdfa7ec8ddd2bca4886557208cb1ea0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/padnv50.c` | MIT (license text) | `95171b9e9abea7bbe98396a7c7b4017e4d7601702fd80b274b34ecd02cd020b8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/i2c/priv.h` | MIT | `4a124bd3842ea8b44489b6dbeb482a969c522f7c531446f8ff68fd36d5b0465f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/iccsense/base.c` | MIT (license text) | `16e8e6af7bb3025491a7898039392a590d80a1a94ae3e6f0ec816236920e1567` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/iccsense/gf100.c` | MIT (license text) | `e97f47df06581302d2cdd0c26fa641e8c738b9ae302447a4ad3321e90b4b528d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/iccsense/priv.h` | MIT | `a43a66656e6567634489e35e314ef70451c8ab6ec7609b77d1c445e148c81e6f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/instmem/base.c` | MIT (license text) | `271c451d5900675e99b3c8b0fe52b978d8fdec74225d3ed25f21d7e3f22a2b08` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/instmem/gk20a.c` | MIT (license text) | `d61bee557fddda679108cb4e7bd94b80e836be6b9de4895ef245fdab72183705` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/instmem/nv04.c` | MIT (license text) | `38ac11e45b1ff94b83fafeec6e4dc46b863b8b094ce36485e2ddd9a9a3e0fe9a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/instmem/nv40.c` | MIT (license text) | `fabd49267b9bf101d3bb32cf3a451b2876a427c4c17fcfc29d771ff21abb8e54` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/instmem/nv50.c` | MIT (license text) | `0fd5655d95638a3df042c8e42d064e15be8cf44bb5211f4585dd4491f60c1b96` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/instmem/priv.h` | MIT | `9e9cbe13dba21a10b632d8b0744f04e886d05db83a0ea23c57578bad5952a486` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/instmem/r535.c` | MIT (license text) | `277b16c94cd39b3182fdce4b1272d03781d08073467b0b67e67d7a011fb9acc0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/ltc/base.c` | MIT (license text) | `480d714348f3bc9333b8c8aae55ececa86ab29461ec32740d3f711cba3579e9d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/ltc/ga102.c` | MIT (license text) | `b3fbeb7cc57a683fc65937446b54be9936cc72afd5a961f4b43dfd44301bcccd` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/ltc/gf100.c` | MIT (license text) | `09eccba3486989365feed0198043f165b944bcb98ca465386047f428d34714e8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/ltc/gk104.c` | MIT (license text) | `b3dc805cce31b0fde5b332119bc4061052bacaa708d6c0b46f90053ef296a2ab` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/ltc/gm107.c` | MIT (license text) | `bf7d9f46ec6aeb4db27cba9084ee1c5424a98858d55e912bb0721dc9e3e68ec1` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/ltc/gm200.c` | MIT (license text) | `7bdb624d795e547f4342316164e22bb737a65c2d8b1b50f56bf8d182f875b709` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/ltc/gp100.c` | MIT (license text) | `1e3e1d8d7948bb009e218e32a7385bbbae4f274906eb0830f8c8d7cddf5515a9` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/ltc/gp102.c` | MIT (license text) | `40ba329254891ae8b6518a547e51b45343f22123f6a388a156e4816e34b96051` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/ltc/gp10b.c` | MIT (license text) | `29880aca05a4c16298de67c64b548392640fe4bb3de896e9f97676c425f5952d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/ltc/priv.h` | MIT | `410f5ae2e47a49601097ba8dbec9ccf3975fd1faf2350567f639e725fa7a5413` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/base.c` | MIT (license text) | `aca570e3b6d297b7d0bf7997aac8c6766c09d531da47f066d814540088a81171` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/g84.c` | MIT (license text) | `af1ff0565c47ab7f3488155760968311dd777af889367e208e9e47f6bd5f4d8c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/g98.c` | MIT (license text) | `c080c0841b414cb2e867a5592d65ed2ddf4e44fe251b09c7fbabfa834484fa89` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/ga100.c` | MIT (license text) | `c82aa33e7143d372cbcf0c7910b808d1080b91165da3df304765bedee567a36b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/gf100.c` | MIT (license text) | `12d9ff2c6f66691fff27c17eb4f9c41b0ad8ed1d30ca7409c8d62d2e88f851f6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/gk104.c` | MIT (license text) | `15dd9fd27d86890a2d5cb9278903a3f95267b3d2743c626e59f6351c95ea8d7a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/gk20a.c` | MIT (license text) | `4e6012ed7ae8983ef8bb9aac34356a8a11890d9a4968d59ada49d77c7477c52a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/gp100.c` | MIT (license text) | `0d5aca122ac4aecea77c668924bbaf491b642e7c103ce4b1d33d00e0289ab626` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/gp10b.c` | MIT (license text) | `f72356336b937c8302a27d319ac329c16c730ff9a6313cb992fb95e4318d6217` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/gt215.c` | MIT (license text) | `e6633cba4d87a451a6a6bf836b154d777cc972ad0f8d2ea4113e02ddda8a1f76` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/nv04.c` | MIT (license text) | `bdf95057dc095b6e0f09fee47ab8d461b0b60b3af153edd4c3325955a5fc84e2` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/nv11.c` | MIT (license text) | `773b29846a692aa8f583f672d890f1d76a27e25356c8ec00b85e8ee19416a931` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/nv17.c` | MIT (license text) | `a901efced2b0a503ed035e5299cfd3540035e39d95f1d3d64179fdf7784de053` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/nv44.c` | MIT (license text) | `cb12851ad96bf002bd029544d99773b35765bd49f245082d34bc7f7e1f893fe3` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/nv50.c` | MIT (license text) | `d79576548c511b3aee29139e80562974007c179db370499b4a0221d2eff37576` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mc/priv.h` | MIT | `60852055ac3415eea4be4d7285a10e6b3eca96f6f8c0b30f954be1df334df0f2` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/base.c` | MIT (license text) | `b65738d4e444562a73546405b7107d5bd3a9b48bdbf798fa28660fce39d0381d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/g84.c` | MIT (license text) | `0403cbe2fe904fc20b7e4cb7cbf4ffcbcfaaa59888195b615e7650971305ddc4` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/gf100.c` | MIT (license text) | `ef1bf1d79658302c6e16e1f0a71e6c7f2e944fd25487a68d5477717dce8e7439` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/gk104.c` | MIT (license text) | `0a00f096c8a9f95a6f72a87d72d3868f31b22284823b842e57b83845da88492d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/gk20a.c` | MIT (license text) | `8b45e88b96c39eb371f4738757e59bc3833e851395d8527cb5da21798632f9d5` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/gm200.c` | MIT (license text) | `a77b631bca671989bf2ed6182f7b29545aa95fe8cab384488c3c8406a871d02f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/gm20b.c` | MIT (license text) | `d8135fbf664afc158c7b4931fd5f4aeaffa68f4130ec95b73d7af75b59fcc7f6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/gp100.c` | MIT (license text) | `89c06c0b692d89ac2efe280148aa317d017ce915ab47267dab02f7a2d7dee909` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/gp10b.c` | MIT (license text) | `12c3978b307eacccf7e54d418aad964cc2af8d865f022946af088672fd643a70` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/gv100.c` | MIT (license text) | `0f6eb608eba59a5140e5b2af7a52c90a50a292e81cb3587e0fc13c8ab97ce9fb` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/mcp77.c` | MIT (license text) | `fd3902cd1ece9034d0c43ee238a49ffe3e4fe0652e4e8a63d79fbdc395e37340` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/mem.c` | MIT (license text) | `c6b92c9ffaa85c999df2e3c8a8218a510bd0c94eee935c59266eccdc2947423f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/memgf100.c` | MIT (license text) | `c41cd05d55c3d76adc21695f51a7b2f47f68d3bda13deb0cc8959dfa60142120` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/memnv04.c` | MIT (license text) | `aa0479aa4a52b50f0cd757c6437fd1ba8a4eb208b2e6444d0da85471c7a9a254` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/memnv50.c` | MIT (license text) | `6743b2e37f5b0e00878bc54848d507d39d6aba8e24761c8446c807e55f51a71f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/nv04.c` | MIT (license text) | `6db6972afa4d7cd228792858ea15b58e823c55726d69abdeb993fe8d7d795d78` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/nv41.c` | MIT (license text) | `25f1b0cde7325eb78bced6fd1dd2c0ecdbb09c976b43eb51e480e6524198d6ed` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/nv44.c` | MIT (license text) | `5af288b6e1bae28a0597c3ab4eff6b98c2c202ba30b168b04afbc57cd876fed5` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/nv50.c` | MIT (license text) | `17519ac3a232a8472761d5a70ab3bd5875f00aa833e839925794c0ec5aff103f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/priv.h` | MIT | `3591f344354998b8185f06c48b6b61bd0a56580d32990c1932879e4caaad45e1` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/r535.c` | MIT (license text) | `14efdf3a261f1944db9deb8e80b14454d5a5dce630f1b8771ac67dc44aab55e2` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/tu102.c` | MIT (license text) | `af4fa274fa7e244ac975b6fa21715d1d0d5c0e8cf2cac2a50aedcaa9e905d9ae` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/umem.c` | MIT (license text) | `413f3a2065acdce0eeeba52e52c459099f333037c1fece05c0aab3a945f15be1` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/ummu.c` | MIT (license text) | `50e227c0b6a401217125d5ebccfafdd930e86af58c6ae657c949424e7a2121dc` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/uvmm.c` | MIT (license text) | `43c6a9fb52906ab67d0b582d4501152b74e6ed1cce6587d692ae9b066fd13e67` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmm.c` | MIT (license text) | `a8fe69633d449428df6452dfeeeaa436b35a3608916f60a07b220e6b0603d639` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgf100.c` | MIT (license text) | `8485236819c37bfcbfae9824c4b03b767314b6e07156c3ed2e203f2447ad89ea` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgk104.c` | MIT (license text) | `3f77d94f4f08546da6361e474c94fe13af23d41758ad5a1fd48b0e5a50ccdd97` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgk20a.c` | MIT (license text) | `16c59ddd6734a76553060ab23d2b701ab01d89837e40ebecac5504473b685358` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgm200.c` | MIT (license text) | `22196cff4fdcb57458b568f2b2b20fe13db6bf1bd097413551c31b23ed6e0361` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgm20b.c` | MIT (license text) | `42eed7c86eb0e489046235127399e3e036deb23ece8f4b976c646281b5e9d661` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgp100.c` | MIT (license text) | `08542f788e9d85147ef1f830b406bb85e5a47c0d63b87f3b901bc398ff100ce6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgp10b.c` | MIT (license text) | `86c4a3ccb0274269264fec8a9ccb5210fe9bee0e397fd275ac209e97417d279f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmgv100.c` | MIT (license text) | `ddf0be77a946cf03c0cab7b9414aa65860f5aef4e6be36d65fac551651a3147d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmmcp77.c` | MIT (license text) | `cf7565b1f4703030147b10a9ba06e1fd64a75358aa37da57f43d53e21b8c9527` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmnv04.c` | MIT (license text) | `070fe9862006cfb39a07ff168331b9fd0d11413a0ef3f09c0da8248f24898ae6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmnv41.c` | MIT (license text) | `12353191968f7feba35db9cb76d9fd75f0d185e4701a3eb214b52ebf335218fd` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmnv44.c` | MIT (license text) | `f34f463667c362beafab1ea0237ae6278babbd64f4d4e52a995273b6a0cdf30b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmnv50.c` | MIT (license text) | `1147adb832085666cf12ef312f716fb467f86292fa1fe168acc49261db21aac4` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mmu/vmmtu102.c` | MIT (license text) | `0af819db3ed59dfa82a0d66c04a286472312795240fdb360cef4f3edd8a09bbb` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mxm/base.c` | MIT (license text) | `d2176323ae59824da62dcd883c0c4169a6178c609b579c2c43b7efe32abc361a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mxm/mxms.c` | MIT (license text) | `b93075feffa3386bd93102ff00bc9c605b26c959aa8dffd201becd7c55b4fc29` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mxm/mxms.h` | MIT | `832e61e3c64b5c8829d6185500957111a837113acb7c2d48be0abfd1fd223cd5` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mxm/nv50.c` | MIT (license text) | `d4f96fa3b735fdb70b50022274fd2889af74225773fa31deecabd12c6354628d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/mxm/priv.h` | MIT | `a8882d246f3c637ba3f13cdfe07cb722a2d292b7df9287e3714612d255220e90` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/agp.c` | MIT (license text) | `cfb2a2baa5576031c8229fa3f6b03e69519a62e0a349e9c31dd643e140c493b0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/agp.h` | MIT | `459bae6f1764fdd7e98ad38568c76e9fd681fd74eb3d9046edc4107c092f9ed8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/base.c` | MIT (license text) | `cc1611a5d1b22109c19567066d6bf3c721e6785040cbdf2e6b1ab25a4bc681a9` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/g84.c` | MIT (license text) | `57b3b585b8e37f011481962c8d831068c7d82fd153b45abcb0334029c4e25ba0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/g92.c` | MIT (license text) | `40646971c46c640ba691296ff2f2b8f6c26788c1ee386e58351b82b120f89ad6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/g94.c` | MIT (license text) | `e75eda341c2c77138e4818c3a5b5c028e67e7121678ecb90e75a61e876ad65b4` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/gf100.c` | MIT (license text) | `9387c551ff3a68debedcd8d9a9df3332b990d817d47af08ac72e8bd3ca44a278` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/gf106.c` | MIT (license text) | `262ae1b0646ecc2c492fa84f650b26b101819d18c29d36aed50783e1d811a358` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/gk104.c` | MIT (license text) | `e318d432073773b7be8878a4ea200ad4f0764ed7c89d248713d46d66ed8409d7` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/gp100.c` | MIT (license text) | `6f66b0581ecfe0ba6845a8a4f72a375db14dcddb595faa708996a5a9b9d5a4a0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/nv04.c` | MIT (license text) | `fd085aac5d60e59de1e9d4865435ab82e7a55725c9900323198ff6d1b6f55af8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/nv40.c` | MIT (license text) | `ad3078ff848315ac81aed415b3f1452628ad65c271945311573b096820c042b2` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/nv46.c` | MIT (license text) | `48288a1dbca8f541f8b688a559eaae64de0cea17a8d5b4fc63009f67b8654856` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/nv4c.c` | MIT (license text) | `dc57ba587614a3e06294fffddd94e3fa050581f1035ba82ed4b67596ba88fa80` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/pcie.c` | MIT (license text) | `062deb6de1ca199d2c90343291d7e1e940b758da998b403fde69c8c049c93d56` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pci/priv.h` | MIT | `b53b6e6f817f8742cc6f2613fe61a8ebdb040a54a0001d3c991a727822446ed5` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/base.c` | MIT (license text) | `822bc80f760a5e329bde3cff7460e3f6647d080ba6d1d7f1ba6daf2a6518fcaa` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/fuc/gf100.fuc3.h` | MIT | `bebce7a910d7fdb8c04df0cfd9e8c4c3f7f8a8ca356540324a869e45fd08263b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/fuc/gf119.fuc4.h` | MIT | `ee3c298ed99d665fc251528597c6f8bfb978d529f478ed063e123209cf1b9c2b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/fuc/gk208.fuc5.h` | MIT | `a1ad178778e0646d5b3d9ada27a275fddcc0da39f1636c3aefdb39cf84383d81` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/fuc/gt215.fuc3.h` | MIT | `a90747e3c30fb4ae2b8865a2c0057f3541aa00d0d9798c14311cee013e1bdd46` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/fuc/os.h` | MIT | `c58ed3cc2c45673796d8bf56720da082e8772750de0ba585859d221568561fed` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gf100.c` | MIT (license text) | `459bcd9a396dbc27b0c6a9bcc9cae6cd5cc158ed17cac7dce1ec9ca0bb413baa` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gf119.c` | MIT (license text) | `f0b635efd3c0d7bca9f854a71fc635a8ace683f5a598cbeab7a674e4db09147e` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gk104.c` | MIT (license text) | `b7c4db6d40c76db816c4d9d35ba24fbb0a8fad60beaadeda9e40e2963e88c3e6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gk110.c` | MIT (license text) | `55ce3a1aa3468664da5ddf1eb8e86cdb975147d3d074e58a40281e1ff27dc4f6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gk208.c` | MIT (license text) | `a28cccfaa35ed74eddc961173b9ca34f43eb4a5029849d281e1a4302e6e501ae` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gk20a.c` | MIT (license text) | `c27610182c07bf0d8c1333e6bff9900b07e33e754efcd2719be2ea889019bea7` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gm107.c` | MIT (license text) | `c36835b18bf48fc1c8a26c4e96c6fa45627e9e86f593982e6fa74a4e78e29b4e` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gm200.c` | MIT (license text) | `809b96eadd579890043fe01cd2d6d7b246dbadf1c681f22dc68ac84b04baeb92` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gm20b.c` | MIT (license text) | `c8ddfc8cefea78d1cae86f7b7b6e1f21a2c8a83936b62dd910908b4d96491832` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gp102.c` | MIT (license text) | `d66ce8bc6e2e0cc337d0ed52dbf3484d840422ce0e75e167394bad00a299a2a1` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gp10b.c` | MIT (license text) | `a0c9d6556aeeffd6ccc7907d81ca9221edccc374d1714310d7075d13cc20a31e` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/gt215.c` | MIT (license text) | `6f89bca71e1059afbc4c852443ff679bb38e974461b505c6fb2cf254635ea455` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/memx.c` | MIT | `70822223f442a6c1e8bdfbcef5507cf0870adbda9fafd871b2332fdaa90b79e9` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/pmu/priv.h` | MIT | `7a2bbfe0084327de53740a7fb2b739aa9c0fdd582d5f3e07d0304021887412e5` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/privring/gf100.c` | MIT (license text) | `67158162179d09474b2fedcc57b3e19bd9dd6fa150fe8d2a89313fd2a07aade6` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/privring/gf117.c` | MIT (license text) | `336c2492f8384bce0c07e4af07d0bc52c3201423ee09e639cea7cf5c9860fdea` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/privring/gk104.c` | MIT (license text) | `31a036910540e89008201ac25ae18895b406236f5e704aba5ddc138482aa8735` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/privring/gk20a.c` | MIT (license text) | `6ce673d4d5a712d43e22b594e069539b72bdcc8dcef66fb1884295c5366c4a74` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/privring/gm200.c` | MIT (license text) | `57f62bf7ad85f06e2b9db357b6f806162efbc3b4ce96331aaac4e8aa7220a67c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/privring/gp10b.c` | MIT (license text) | `b95837002088916e2ff90f28e653d0a6df1e8e0854acb4bec2b4df3bde92467a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/privring/priv.h` | MIT | `a942d9b2434a6a5f25198c7a05969d35bc8e3cda2af57cdb472739f58414cf51` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/base.c` | MIT (license text) | `ac2b8c2b9aea906af341ac5553c7d3f4c7d4e75746803c660316ffc7669ab104` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/fan.c` | MIT (license text) | `d7e2f1718dd33cbd2fe294d6d963fa1574366fd4a39709aa24a140bae9559333` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/fannil.c` | MIT (license text) | `3dc47ca7911b5b4556df24629e2703517851d6d859622700a0d48d2878fdc9b1` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/fanpwm.c` | MIT (license text) | `9df9c976a873d69f73582a3e51a3fe2e580dfc4eeaa7b05a0228e4764ab8f4cb` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/fantog.c` | MIT (license text) | `0c831e1a66415267b34c72a5227757cb4d0f91cee72b61f4ca25049a22e847de` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/g84.c` | MIT (license text) | `168e720d9badadb4432ae7cae2675e243939f0ff4a8b0543f164fe02b3fe3fa7` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/gf100.c` | MIT (license text) | `9c881603c600e46b318fa9c6112ed7d6241e00eb031e75f94b349b8aa60c0691` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/gf100.h` | MIT (license text) | `48cc217843f6ef3e74699f85bbe020ce398034f1b81f07cdb281c79d3df56d7d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/gf119.c` | MIT (license text) | `64661aad08413eed48b00d877d6b815093a5d420043a8a3071a9a24edb439901` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/gk104.c` | MIT (license text) | `7b4a9f82d3f89ee71c634eb6bd3dce292a8d8daa72eb0358e32898e072912d3d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/gk104.h` | MIT (license text) | `7de2916d24012edc295848ac204badbd0a6c2a1cf39e5f9509cb1c950b525158` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/gm107.c` | MIT (license text) | `e177d06ff87fb989831aadf8121bfed77ce774eae32f6b61267a6b7184f494f2` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/gm200.c` | MIT (license text) | `6f5a0abb69049b99435923fab6e77389b333f2f948a6bdfdd19ba566ca2929ed` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/gp100.c` | MIT (license text) | `971670e9cbd72c70b4b43056360a276cd81d171c6aa179af71bcb342e0d7bff0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/gt215.c` | MIT (license text) | `5bcf298f06091c7d8aeb98a7f39e8f49e0d9b425696a836d8e10ebcd0369a5ca` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/ic.c` | MIT (license text) | `24ad2a794c28e4655f27241a3a9ba79fb564b04fcdfef918ec6d6fd2f593cf66` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/nv40.c` | MIT (license text) | `e72db8574c6808788a532c365e40d0316016837f0702c1291e9777415ade0a84` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/nv50.c` | MIT (license text) | `0765d1d7187ffdc728802fdb74765e465e7002dcd35f411013a54b3bd2529403` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/priv.h` | MIT (license text) | `34080a64e773c86b42af168f46e2f3e6190cbc2ddf1a186db06c58754b2913d9` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/therm/temp.c` | MIT (license text) | `2b5a08e64216514c6dee1db7a4ea3bd99ee997b44d77c71406ad72597957b66a` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/timer/base.c` | MIT (license text) | `b9dde07acec4980c8fc67de060d8b2e8c4eef0d641643244ee722459f5d50a33` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/timer/gk20a.c` | MIT (license text) | `025ff758191bbd9310a4d9f0353bf435f25daeb11014b391175d8920a0a03d86` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/timer/nv04.c` | MIT (license text) | `9ff42a08081ac418ddf60a7b888f326370e25c382dfdd02b8deca3836bdb2106` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/timer/nv40.c` | MIT (license text) | `209606e6bf72f1b1fcab9aa85f80eaadfd8b5132e3d853c2e8a1ac7502fcac84` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/timer/nv41.c` | MIT (license text) | `835ec735ff354bcdfe579379ef8b97e5db60864f51efa3fabf17b9f9bcfb86ff` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/timer/priv.h` | MIT | `fe9e0e5409fd26fe269eb45fde22dcdacebd285fa908087e849bdd9cad1c37c1` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/timer/regsnv04.h` | MIT | `56aa976aca8025429f502a48740aba19d37821a0874440f6cb0fa8fe060fa28b` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/top/base.c` | MIT (license text) | `a5384c4f5a920e457bbf70be2a5643328f4fe03ed2042f40105a4a48ad32f373` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/top/ga100.c` | MIT (license text) | `665f4e10f8519162581a887d470a7571569d0b014e3afbdd20bce1890c69c2e5` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/top/gk104.c` | MIT (license text) | `834dcf42fee7cb42442d5a60315f4bf8ab401add3ea2e0845af5b34566843b99` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/top/priv.h` | MIT | `6c7cc843d236e8133ad80239e66630e8339b6f27a21488fed2aa112db300d492` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/vfn/base.c` | MIT (license text) | `71c545f266ec902884f16868ee64e525e92cb3e3684a0a095930c0882784fe66` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/vfn/ga100.c` | MIT (license text) | `4d146125e50f7ff344926b722cf8065f1d9f3b0ccac0bcc00c55b9d41c087b79` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/vfn/gv100.c` | MIT (license text) | `de0655422d382dd17492697f720739bc4481a06dfa16e81c88606e6ad136a3d8` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/vfn/priv.h` | MIT | `331b3344910443156e2744df8bfb14f1d9a63dd0013fd36e43f82fbc17741a9e` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/vfn/r535.c` | MIT (license text) | `a09d372d54fd84ec3a69be26c80340265c5be5bd4714bf930d7480349e4caa0c` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/vfn/tu102.c` | MIT (license text) | `5299a6b1c4bc89b4d5bf43802e8a94645fbb85a2411eebcd38b06586ca77f861` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/vfn/uvfn.c` | MIT (license text) | `7eb511021fcd95f68e2e3a9f64ad7f2341225eaacb21dd4793f4ba88ac25eb47` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/volt/base.c` | MIT (license text) | `c0dd9edeb0ca4c3fa8369c97774c14ca9185149fdacef16787c309e1b5b85cfb` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/volt/gf100.c` | MIT (license text) | `7c1c2b9ec98063f43ce6058dcb4839bb5000987a4d1b3e280438881c451ffef0` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/volt/gf117.c` | MIT (license text) | `6b64c8a82f0a1e4f9d589b2abafdc1806075308e118c2d26f4fe459d87df882f` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/volt/gk104.c` | MIT (license text) | `fdbdb09b0adab174f06ec1822809198b749e01dec0c608def38f18d83fe6be28` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/volt/gk20a.c` | MIT (license text) | `f0f421600bd39ae5fd1a45cfc8995323b4a89383e872de579e7d439d13064a95` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/volt/gk20a.h` | MIT (license text) | `1ea38022f8854bc2dd1bdbd6ee0e2ba1fafd555dbcdb5500ba8716248a45dc10` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/volt/gm20b.c` | MIT (license text) | `bdf6b9b15587b0c7c692eb077e8c57222c51c144df3553f3337eb7dcf9d90a3d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/volt/gpio.c` | MIT (license text) | `607d5bc38534c3ab27c12d40bf74c59b530b9789fa6cb5ebf3219e97159ca79d` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/volt/nv40.c` | MIT (license text) | `198cca4b2c54666f3dece93e5984a036ebf7f44ab583e4f0ba16afe4f70b7cf3` |
| `drivers/gpu/drm/nouveau/nvkm/subdev/volt/priv.h` | MIT | `f23235b001705368283e4a5ea17300650668c25c8568b3fafe46e7e97cef4884` |
