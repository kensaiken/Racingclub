# Funciones del sistema PS3 que usa GTI Club (para el port recompilado)

Fuente: log de carga de RPCS3 (`re/imports.txt`). "->" = direccion del stub en el EBOOT: cuando el codigo recompilado
llama a esa direccion, el port ejecuta su propia implementacion (hook de host).

## sysPrxForUser (106) — IMPLEMENTAR (hilos, mutex, memoria, tiempo, printf) -> Win32/C++

`0x9FB6228E`@0x128d978, `_sys_free`@0x12bf8dc, `_sys_free`@0x12d9430, `_sys_malloc`@0x12bf89c, `_sys_malloc`@0x12d93f0, `_sys_memcmp`@0x128daf8, `_sys_memcmp`@0x12d9450, `_sys_memcmp`@0x130f198, `_sys_memcpy`@0x128d8f8, `_sys_memcpy`@0x12bf83c, `_sys_memcpy`@0x12d9390, `_sys_memcpy`@0x130f098, `_sys_memmove`@0x130efd8, `_sys_memset`@0x125adec, `_sys_memset`@0x128d8d8, `_sys_memset`@0x12bf81c, `_sys_memset`@0x12d9370, `_sys_memset`@0x130f078, `_sys_printf`@0x125ae0c, `_sys_printf`@0x128d958, `_sys_printf`@0x12d93b0, `_sys_printf`@0x130f118, `_sys_process_at_Exitspawn`@0xa7f94c, `_sys_process_atexitspawn`@0xa7f86c, `_sys_snprintf`@0x128d7b8, `_sys_snprintf`@0x130ef98, `_sys_spu_printf_attach_group`@0x128da18, `_sys_spu_printf_detach_group`@0x128d8b8, `_sys_strcat`@0x128d778, `_sys_strcat`@0x130ef78, `_sys_strchr`@0x130f0b8, `_sys_strcmp`@0x12bf7fc, `_sys_strcmp`@0x130f058, `_sys_strcpy`@0x128d938, `_sys_strcpy`@0x12bf85c, `_sys_strcpy`@0x130f0f8, `_sys_strlen`@0x128d878, `_sys_strlen`@0x12bf79c, `_sys_strlen`@0x12d9330, `_sys_strlen`@0x130eff8, `_sys_strncat`@0x128d918, `_sys_strncmp`@0x125ad4c, `_sys_strncmp`@0x128d758, `_sys_strncmp`@0x12bf71c, `_sys_strncmp`@0x130ef58, `_sys_strncpy`@0x128d9d8, `_sys_strncpy`@0x130f178, `_sys_vprintf`@0x128dad8, `_sys_vsnprintf`@0x128d798, `sys_initialize_tls`@0xa7f90c, `sys_lwcond_create`@0x128d9f8, `sys_lwcond_destroy`@0x128d818, `sys_lwcond_signal`@0x128dab8, `sys_lwcond_wait`@0x128d858, `sys_lwmutex_create`@0x125adcc, `sys_lwmutex_create`@0x128d898, `sys_lwmutex_create`@0x12bf7bc, `sys_lwmutex_create`@0x12d9350, `sys_lwmutex_create`@0xa7f88c, `sys_lwmutex_destroy`@0x125ae4c, `sys_lwmutex_destroy`@0x128d9b8, `sys_lwmutex_destroy`@0x12bf8bc, `sys_lwmutex_destroy`@0x12d9410, `sys_lwmutex_destroy`@0xa7f9cc, `sys_lwmutex_lock`@0x125ad6c, `sys_lwmutex_lock`@0x128d7d8, `sys_lwmutex_lock`@0x12bf73c, `sys_lwmutex_lock`@0x12d92d0, `sys_lwmutex_lock`@0xa7f80c, `sys_lwmutex_unlock`@0x125ad8c, `sys_lwmutex_unlock`@0x128d7f8, `sys_lwmutex_unlock`@0x12bf75c, `sys_lwmutex_unlock`@0x12d92f0, `sys_lwmutex_unlock`@0xa7f82c, `sys_mempool_create`@0xa7f9ec, `sys_mempool_destroy`@0xa7f96c, `sys_mempool_free_block`@0xa7f8ec, `sys_mempool_try_allocate_block`@0xa7f7ec, `sys_ppu_thread_create`@0x125adac, `sys_ppu_thread_create`@0x128d838, `sys_ppu_thread_create`@0x12bf77c, `sys_ppu_thread_create`@0x12d9310, `sys_ppu_thread_create`@0x130efb8, `sys_ppu_thread_create`@0xa7f84c, `sys_ppu_thread_exit`@0x125ae2c, `sys_ppu_thread_exit`@0x128d998, `sys_ppu_thread_exit`@0x12bf87c, `sys_ppu_thread_exit`@0x12d93d0, `sys_ppu_thread_exit`@0x130f158, `sys_ppu_thread_exit`@0xa7f9ac, `sys_ppu_thread_get_id`@0x12bf7dc, `sys_ppu_thread_get_id`@0x130f018, `sys_ppu_thread_get_id`@0xa7f8ac, `sys_ppu_thread_register_atexit`@0x130f038, `sys_ppu_thread_unregister_atexit`@0x130f138, `sys_process_exit`@0xa7fa2c, `sys_process_get_paramsfo`@0x125ae6c, `sys_process_get_paramsfo`@0x128da78, `sys_prx_exitspawn_with_level`@0xa7f98c, `sys_prx_get_module_id_by_name`@0x128da38, `sys_prx_register_library`@0xa7f8cc, `sys_spu_image_close`@0x128da58, `sys_spu_image_close`@0xa7fa0c, `sys_spu_image_import`@0x128da98, `sys_time_get_system_time`@0x130f0d8, `sys_time_get_system_time`@0xa7f92c

## sceNp (46) — STUB (PlayStation Network)

`sceNpBasicAddBlockListEntry`@0xa7eb4c, `sceNpBasicAddFriend`@0xa7ebac, `sceNpBasicAddPlayersHistory`@0xa7eb2c, `sceNpBasicAddPlayersHistoryAsync`@0xa7ee8c, `sceNpBasicGetBlockListEntry`@0xa7f00c, `sceNpBasicGetBlockListEntryCount`@0xa7ecec, `sceNpBasicGetEvent`@0xa7ef4c, `sceNpBasicGetFriendListEntry`@0xa7eb0c, `sceNpBasicGetFriendListEntryCount`@0xa7ee0c, `sceNpBasicGetFriendPresenceByIndex`@0xa7ebcc, `sceNpBasicGetPlayersHistoryEntry`@0xa7ee4c, `sceNpBasicGetPlayersHistoryEntryCount`@0xa7ed8c, `sceNpBasicRecvMessageAttachmentLoad`@0xa7ec8c, `sceNpBasicRecvMessageCustom`@0xa7ed2c, `sceNpBasicRegisterHandler`@0xa7ee6c, `sceNpBasicSendMessageGui`@0xa7eaec, `sceNpBasicSetPresenceDetails`@0xa7eecc, `sceNpBasicUnregisterHandler`@0xa7edec, `sceNpFriendlist`@0xa7efcc, `sceNpLookupCreateTitleCtx`@0xa7ef0c, `sceNpLookupCreateTransactionCtx`@0xa7ef8c, `sceNpLookupDestroyTitleCtx`@0xa7ec4c, `sceNpLookupDestroyTransactionCtx`@0xa7f04c, `sceNpLookupInit`@0xa7ec6c, `sceNpLookupPollAsync`@0xa7ed0c, `sceNpLookupTerm`@0xa7ed4c, `sceNpLookupUserProfileAsync`@0xa7f08c, `sceNpManagerGetChatRestrictionFlag`@0xa7efac, `sceNpManagerGetContentRatingFlag`@0xa7ecac, `sceNpManagerGetNpId`@0xa7f06c, `sceNpManagerGetOnlineId`@0xa7eeac, `sceNpManagerGetStatus`@0xa7edcc, `sceNpManagerRegisterCallback`@0xa7ef6c, `sceNpManagerUnregisterCallback`@0xa7ec2c, `sceNpScoreCreateTitleCtx`@0xa7ee2c, `sceNpScoreCreateTransactionCtx`@0xa7eccc, `sceNpScoreDestroyTitleCtx`@0xa7eb8c, `sceNpScoreDestroyTransactionCtx`@0xa7eeec, `sceNpScoreGetRankingByNpIdAsync`@0xa7ec0c, `sceNpScoreGetRankingByRangeAsync`@0xa7eb6c, `sceNpScoreInit`@0xa7ebec, `sceNpScorePollAsync`@0xa7edac, `sceNpScoreRecordGameDataAsync`@0xa7f02c, `sceNpScoreRecordScoreAsync`@0xa7efec, `sceNpScoreTerm`@0xa7ed6c, `sceNpUtilCmpNpId`@0xa7ef2c

## cellSpurs (30) — IMPLEMENTAR/STUB (tareas SPU: audio, zlib) -> hilos en PC

`_cellSpursAttributeInitialize`@0xa7e70c, `_cellSpursEventFlagInitialize`@0xa7e62c, `_cellSpursJobChainAttributeInitialize`@0xa7e5ac, `_cellSpursLFQueueInitialize`@0xa7e4ec, `_cellSpursLFQueuePushBody`@0xa7e6ec, `_cellSpursSendSignal`@0xa7e86c, `_cellSpursTasksetAttributeInitialize`@0xa7e52c, `cellSpursAttachLv2EventQueue`@0xa7e7cc, `cellSpursAttributeSetNamePrefix`@0xa7e50c, `cellSpursCreateJobChainWithAttribute`@0xa7e58c, `cellSpursCreateTask`@0xa7e7ec, `cellSpursCreateTaskset`@0xa7e60c, `cellSpursCreateTasksetWithAttribute`@0xa7e80c, `cellSpursDetachLv2EventQueue`@0xa7e5ec, `cellSpursEventFlagAttachLv2EventQueue`@0xa7e6cc, `cellSpursEventFlagDetachLv2EventQueue`@0xa7e56c, `cellSpursEventFlagWait`@0xa7e5cc, `cellSpursFinalize`@0xa7e82c, `cellSpursInitializeWithAttribute`@0xa7e7ac, `cellSpursJobChainAttributeSetName`@0xa7e74c, `cellSpursJobGuardInitialize`@0xa7e66c, `cellSpursJobGuardNotify`@0xa7e84c, `cellSpursJoinJobChain`@0xa7e78c, `cellSpursJoinTaskset`@0xa7e72c, `cellSpursLFQueueAttachLv2EventQueue`@0xa7e54c, `cellSpursLFQueueDetachLv2EventQueue`@0xa7e6ac, `cellSpursRunJobChain`@0xa7e88c, `cellSpursShutdownJobChain`@0xa7e68c, `cellSpursShutdownTaskset`@0xa7e76c, `cellSpursTasksetAttributeSetName`@0xa7e64c

## sceNp2 (29) — STUB (PSN)

`sceNp2Init`@0xa7f24c, `sceNp2Term`@0xa7f38c, `sceNpMatching2AbortRequest`@0xa7f1ec, `sceNpMatching2ClearEventData`@0xa7f36c, `sceNpMatching2ContextStart`@0xa7f16c, `sceNpMatching2ContextStop`@0xa7f3ec, `sceNpMatching2CreateContext`@0xa7f32c, `sceNpMatching2CreateJoinRoom`@0xa7f3cc, `sceNpMatching2DestroyContext`@0xa7f0ac, `sceNpMatching2GetEventData`@0xa7f22c, `sceNpMatching2GetServerIdListLocal`@0xa7f30c, `sceNpMatching2GetServerInfo`@0xa7f20c, `sceNpMatching2GetWorldInfoList`@0xa7f0ec, `sceNpMatching2Init2`@0xa7f40c, `sceNpMatching2JoinRoom`@0xa7f26c, `sceNpMatching2KickoutRoomMember`@0xa7f28c, `sceNpMatching2LeaveRoom`@0xa7f18c, `sceNpMatching2RegisterContextCallback`@0xa7f2ac, `sceNpMatching2RegisterLobbyEventCallback`@0xa7f10c, `sceNpMatching2RegisterLobbyMessageCallback`@0xa7f0cc, `sceNpMatching2RegisterRoomEventCallback`@0xa7f3ac, `sceNpMatching2RegisterRoomMessageCallback`@0xa7f42c, `sceNpMatching2RegisterSignalingCallback`@0xa7f34c, `sceNpMatching2SearchRoom`@0xa7f12c, `sceNpMatching2SetRoomDataExternal`@0xa7f1ac, `sceNpMatching2SetRoomDataInternal`@0xa7f2cc, `sceNpMatching2SignalingGetConnectionStatus`@0xa7f14c, `sceNpMatching2SignalingGetPingInfo`@0xa7f2ec, `sceNpMatching2Term2`@0xa7f1cc

## cellGcmSys (19) — SUSTITUIR (graficos RSX) -> renderizador del port

`_cellGcmFunc15`@0xa7e06c, `_cellGcmInitBody`@0xa7e00c, `_cellGcmSetFlipCommand`@0xa7e02c, `cellGcmAddressToOffset`@0xa7e04c, `cellGcmBindTile`@0xa7e08c, `cellGcmBindZcull`@0xa7e10c, `cellGcmGetConfiguration`@0xa7e1ec, `cellGcmGetControlRegister`@0xa7e16c, `cellGcmGetLabelAddress`@0xa7e20c, `cellGcmGetReport`@0xa7e0ec, `cellGcmGetTiledPitchSize`@0xa7dfcc, `cellGcmGetTimeStamp`@0xa7e0cc, `cellGcmSetDisplayBuffer`@0xa7e14c, `cellGcmSetFlipHandler`@0xa7e12c, `cellGcmSetFlipMode`@0xa7e0ac, `cellGcmSetTile`@0xa7e1cc, `cellGcmSetTileInfo`@0xa7e1ac, `cellGcmSetUserHandler`@0xa7dfec, `cellGcmSetVBlankHandler`@0xa7e18c

## cellSysutil (19) — PARCIAL (callbacks del sistema, idioma, dialogos) -> stub con valores fijos

`cellAudioOutConfigure`@0xa7dcac, `cellAudioOutGetSoundAvailability`@0xa7dd6c, `cellAudioOutGetState`@0xa7e26c, `cellMsgDialogOpenErrorCode`@0xa7dc6c, `cellOskDialogAbort`@0xa7dd4c, `cellOskDialogGetInputText`@0xa7dc2c, `cellOskDialogLoadAsync`@0xa7dccc, `cellOskDialogSetDeviceMask`@0xa7dd8c, `cellOskDialogUnloadAsync`@0xa7dc4c, `cellSaveDataAutoLoad2`@0xa7ddcc, `cellSaveDataAutoSave2`@0xa7dd0c, `cellSysutilCheckCallback`@0xa7dc0c, `cellSysutilGetSystemParamInt`@0xa7dc8c, `cellSysutilRegisterCallback`@0xa7dd2c, `cellSysutilUnregisterCallback`@0xa7dbcc, `cellVideoOutConfigure`@0xa7dbec, `cellVideoOutGetConfiguration`@0x125ad2c, `cellVideoOutGetResolution`@0xa7ddac, `cellVideoOutGetState`@0xa7dcec

## sys_io (19) — IMPLEMENTAR (mando cellPad) -> SDL3 gamepad

`cellKbClearBuf`@0xa7d88c, `cellKbEnd`@0xa7d9cc, `cellKbGetInfo`@0xa7d8ac, `cellKbInit`@0xa7d90c, `cellKbRead`@0xa7da2c, `cellKbSetCodeType`@0xa7d98c, `cellKbSetLEDStatus`@0xa7d8ec, `cellKbSetReadMode`@0xa7d9ec, `cellPadEnd`@0xa7d92c, `cellPadGetData`@0xa7d96c, `cellPadGetInfo`@0xa7d8cc, `cellPadGetInfo2`@0xa7d9ac, `cellPadInit`@0xa7d86c, `cellPadLddDataInsert`@0xa7df8c, `cellPadLddGetPortNo`@0xa7de0c, `cellPadLddRegisterController`@0xa7df6c, `cellPadLddUnregisterController`@0xa7dfac, `cellPadSetActDirect`@0xa7da0c, `cellPadSetPortSetting`@0xa7d94c

## cellSysutilAvc2 (17) — STUB (chat de voz)

`cellSysutilAvc2GetPlayerInfo`@0xa7f44c, `cellSysutilAvc2GetPlayerVoiceMuting`@0xa7f58c, `cellSysutilAvc2GetVoiceMuting`@0xa7f64c, `cellSysutilAvc2InitParam`@0xa7f5cc, `cellSysutilAvc2JoinChat`@0xa7f46c, `cellSysutilAvc2LeaveChat`@0xa7f5ec, `cellSysutilAvc2Load`@0xa7f60c, `cellSysutilAvc2SetAttribute`@0xa7f62c, `cellSysutilAvc2SetPlayerVoiceMuting`@0xa7f52c, `cellSysutilAvc2SetSpeakerVolumeLevel`@0xa7f50c, `cellSysutilAvc2SetStreamingTarget`@0xa7f54c, `cellSysutilAvc2SetVoiceMuting`@0xa7f4ac, `cellSysutilAvc2StartStreaming`@0xa7f5ac, `cellSysutilAvc2StartVoiceDetection`@0xa7f4cc, `cellSysutilAvc2StopStreaming`@0xa7f48c, `cellSysutilAvc2StopVoiceDetection`@0xa7f4ec, `cellSysutilAvc2Unload`@0xa7f56c

## sys_fs (13) — IMPLEMENTAR (archivos) -> lectura de NPEB00060/USRDIR

`cellFsClose`@0xa7e28c, `cellFsClosedir`@0xa7e40c, `cellFsFstat`@0xa7e3ec, `cellFsGetDirectoryEntries`@0xa7e36c, `cellFsLseek`@0xa7e38c, `cellFsMkdir`@0xa7e3ac, `cellFsOpen`@0xa7e30c, `cellFsOpendir`@0xa7e2ac, `cellFsRead`@0xa7e2cc, `cellFsReaddir`@0xa7e2ec, `cellFsStat`@0xa7e32c, `cellFsUnlink`@0xa7e34c, `cellFsWrite`@0xa7e3cc

## cellUsbd (12) — STUB (USB)

`cellUsbdClosePipe`@0xa7deec, `cellUsbdControlTransfer`@0xa7df0c, `cellUsbdEnd`@0xa7da4c, `cellUsbdGetDeviceLocation`@0xa7df4c, `cellUsbdGetPrivateData`@0xa7de8c, `cellUsbdInit`@0xa7da6c, `cellUsbdInterruptTransfer`@0xa7de2c, `cellUsbdOpenPipe`@0xa7de4c, `cellUsbdRegisterExtraLdd2`@0xa7df2c, `cellUsbdScanStaticDescriptor`@0xa7de6c, `cellUsbdSetPrivateData`@0xa7deac, `cellUsbdUnregisterExtraLdd`@0xa7decc

## cellCamera (10) — STUB (EyeToy/camara)

`cellCameraClose`@0xa7daac, `cellCameraEnd`@0xa7db2c, `cellCameraGetType`@0xa7db0c, `cellCameraInit`@0xa7dbac, `cellCameraIsAttached`@0xa7db6c, `cellCameraOpenEx`@0xa7db4c, `cellCameraRead`@0xa7dacc, `cellCameraReset`@0xa7db8c, `cellCameraStart`@0xa7daec, `cellCameraStop`@0xa7da8c

## sceNpTrophy (10) — STUB o trofeos locales

`sceNpTrophyCreateContext`@0xa7f76c, `sceNpTrophyCreateHandle`@0xa7f68c, `sceNpTrophyDestroyContext`@0xa7f6ac, `sceNpTrophyDestroyHandle`@0xa7f6ec, `sceNpTrophyGetTrophyInfo`@0xa7f78c, `sceNpTrophyGetTrophyUnlockState`@0xa7f74c, `sceNpTrophyInit`@0xa7f6cc, `sceNpTrophyRegisterContext`@0xa7f66c, `sceNpTrophyTerm`@0xa7f72c, `sceNpTrophyUnlockTrophy`@0xa7f70c

## sys_net (10) — STUB (red)

`_sys_net_errno_loc`@0xa7e9ec, `bind`@0xa7ea8c, `inet_addr`@0xa7eacc, `recvfrom`@0xa7e9cc, `sendto`@0xa7ea4c, `setsockopt`@0xa7ea2c, `socket`@0xa7ea6c, `socketclose`@0xa7ea0c, `sys_net_finalize_network`@0xa7eaac, `sys_net_initialize_network_ex`@0xa7e9ac

## cellAudio (6) — IMPLEMENTAR (salida de audio) -> SDL3 audio

`cellAudioGetPortConfig`@0xa7e46c, `cellAudioInit`@0xa7e42c, `cellAudioPortClose`@0xa7e44c, `cellAudioPortOpen`@0xa7e4cc, `cellAudioPortStart`@0xa7e48c, `cellAudioQuit`@0xa7e4ac

## cellNetCtl (5) — STUB (red)

`cellNetCtlGetInfo`@0xa7e96c, `cellNetCtlInit`@0xa7e98c, `cellNetCtlNetStartDialogLoadAsync`@0xa7e90c, `cellNetCtlNetStartDialogUnloadAsync`@0xa7e92c, `cellNetCtlTerm`@0xa7e94c

## cellRtc (3) — IMPLEMENTAR (reloj)

`cellRtcGetCurrentClockLocalTime`@0xa7d80c, `cellRtcGetDayOfWeek`@0xa7d84c, `cellRtcSetTick`@0xa7d82c

## cellSync (3) — IMPLEMENTAR (sincronizacion)

`cellSyncMutexInitialize`@0xa7e8ec, `cellSyncMutexLock`@0xa7e8ac, `cellSyncMutexUnlock`@0xa7e8cc

## cellGameExec (2) — STUB

`cellGameGetBootGameInfo`@0xa7f7cc, `cellGameGetHomeLaunchOptionPath`@0xa7f7ac

## cellLibprof (2) — STUB

`cellUserTraceRegister`@0x128d718, `cellUserTraceUnregister`@0x128d738

## cellSysmodule (2) — STUB (carga de modulos: devolver OK)

`cellSysmoduleLoadModule`@0xa7e24c, `cellSysmoduleUnloadModule`@0xa7e22c

## cellSaveData (1) — IMPLEMENTAR (partidas guardadas) -> carpeta del usuario

`cellSaveDataListDelete`@0xa7ddec


## Llamadas directas al núcleo lv2 (`sc`)
Hay 3028 instrucciones `sc` con unos 60 números distintos. Por grupos:

- **43-48:** hilos PPU (yield, join…).
- **70-76:** grupos SPU.
- **90-94:** semáforos.
- **100-104:** mutex.
- **114:** sys_mutex.
- **128-138:** colas y puertos de eventos.
- **141-147:** timers y sleep.
- **169-192:** SPU.
- **341/342/352:** memoria.
- **402/403:** consola tty.
- **801-834:** sistema de archivos.

En el código traducido, cada `sc` llama a `rc_syscall(c)`. El número de la llamada va en `r11`, los argumentos en `r3`…`r10` y el resultado en `r3`.
