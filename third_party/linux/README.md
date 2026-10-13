# Linux drivers, vendored unmodified

These files are copied byte for byte from the Linux kernel, tag **v6.12**
(`https://raw.githubusercontent.com/torvalds/linux/v6.12/<path>`). They are
built through Kestrel's Linux KPI (`linuxkpi/`, see the "Linux drivers"
section of `kestrel/README.md`) against the KPI's own `<linux/...>` headers;
none of Linux's headers are copied. There are no patches: if one is ever
needed it goes in `patches/` next to this file, applied to a copy at build
time, so the files here stay as upstream has them.

| File here | Upstream path (v6.12) | License (SPDX) | SHA-256 |
|---|---|---|---|
| `drivers/misc/pvpanic/pvpanic.c` | `drivers/misc/pvpanic/pvpanic.c` | GPL-2.0+ | `ee907764d7540598b2189a43904c161c9dfb8575c24cc939c4ca54c5aba34659` |
| `drivers/misc/pvpanic/pvpanic.h` | `drivers/misc/pvpanic/pvpanic.h` | GPL-2.0+ | `3c0b1d6d904ce1974145c5a909552bc4ca8c0d09b055d01734efd82c357a3aa9` |
| `drivers/misc/pvpanic/pvpanic-pci.c` | `drivers/misc/pvpanic/pvpanic-pci.c` | GPL-2.0+ | `9ccb6e1c335ae93ca9a485fd6ae85f23d2fddd1b61a66f14a8f12e33c00c7edd` |
| `drivers/watchdog/i6300esb.c` | `drivers/watchdog/i6300esb.c` | GPL-2.0-or-later | `ce08519e7df3003a0e1baa79eb21fe9f2e50312d05487cb0b400745820453107` |

Each file keeps its SPDX line and copyright notices. GPL-2.0-or-later code
may be combined with Kestrel, which is GPL-3.0 (the repository's `LICENSE`):
the combination is distributed under GPLv3.

## What may be vendored

- **Yes:** files whose SPDX line is `GPL-2.0+` / `GPL-2.0-or-later`, MIT,
  BSD (2- or 3-clause), or a dual license with one of those (e.g.
  `GPL-2.0 OR MIT`, `Dual BSD/GPL`).
- **No:** `GPL-2.0` / `GPL-2.0-only` files, including `GPL-2.0 WITH
  Linux-syscall-note` UAPI headers. GPLv2-only code cannot be combined with
  GPLv3 code. Most of Linux's core (and most of its headers) is GPLv2-only,
  which is why the KPI's headers are written from scratch.
  For example `include/uapi/misc/pvpanic.h` (GPL-2.0 WITH Linux-syscall-note)
  is not copied: `linuxkpi/include/uapi/misc/pvpanic.h` defines the three
  event bits itself, from QEMU's documented device interface.
- Check the SPDX line of every file, not the directory: licenses differ file
  by file. `MODULE_LICENSE("GPL")` means "GPL v2 or later" in Linux's own
  convention, but the SPDX line is what counts.

## Phase 2: DRM, TTM and bochs

These are the DRM core, the KMS helpers, TTM, the VRAM helper and the
bochs driver, with what they need from the rest of Linux, all from the same
tag. `make lkpi-drm-check` compiles each `.c` file against the KPI.

Unlike phase 1, the permissive headers that go with them are copied too
(`include/drm`, `include/drm/ttm`, `include/uapi/drm` and a few in
`include/linux`): the DRM code uses nearly every field of their structures.
Headers that are GPL-2.0-only, or carry no license line (which in Linux
means GPL-2.0), are written in the KPI instead, under `linuxkpi/include`:
`drm/drm_device.h`, `drm/drm_managed.h`, `drm/drm_gem_framebuffer_helper.h`,
`drm/drm_writeback.h`, `drm/drm_sysfs.h`, `linux/ww_mutex.h`,
`linux/dma-fence.h`, `linux/iosys-map.h`, `linux/seqlock.h` and the rest.

Three files of the DRM core are left out because they serve user space,
which phase 2 does not offer yet: `drm_prime.c` (dma-buf sharing),
`drm_syncobj.c` (sync objects) and `drm_flip_work.c`. The KPI answers their
calls.

| File here | License (SPDX, or the license text when there is no SPDX line) | SHA-256 |
|---|---|---|
| `drivers/dma-buf/dma-resv.c` | MIT | `2de74f2bcb3e1bd09abc755f5c955bcc8a2bff3ad8c9362bc6c6cec5828159ec` |
| `drivers/gpu/drm/drm_aperture.c` | MIT | `af270fb7410df6ed4ae0533a8e7255a0939bdb1c03d9f225801ae9c7bb63bc04` |
| `drivers/gpu/drm/drm_atomic.c` | MIT (license text) | `12749ac7e9689791f92534effdb6eae9e8529e5a169837ac753ca9a0799a3cf4` |
| `drivers/gpu/drm/drm_atomic_helper.c` | MIT (license text) | `17109cb00d68c2f4e560efcbca0f03276a10841c154e4d4ead5d1a3abfb132a2` |
| `drivers/gpu/drm/drm_atomic_state_helper.c` | MIT (license text) | `f395fac869a7b840cc0672916a872e37a98833d952889a121ef5e99801edbce0` |
| `drivers/gpu/drm/drm_atomic_uapi.c` | MIT (license text) | `d130775eeab9ea43c11d6185fdfe376570765c5d1418a345dd6bbb589313ea16` |
| `drivers/gpu/drm/drm_auth.c` | MIT (license text) | `60cd5698b0b1357168379c9649b1f3e2a3ca2338594397355e7b4b51cf515df8` |
| `drivers/gpu/drm/drm_blend.c` | MIT (license text) | `f3a63f97036f4bbd7a2a889d16a3c22d7a0a7dd1b9dc7ad172e1cc9c828ebac0` |
| `drivers/gpu/drm/drm_bridge.c` | MIT (license text) | `40ce006786ddb0cda02b31309534ad5e977c7e31bf4bdd6812fa50e70a6b37af` |
| `drivers/gpu/drm/drm_cache.c` | MIT (license text) | `0209a134ddde86dcc00ffa2e7d2aa14a8c07319d95211caa002d57f7f291eed2` |
| `drivers/gpu/drm/drm_client.c` | GPL-2.0 or MIT | `6cd4ebda8f1ea9c9805279f824f4dc3d330a9eaabce6e4a439a247820c60fbbc` |
| `drivers/gpu/drm/drm_client_modeset.c` | MIT | `f7bdc87f73a3e1a0d1560ef3bb005e92cd310ad3e2d748462717f14fd1020258` |
| `drivers/gpu/drm/drm_color_mgmt.c` | MIT (license text) | `044f70a228b1a8585a7532d825656d78bf7b17871b978e46cc016c8aa171d367` |
| `drivers/gpu/drm/drm_connector.c` | MIT (license text) | `e0c9c12df860879b83ae65a62ab0c7df90046e3155d6f0e1af09c30ca77b5410` |
| `drivers/gpu/drm/drm_crtc.c` | MIT (license text) | `e458de19f4498a20fc44baa6d0cdcf4e1a65b9dbd55bf6d1fbecd108a1157bb3` |
| `drivers/gpu/drm/drm_crtc_helper.c` | MIT (license text) | `8f2139503596842c8cde0ce07a85e38b6868193e8f8b8fdd79478752732335ac` |
| `drivers/gpu/drm/drm_crtc_helper_internal.h` | MIT (license text) | `834cfbaed65751458e62f356d33df9ea3bdf832f5bd7e624734c7f4160670ca1` |
| `drivers/gpu/drm/drm_crtc_internal.h` | MIT (license text) | `fbb9e5fc67af6477f2b73d0953b02eb80d3131268a84bbdc9260b7148b556d0d` |
| `drivers/gpu/drm/drm_damage_helper.c` | GPL-2.0 OR MIT | `84ef2abd52ae95c6ad0055df2e9f958bc3e619c49d019cff4a3ebe664f3d96b3` |
| `drivers/gpu/drm/drm_displayid.c` | MIT | `11c00eae7b94fea57ba8e92dd48762e91f636423bf19c095c185eb962caa7193` |
| `drivers/gpu/drm/drm_displayid_internal.h` | MIT (license text) | `71db4be6b00cc63a7cae3363329f0d2a8399986ced8c9314b5c8a0357d0a4509` |
| `drivers/gpu/drm/drm_drv.c` | MIT (license text) | `f1263cc61499f622da05bbb2be6b66136e39499dcbc9348ebc17c64442bbd595` |
| `drivers/gpu/drm/drm_dumb_buffers.c` | MIT (license text) | `836a0809a82e6ca51c7e442fc8efb8a221f40158f5897be11f99a32059441ef5` |
| `drivers/gpu/drm/drm_edid.c` | MIT (license text) | `1d042133c26d43abff917cd2dd3d0bf7694baa2188e84326e03161381dc1d9f8` |
| `drivers/gpu/drm/drm_eld.c` | MIT | `5284adfd06376cf1fe10a1122a6acdd75f0ce238028a64327a6acd0d7becd9b2` |
| `drivers/gpu/drm/drm_encoder.c` | MIT (license text) | `844ed39fceaa828ad9974eb0b598fe957ae12ea65191fb6f15a65576c459cb9a` |
| `drivers/gpu/drm/drm_file.c` | MIT (license text) | `29f83ac332e86d1680104e8db93833ef69ae17e7de4dc488b19ba3e3d96bac5c` |
| `drivers/gpu/drm/drm_format_helper.c` | GPL-2.0 or MIT | `a8d4984d56a6a89b804ca2321e51e98d8173df1f8764da973985f686542cb56e` |
| `drivers/gpu/drm/drm_fourcc.c` | MIT (license text) | `2df9bfee6a0340b20a98585b4893fc3c702bcc7a7c111f66142eb3e83328c7ea` |
| `drivers/gpu/drm/drm_framebuffer.c` | MIT (license text) | `3d45085a8cb741cd3022fca7bd3ca80a7a216a7e3d67bf627894d429aedfc9a1` |
| `drivers/gpu/drm/drm_gem.c` | MIT (license text) | `4cc7609b82cf050ae4844d7ceb5e6783925e7c458bb3237c7359420cedb888b6` |
| `drivers/gpu/drm/drm_gem_atomic_helper.c` | GPL-2.0-or-later | `70a8d967b93e217c774b4996eff955393e4716f87a11ea6fa3e30d3e08dd6bf2` |
| `drivers/gpu/drm/drm_gem_framebuffer_helper.c` | GPL-2.0-or-later | `4727ac12a504070a9d229bc4e9f45dab063cac720120004fe23146934c4a36d5` |
| `drivers/gpu/drm/drm_gem_ttm_helper.c` | GPL-2.0-or-later | `8f05e84031b983a28e0ab02da5524e3af35ff93b6ac2043af5c714b0cad3a015` |
| `drivers/gpu/drm/drm_gem_vram_helper.c` | GPL-2.0-or-later | `5ad488ae4ee41a8e8de37f7435e0c5ea606f6e42869f22447fb7698e9b02e286` |
| `drivers/gpu/drm/drm_internal.h` | MIT (license text) | `186d76c744e6c208b1ef6128ec206a1de57ba83d1d38bf8e324f6b52eb2c0440` |
| `drivers/gpu/drm/drm_ioctl.c` | MIT (license text) | `f544251150d1bd786d570c20525bd29f04906a22617d72aed4c12d38326041ea` |
| `drivers/gpu/drm/drm_kms_helper_common.c` | MIT (license text) | `5e7030ceb612d89d21a2fb49e81a379a29f0c84360d01051698b297b5ea286fe` |
| `drivers/gpu/drm/drm_lease.c` | GPL-2.0-or-later | `74e3674d35c456764e8fe848e8f01f3e5e24a05889b6b909f1d09412e6b28685` |
| `drivers/gpu/drm/drm_mm.c` | MIT (license text) | `4a92b33691aeae6d98295a2b988d4951e2e87917d08e9214b942128b88d88d96` |
| `drivers/gpu/drm/drm_mode_config.c` | MIT (license text) | `60a57abb881fb6af4bf494d4440d9d63889b0a4288221c0d77e6390194c1d805` |
| `drivers/gpu/drm/drm_mode_object.c` | MIT (license text) | `6f0e1716e2666eb1dff72ec9cc9476f032b45c6da31367429466bd87c1caf9d3` |
| `drivers/gpu/drm/drm_modes.c` | MIT (license text) | `bbd8079eebc846aefb3c9f54f47d1162f96f85d1f2413ab4aca596b4fb8b9d97` |
| `drivers/gpu/drm/drm_modeset_helper.c` | MIT (license text) | `42bb13e3676007e55b491fd0684c7aafdc53609e2e63aeca89dcb33455bccaad` |
| `drivers/gpu/drm/drm_modeset_lock.c` | MIT (license text) | `035a917a1c58a8c28531fbc54c53106ab9142c217ca33f6134ed5dc340329d19` |
| `drivers/gpu/drm/drm_pci.c` | MIT (license text) | `779cacbd450668361747399b2fba5518629d809b707e38f725276c2043c97386` |
| `drivers/gpu/drm/drm_plane.c` | MIT (license text) | `abab950b95dff48c1b546976277b4d56477d26f3283a4b573d4074d7d1e33ebb` |
| `drivers/gpu/drm/drm_plane_helper.c` | MIT (license text) | `4e89ec927bfe7d10a804c613239a7b59fd5339dcfcddcf5a2a58cac8c3d6d88d` |
| `drivers/gpu/drm/drm_print.c` | MIT (license text) | `4f276252940216bf8baaa939f32ef296d3c561d9b4a44194f9280d9a8e7d699a` |
| `drivers/gpu/drm/drm_probe_helper.c` | MIT (license text) | `26fbc6b89a7a07738b1f01dadd065d48c7c70f745999325fccde6b30153595e0` |
| `drivers/gpu/drm/drm_property.c` | MIT (license text) | `0cc2a221ec2f9220aa8c4872c7841d679080c5b29e1d059692233491b7d86843` |
| `drivers/gpu/drm/drm_rect.c` | MIT (license text) | `2fce0089dc7475a796dbc809aeb078dc859f13fdb1e8113fa483f37f7e73c44a` |
| `drivers/gpu/drm/drm_self_refresh_helper.c` | MIT | `34a798b3ebc36003056164945f46fd3689db7316dd65f79918af7ac98a889537` |
| `drivers/gpu/drm/drm_simple_kms_helper.c` | GPL-2.0-or-later | `c1173c217ff3168ddb32791ebc41831bddb978f6c6cf78588296c73bc71989fa` |
| `drivers/gpu/drm/drm_vblank.c` | MIT (license text) | `ac0f83cecacb2cb2d84e0d0c17138d3726e2f060a0b47296fbea6a2e7c791e51` |
| `drivers/gpu/drm/drm_vblank_work.c` | MIT | `71501a32e8f8ba1dabdb863960fa0b1630ac4ace031e2809d69dcce013eba57e` |
| `drivers/gpu/drm/drm_vma_manager.c` | GPL-2.0 OR MIT | `b9813a35186f67f01d85a5b3b412b45d80279ee3d4f07f1aa830380977adbdb7` |
| `drivers/gpu/drm/tiny/bochs.c` | GPL-2.0-or-later | `5877fe15d83a825666e474e5bd6580ea16bc72e5dbb2ffe43802a66b0cb8368a` |
| `drivers/gpu/drm/ttm/ttm_bo.c` | GPL-2.0 OR MIT | `bb767392966340828e110b8b8cc17cf1db12bf9b7f4fc6cacf94b80007a93895` |
| `drivers/gpu/drm/ttm/ttm_bo_util.c` | GPL-2.0 OR MIT | `2fa09d706178341c32b09cb5b285faf83165353ca08fffdb0980e336ec70461e` |
| `drivers/gpu/drm/ttm/ttm_bo_vm.c` | GPL-2.0 OR MIT | `fc58fab41b45c49198abe448859cb28d08462d3bdf2f754d615c11f9dc6eb8a1` |
| `drivers/gpu/drm/ttm/ttm_device.c` | GPL-2.0 OR MIT | `af225d5dbf43fbe00110c67e2a9f9a01069dea46c183875c5dc6d10b077ec30e` |
| `drivers/gpu/drm/ttm/ttm_execbuf_util.c` | GPL-2.0 OR MIT | `e5e41742850a705142f4a1df5e1fd275866a8907595bf9bcbbb28636d070deae` |
| `drivers/gpu/drm/ttm/ttm_module.c` | GPL-2.0 OR MIT | `8637badcd3396e8fb4e4b0858737a85abd5544b877e0840290172a1dd0d18798` |
| `drivers/gpu/drm/ttm/ttm_module.h` | MIT (license text) | `bb517301d23c5c06f6188173ba52a07ec9e44208c6a8e9a0cfe9550ae94886dd` |
| `drivers/gpu/drm/ttm/ttm_pool.c` | GPL-2.0 OR MIT | `61a9125deaffa72607abe9b1fe88113162db4d468fcc2005376d12209e4aa9cd` |
| `drivers/gpu/drm/ttm/ttm_range_manager.c` | GPL-2.0 OR MIT | `00517a5b5fa5d2b78f4232d7ac413d3b6d1c42b0dcc5dda689e6abfc987569fa` |
| `drivers/gpu/drm/ttm/ttm_resource.c` | MIT (license text) | `06ce1d892b5e7a1fe9794ad95644aad7416ab482afcc9912e4064b9540ce77a3` |
| `drivers/gpu/drm/ttm/ttm_sys_manager.c` | GPL-2.0 OR MIT | `10270531dcb3082e7451f344c1951ce46f42ff269624b1f722911e8e05d7c16e` |
| `drivers/gpu/drm/ttm/ttm_tt.c` | GPL-2.0 OR MIT | `338c73b11d9762aded57a6369816371896b16b5f2c855ca8d6a98a44bf3cdf6c` |
| `drivers/video/aperture.c` | MIT | `c397bb5c95c28f95a5c8a5e8481e7394ce3517f43ba1fb939492828ad1c89001` |
| `drivers/video/hdmi.c` | MIT (license text) | `43a6223cb0fc85708aee89b6a69ffbd43d6b9f7e9f2b56d7c113dc4b9bfb3d78` |
| `include/drm/amd_asic_type.h` | MIT (license text) | `cea577df48a684246ef00d887350d42b9fa5048b9135ae5e8e93dc9eb94a7809` |
| `include/drm/display/drm_dp.h` | MIT (license text) | `70ea7e2c08c2155450f998d30d53e147070443168f616f06584b671e2b37dfd2` |
| `include/drm/drm_aperture.h` | MIT | `b3c41831cd7f85405690cbb84c72d4ecd85aa1fdc1400d778463b357ce6c9ced` |
| `include/drm/drm_atomic.h` | MIT (license text) | `19ea82c5147f56ba934f09ab4aae632f62fb9f77da279f2c5f56bfda97404f33` |
| `include/drm/drm_atomic_helper.h` | MIT (license text) | `7774ed6ccf86c1f2c7b5d34c03a7b1143d6a2a280c7b71c9d87c13b6b9119057` |
| `include/drm/drm_atomic_state_helper.h` | MIT (license text) | `165ed56511b0b1c45640aea35ee60b4728335a7c264526deeca7967d90cea45c` |
| `include/drm/drm_atomic_uapi.h` | MIT (license text) | `0fabc1d74c02a65b37b3d9ac505ec85362cbcb427e297227e3f3e059bec361fe` |
| `include/drm/drm_audio_component.h` | MIT | `6d59e60cbb84b118da265c0250d0f52b5ded63f2c7ff87de61a6023c228b30d6` |
| `include/drm/drm_auth.h` | MIT (license text) | `3c4f8759e905141c9128260f185e7c9a0289b7aff9bacbb5bb4ee4f292f2b726` |
| `include/drm/drm_blend.h` | MIT (license text) | `46a47b37fcb9dbb1da3f98d58db0075d21b1a0cde21290c5d627d4ace05f044f` |
| `include/drm/drm_bridge.h` | MIT (license text) | `7602505e1447315a577a3cd6953715caf58592086a8b8ad25f70fbde79778e94` |
| `include/drm/drm_bridge_connector.h` | GPL-2.0+ | `4fe107999bf2de46f34f1d52a085c911177317b204aa3af0964d62d54febdb6e` |
| `include/drm/drm_buddy.h` | MIT | `5b6e5e0978482722e89a0bfbe46417a87d3fc1a04f29b0fc1ed1be9c3667b48c` |
| `include/drm/drm_cache.h` | MIT (license text) | `2626120d1dd4222c14779b71a352e239aa80536aeebc1e742cf552abb77598ed` |
| `include/drm/drm_client.h` | GPL-2.0 or MIT | `6cfdf0a4f6ff275a9fa0575d89b5fb17d28810a20ffb44bcf72344d02a8ee3a8` |
| `include/drm/drm_color_mgmt.h` | MIT (license text) | `6332adb2c833e3a6ac6a7b1083446eb1e2964c3e2b7f40014309a067f0b8ee5e` |
| `include/drm/drm_connector.h` | MIT (license text) | `7408d1e99a94c9c7b9ed43c8aa117d418698237d9f9f2e73fb8e8f07d264911a` |
| `include/drm/drm_crtc.h` | MIT (license text) | `8bcf63af1be3bd99ebf195eb7f269b2032aded6b273fb02f45a155c46f89879a` |
| `include/drm/drm_crtc_helper.h` | MIT (license text) | `c81f00db30ddb11af48f7f6497141ef9e2513b00ac51731880d0cbe606dcf46d` |
| `include/drm/drm_damage_helper.h` | GPL-2.0 OR MIT | `d38715934ff674a7f4c620e3b1ed987a5076e9c6437637b3b58a8b0b4c5ea7a6` |
| `include/drm/drm_debugfs.h` | MIT (license text) | `828ab0416c5c41df36ca0765db7dc9034ec0633e74628a16f48b897c4f6d4294` |
| `include/drm/drm_debugfs_crc.h` | MIT (license text) | `fca5a1b50b80cc6b3bd120175e10c657bf47a75a9a6313f9e7a6bf146a73885d` |
| `include/drm/drm_drv.h` | MIT (license text) | `457b1ed1d1d777c397d2f7024e74607f8815f8360a1d104366640a212b4e6d9d` |
| `include/drm/drm_edid.h` | MIT (license text) | `be7f3d00c6e89583213f866c010ee41ef777f0cc6af32ca45758d81cefe88fb3` |
| `include/drm/drm_eld.h` | MIT | `c727bcb398db53d5f774e39363dc4ffbf8ef7832ef0d1d2570a618e10937b507` |
| `include/drm/drm_encoder.h` | MIT (license text) | `445f36154601a1618d971afcf2f8fa9000e0a828b86d65a16aab26a89745474b` |
| `include/drm/drm_encoder_slave.h` | MIT (license text) | `7520c3f1d2a19bb76a61c735e74327899d920793c5b9e84c45168a4a870dca17` |
| `include/drm/drm_exec.h` | GPL-2.0 OR MIT | `d723c55591003b60e57261ba6d1f209331461e1d84e6eef5b73a2a0b6f8d1573` |
| `include/drm/drm_fb_helper.h` | MIT (license text) | `4b4abdf7ce6e2ddd170dc8bfc26891be22fccfe63073253ab6b4a3fcfe596b0d` |
| `include/drm/drm_fbdev_dma.h` | MIT | `8683fcfee5a327dde00d0ff957685cda6bbcf10fd8a390376788923342538831` |
| `include/drm/drm_fbdev_shmem.h` | MIT | `9fb99e56bb9b1f6ec85783ec6aa4d2b79ad6a7f11e40c605c247c2d31b318732` |
| `include/drm/drm_fbdev_ttm.h` | MIT | `355ea754a3f25cf5a7068b4b1a76ff61ff16bc3ffdc544bdff56b05abc467f23` |
| `include/drm/drm_file.h` | MIT (license text) | `51e09159699ade73203f60cdc307b5b115eb59cb6bf8885a7f92b2ed0e33f550` |
| `include/drm/drm_fixed.h` | MIT (license text) | `c62fbb9cd17416f7921964f14f39e45d5e3df3e61e49ea213a3aa1ae27d60051` |
| `include/drm/drm_flip_work.h` | MIT (license text) | `d20dcd1362a2227cf3b161e78edd8d2c08672d54e161eceb13844043bfd472f3` |
| `include/drm/drm_format_helper.h` | GPL-2.0-or-later | `7321e3626f1fd65b5ab1e3a901cc6408c5ddcac6e55bbe29e726271cc366c49e` |
| `include/drm/drm_fourcc.h` | MIT (license text) | `62afca4acef76d92f7ebea38a851404f27d4cbb981919feb5f3dde866b3410c0` |
| `include/drm/drm_framebuffer.h` | MIT (license text) | `bfef4f52242a97286e75826f642c409ac2be9945044c63a10e9c27eeb702c1c0` |
| `include/drm/drm_gem.h` | MIT (license text) | `6f757a43dbf7bb344a91428015f2bb312970504fb02568678141677e0778fbae` |
| `include/drm/drm_gem_atomic_helper.h` | GPL-2.0-or-later | `0f9c57cd46436d2e60caf9c7c68d65d86b1a36becff0217525f2be8f7bc443f8` |
| `include/drm/drm_gem_ttm_helper.h` | GPL-2.0-or-later | `402dca017a9517eeda6a2ddcc71d96fc0aa1d66aed5e4273f19b2f330226e2f9` |
| `include/drm/drm_gem_vram_helper.h` | GPL-2.0-or-later | `732b419bcb3e4d32791b456ccff4eaf557fbb7807a7194c938b44cb661e2eb45` |
| `include/drm/drm_gpuvm.h` | GPL-2.0-only OR MIT | `704c2fa1efd94b69fecffa8e8c5a9ad81a1ea0e43d4f25aad0f1d0aaf49faab3` |
| `include/drm/drm_ioctl.h` | MIT (license text) | `19e9387b68cc066bf98ff22add414855d82084486ddcd5344594c883e415d4ac` |
| `include/drm/drm_lease.h` | GPL-2.0-or-later | `e8459d83ec91e6336763afbca326321967e2d25d1666f693218811154ceb77b2` |
| `include/drm/drm_mipi_dbi.h` | GPL-2.0-or-later | `cd224ad4818c91b31e8173cf6a17cfa8dbf01f78469b8ddd14ca4d44b46674cb` |
| `include/drm/drm_mm.h` | MIT (license text) | `f77781d73fc713fa76b60972cde2d6684c88fdee7b7eea5ded9348f723c1a03a` |
| `include/drm/drm_mode_config.h` | MIT (license text) | `68b6b93b87ace94b89363a947fb62418a749b592615ae19943c6ff5f5293c6d3` |
| `include/drm/drm_mode_object.h` | MIT (license text) | `6fdc6f42057709f6d4ea7e92772e87404574206ead58b571d32bee7f557a992e` |
| `include/drm/drm_modes.h` | MIT (license text) | `d2502e077f292cffda4ca98d8c83df610397cb62637bb932d3b4256c9ce73759` |
| `include/drm/drm_modeset_helper.h` | MIT (license text) | `e4dac917f063ad6120ebf9025997914138a8aa30035bf6001ae97d7a566e4790` |
| `include/drm/drm_modeset_helper_vtables.h` | MIT (license text) | `b286d3a4079379930bac7049713bad7103a240a7a1dd6cbcfa8d7bdc8f119dbe` |
| `include/drm/drm_modeset_lock.h` | MIT (license text) | `c41c21cd102a3a5a42ffa2be8599308fdb962bc7fc5ea15fbda7cbc1e2958244` |
| `include/drm/drm_module.h` | MIT | `dc6892ad6b8dc42b1500134f318561fe8543cb89f28693fdd8ca54a28b806207` |
| `include/drm/drm_panel.h` | MIT (license text) | `9ee92c717c5fd869239b3d1cd34f91cccd499d432af7c834ea50ae30a6bc8929` |
| `include/drm/drm_panic.h` | GPL-2.0 or MIT | `f480234e4e6f315808b381fe378b54a2ee8e4f0f54cfc9e9284365f4340f6fda` |
| `include/drm/drm_pciids.h` | MIT | `ba5fec67c26231cee37716519d6ef5f240881ea12b7005094dbb0f4cf14d9f3f` |
| `include/drm/drm_plane.h` | MIT (license text) | `e2e0e52165ef85a35e5d6ae3fb2c5135ab366822583b5d208179bca1e7f71d04` |
| `include/drm/drm_plane_helper.h` | MIT (license text) | `81995b67aea55f88f0d57a98c3672e8a3ea18ea00d0243ee21e8a3a2541d644e` |
| `include/drm/drm_prime.h` | MIT (license text) | `089d54d42172d9a7a3dfd6a60505c24ae91ddc294f6df97fe4039c6ca038a971` |
| `include/drm/drm_print.h` | MIT (license text) | `714a1e0deec5abb6b7a641f18c2f1c4782d5ce078a685a4b7795f531777d79ff` |
| `include/drm/drm_privacy_screen_consumer.h` | MIT | `43f310e92522933f889e2f65ee2cd18a2e4dcf30b9e078a9d549e6d36289f521` |
| `include/drm/drm_privacy_screen_driver.h` | MIT | `0f9d08b01050cfc867a5691f6667b25a548f9e50516404ba18ce060e89fb7671` |
| `include/drm/drm_privacy_screen_machine.h` | MIT | `41bf104768bc24bb34e3aa207d8a1ae267f0a3679f1dae7b189a36b41082a36f` |
| `include/drm/drm_probe_helper.h` | GPL-2.0 OR MIT | `002be91ed2d11f5119262052a83271771860c01f763fdea4435c9cd1bb784f2a` |
| `include/drm/drm_property.h` | MIT (license text) | `e32b29da2ddd823ebe51d4a0e4221517214be4d901a46b498ec2a36211c561eb` |
| `include/drm/drm_rect.h` | MIT (license text) | `9faf2644c273934c05e4dbc788afe2dec58aac2b22b6eb31a29a9a39821ed353` |
| `include/drm/drm_self_refresh_helper.h` | MIT | `3c21220a5da75301faa25b4cd4d72036a25c6278c131b046b2afe54ce099f9ed` |
| `include/drm/drm_simple_kms_helper.h` | GPL-2.0-or-later | `c57d04007663706aa6c8ded8921e4b0b596f04cc667a0d18f360c2e0e363e863` |
| `include/drm/drm_suballoc.h` | GPL-2.0 OR MIT | `0bff90c3c8cf041158fba0e8508da15cb958e7155990b40fa9e2078fd357e1f6` |
| `include/drm/drm_syncobj.h` | MIT (license text) | `fb97882ee3220ff6a99d60d2332d7b0afa473d569500cf1042c05dd16534c3da` |
| `include/drm/drm_util.h` | MIT (license text) | `21fb821e715183662f1f07405cd5b0e5094022646787ff5da4ece2caf4cd7282` |
| `include/drm/drm_utils.h` | MIT | `070c2464bb9ea4cec8c506a1494f7bec5e85af8f0143632c15c4852791eace44` |
| `include/drm/drm_vblank.h` | MIT (license text) | `ca73645781e21c8c67f7559c82f1f054ffbd27767c8a9880cec8b36809d3bbf6` |
| `include/drm/drm_vblank_work.h` | MIT | `7449e01915ddd1e389da5106eb40c0809341c2ce110aef2437e6064c15156c40` |
| `include/drm/drm_vma_manager.h` | MIT (license text) | `76497508aeee275b309dd61c2480adf6086dab998fd52e5f30ecb735923418d8` |
| `include/drm/gpu_scheduler.h` | MIT (license text) | `127b116b0d164b0c885367f47f3ccbd7a6a3c1b1c2ae0d09167fcaf59b78dedd` |
| `include/drm/gud.h` | MIT | `7ea644a846dd6a3c6427ed45877a8b5d1e87da0ca558ece8c82d3a5d044f549a` |
| `include/drm/spsc_queue.h` | MIT (license text) | `f4a4eee1a1b969bbd28a7e4da101280a8da9d2ccece82db559822c0d153cb84f` |
| `include/drm/task_barrier.h` | MIT (license text) | `86b2f299f54a7069ba3ddbcdbe7a25f2f64a19501ff7254bdaf75419b21c41b6` |
| `include/drm/ttm/ttm_bo.h` | MIT (license text) | `90d76ee5c693cefaae523b5d2bba16f677f58f8c72c2e9063aa0bed33ec872b7` |
| `include/drm/ttm/ttm_caching.h` | MIT (license text) | `4004c637257a1147149649f0db21d0cabbdbe07c137409ef5ba698a8d0a40f65` |
| `include/drm/ttm/ttm_device.h` | MIT (license text) | `7e7d923e5203e931df5346da15fbc71458fefea739ac3e943d25b66c0d479c80` |
| `include/drm/ttm/ttm_execbuf_util.h` | MIT (license text) | `6393e8fb5c9dbeae4c98dc494ba05f882f40f007be437c1dd6a0fe7e3770b039` |
| `include/drm/ttm/ttm_kmap_iter.h` | MIT | `61c83bc1e9d69a4a0c4acc9947682232ae7f80ce28a5797b366eae773d262290` |
| `include/drm/ttm/ttm_placement.h` | MIT (license text) | `7fa52416e77115810c57300847b20f10fe1d801df4ada75df05bc706500fdff5` |
| `include/drm/ttm/ttm_pool.h` | GPL-2.0 OR MIT | `1b065e9b7884028b88258bbcf030b61be3a92b0173ee3de8aba322293c672228` |
| `include/drm/ttm/ttm_range_manager.h` | GPL-2.0 OR MIT | `d90df16986805e8249569ee5e2da3dfb1fef2be1ed8bf798b913417ad7ae4f4a` |
| `include/drm/ttm/ttm_resource.h` | MIT (license text) | `9cb648858a498bb211b50b81e310abb5022ea754c155d4385682991a221059dc` |
| `include/drm/ttm/ttm_tt.h` | MIT (license text) | `e907588ddcb102a5c4e3721050d8a35e641854c9c8d83c238d00319dc6263f06` |
| `include/linux/aperture.h` | MIT | `0738559a7af8ce36f454a075dbfecc6a45363dc55f3b4d243d2843bb83d28679` |
| `include/linux/dma-resv.h` | MIT (license text) | `35bcab866910f8b4a923d28fff5e56fd8317c49db6a0c7ea59550c3281b0101c` |
| `include/linux/hdmi.h` | MIT (license text) | `d0786532230d46c8fd4bd633d74ecb19f9a80dfa3d283cfa7945255048f64f6f` |
| `include/linux/interval_tree_generic.h` | GPL-2.0-or-later | `be967d888816b1c1d859ba02478cbea1a684e1f2180debef0c33d27f828f48f1` |
| `include/linux/rbtree.h` | GPL-2.0-or-later | `f80d6c4d8ed7ac6f4714a500e4080bcd5070279496a82f602b517055bbbcbd6e` |
| `include/linux/rbtree_augmented.h` | GPL-2.0-or-later | `6d9e24ee6482b468d4661eb23bfbeed490cce0aea7ba87c0ebe73101d4f168c4` |
| `include/linux/rbtree_types.h` | GPL-2.0-or-later | `bb78534d4212776381b3b849ad6ce1009b9d3fdfb87e19f51e5e9449e7f03e22` |
| `include/linux/xarray.h` | GPL-2.0+ | `3b98cad8f1a4234b7520720819012bc1344dfa4b0df16b215df7c994d798eb36` |
| `include/uapi/drm/drm.h` | MIT (license text) | `3ab6ac01bf91067aed96b70d7fa7847a86e7f726d74278151f085143688659cc` |
| `include/uapi/drm/drm_fourcc.h` | MIT (license text) | `dd775484fc3d603d20cde445f79b858906531b4a26104b4726a0c8852f7f4415` |
| `include/uapi/drm/drm_mode.h` | MIT (license text) | `3a5bad960e64a6374e046450dcb637145f4127950659a6429561d9d8faf4d14b` |
| `lib/radix-tree.h` | GPL-2.0+ | `fa9aeb8dc0b7ef14854330cd4d3a50f423e6f690ac269a61d62a9ae6c0147125` |
| `lib/rbtree.c` | GPL-2.0-or-later | `af9e0683db2dc9ce457a3262d9aaa8c73ce46cb5019e1a968502bb30d69f5dd5` |
| `lib/xarray.c` | GPL-2.0+ | `34b1800a58931a13cda948ab8ef5ae66e06cf91714618af887c24b03e32f2c32` |
